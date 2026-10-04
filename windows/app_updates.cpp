#include "app_updates.h"

#include <flutter/standard_method_codec.h>
#include <Velopack.h>
#include <winver.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cwchar>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "update_config.h"

namespace {
using Value = flutter::EncodableValue;
using Map = flutter::EncodableMap;

std::wstring PreferenceKey() {
  const std::string id = DESKUP_UPDATE_PACKAGE_ID;
  return std::wstring(DESKUP_PREFERENCE_ROOT) + std::wstring(id.begin(), id.end()) + L"\\Updates";
}
DWORD ReadPreference(const wchar_t* name) {
  DWORD value = 0;
  DWORD size = sizeof(value);
  RegGetValueW(HKEY_CURRENT_USER, PreferenceKey().c_str(), name, RRF_RT_REG_DWORD,
               nullptr, &value, &size);
  return value;
}
bool WritePreference(const wchar_t* name, DWORD value) {
  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, PreferenceKey().c_str(), 0, nullptr, 0,
                      KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
    return false;
  }
  const auto result = RegSetValueExW(key, name, 0, REG_DWORD,
      reinterpret_cast<const BYTE*>(&value), sizeof(value));
  RegCloseKey(key);
  return result == ERROR_SUCCESS;
}
DWORD Now() {
  return static_cast<DWORD>(std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
}
std::string LastError() {
  const size_t size = vpkc_get_last_error(nullptr, 0);
  std::string error(size + 1, '\0');
  vpkc_get_last_error(error.data(), error.size());
  error.resize(std::char_traits<char>::length(error.c_str()));
  return error.empty() ? "Update operation failed" : error;
}
bool HasPackageManifest() {
  wchar_t executable[32768]{};
  const DWORD count = GetModuleFileNameW(nullptr, executable, 32768);
  if (count == 0 || count == 32768) return false;
  const auto manifest = std::filesystem::path(executable).parent_path() / L"sq.version";
  return GetFileAttributesW(manifest.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::pair<std::string, std::string> ExecutableVersion() {
  wchar_t executable[32768]{};
  if (!GetModuleFileNameW(nullptr, executable, 32768)) return {};
  DWORD unused = 0;
  const auto size = GetFileVersionInfoSizeW(executable, &unused);
  if (!size) return {};
  std::vector<BYTE> data(size);
  if (!GetFileVersionInfoW(executable, 0, size, data.data())) return {};
  struct Translation { WORD language; WORD code_page; };
  Translation* translation = nullptr;
  UINT translation_size = 0;
  if (VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
      reinterpret_cast<void**>(&translation), &translation_size) &&
      translation_size >= sizeof(Translation)) {
    wchar_t key[64]{};
    swprintf_s(key, 64, L"\\StringFileInfo\\%04x%04x\\ProductVersion",
        static_cast<unsigned int>(translation->language),
        static_cast<unsigned int>(translation->code_page));
    wchar_t* text = nullptr;
    UINT text_size = 0;
    if (VerQueryValueW(data.data(), key, reinterpret_cast<void**>(&text), &text_size) &&
        text && text_size > 1) {
      // Flutter's string resource retains build numbers above 65535, whereas
      // VS_FIXEDFILEINFO stores each version component in only sixteen bits.
      const int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
      if (count > 1) {
        std::string product(static_cast<size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text, -1, product.data(), count, nullptr, nullptr);
        product.pop_back();
        const auto build = product.find('+');
        if (build != std::string::npos) return {product.substr(0, build), product.substr(build + 1)};
        return {product, ""};
      }
    }
  }
  VS_FIXEDFILEINFO* info = nullptr;
  UINT length = 0;
  if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length) ||
      length < sizeof(VS_FIXEDFILEINFO)) return {};
  return {std::to_string(HIWORD(info->dwProductVersionMS)) + "." +
      std::to_string(LOWORD(info->dwProductVersionMS)) + "." +
      std::to_string(HIWORD(info->dwProductVersionLS)),
      std::to_string(LOWORD(info->dwProductVersionLS))};
}

enum class Phase { Idle, Checking, Current, Available, Downloading, Ready, Installing };

const char* PhaseName(Phase phase) {
  switch (phase) {
    case Phase::Idle: return "idle";
    case Phase::Checking: return "checking";
    case Phase::Current: return "current";
    case Phase::Available: return "available";
    case Phase::Downloading: return "downloading";
    case Phase::Ready: return "ready";
    case Phase::Installing: return "installing";
  }
  return "idle";
}

struct UpdateResult {
  Phase phase = Phase::Idle;
  std::string available;
  std::string error;
  bool succeeded = true;

  static UpdateResult Failure(std::string error) {
    UpdateResult result;
    result.error = std::move(error);
    result.succeeded = false;
    return result;
  }
};

template <typename T, void (*Release)(T*)>
struct SdkDeleter {
  void operator()(T* value) const { Release(value); }
};

template <typename T, void (*Release)(T*)>
using SdkPointer = std::unique_ptr<T, SdkDeleter<T, Release>>;

// Only the current worker accesses the SDK. Reverse member destruction frees
// releases before the manager, and the manager before its source.
class VelopackSession {
 public:
  bool configured() const { return manager_ != nullptr; }

  UpdateResult Initialize() {
    const std::string url = DESKUP_WINDOWS_UPDATE_URL;
    if (url.empty() || !HasPackageManifest()) return {};
    vpkc_http_options_t http{};
    http.TimeoutMilliseconds = 600000;  // Large processing bundles need time.
    source_.reset(vpkc_new_source_http_url_with_options(url.c_str(), &http));
    vpkc_update_options_t options{};
    options.MaximumDeltasBeforeFallback = 10;
    vpkc_update_manager_t* manager = nullptr;
    const bool created = source_ && vpkc_new_update_manager_with_source(
        source_.get(), &options, nullptr, &manager);
    manager_.reset(manager);
    if (!created) {
      const auto error = LastError();
      manager_.reset();
      return UpdateResult::Failure(error);
    }
    char id[256]{};
    vpkc_get_app_id(manager_.get(), id, sizeof(id));
    if (std::string(id) != DESKUP_UPDATE_PACKAGE_ID) {
      manager_.reset();
      return UpdateResult::Failure("Unexpected installed package identity");
    }
    return WithPending({});
  }

  UpdateResult Check() {
    update_.reset();
    vpkc_update_info_t* update = nullptr;
    const auto check = vpkc_check_for_updates(manager_.get(), &update);
    update_.reset(update);
    UpdateResult result;
    if (check == UPDATE_ERROR) {
      result = UpdateResult::Failure(LastError());
    } else if (check == UPDATE_AVAILABLE) {
      if (!update_ || !update_->TargetFullRelease) {
        result = UpdateResult::Failure("Update release metadata is missing");
      } else if (std::string(update_->TargetFullRelease->PackageId) != DESKUP_UPDATE_PACKAGE_ID) {
        result = UpdateResult::Failure("Update package identity does not match this app");
        update_.reset();
      } else {
        result.phase = Phase::Available;
        result.available = update_->TargetFullRelease->Version;
      }
    } else {
      result.phase = Phase::Current;
    }
    result = WithPending(std::move(result));
    if (result.succeeded) WritePreference(L"LastCheck", Now());
    return result;
  }

  UpdateResult Download(vpkc_progress_callback_t progress, void* data) {
    if (!update_ || !update_->TargetFullRelease) {
      return UpdateResult::Failure("No update is available to download");
    }
    UpdateResult result;
    result.succeeded = vpkc_download_updates(manager_.get(), update_.get(), progress, data);
    if (!result.succeeded) result.error = LastError();
    result.phase = Phase::Available;
    result.available = update_->TargetFullRelease->Version;
    result = WithPending(std::move(result));
    if (result.succeeded && result.phase != Phase::Ready) {
      result.succeeded = false;
      result.error = "Downloaded package is not ready to install";
    }
    return result;
  }

  UpdateResult Install() {
    auto result = WithPending({});
    if (!pending_) return UpdateResult::Failure("Downloaded update is missing");
    result.succeeded = vpkc_wait_exit_then_apply_updates(
        manager_.get(), pending_.get(), false, true, nullptr, 0);
    if (!result.succeeded) result.error = LastError();
    return result;
  }

 private:
  UpdateResult WithPending(UpdateResult result) {
    pending_.reset();
    vpkc_asset_t* pending = nullptr;
    vpkc_update_pending_restart(manager_.get(), &pending);
    pending_.reset(pending);
    if (pending_) {
      result.phase = Phase::Ready;
      result.available = pending_->Version;
    }
    return result;
  }

  SdkPointer<vpkc_update_source_t, vpkc_free_source> source_;
  SdkPointer<vpkc_update_manager_t, vpkc_free_update_manager> manager_;
  SdkPointer<vpkc_update_info_t, vpkc_free_update_info> update_;
  SdkPointer<vpkc_asset_t, vpkc_free_asset> pending_;
};
}  // namespace

struct AppUpdates::State {
  explicit State(HWND target) : window(target) {}

  void Notify() {
    if (alive.load()) {
      PostMessage(window, StatusMessage(), reinterpret_cast<WPARAM>(this), 0);
    }
  }

  UpdateResult Run(Operation operation) {
    switch (operation) {
      case Operation::Initialize: return session.Initialize();
      case Operation::Check:
      case Operation::AutomaticCheck: return session.Check();
      case Operation::Install: return session.Install();
      case Operation::Download:
        return session.Download([](void* data, size_t progress) {
          auto* target = static_cast<State*>(data);
          {
            std::lock_guard<std::mutex> lock(target->mutex);
            target->progress = static_cast<int32_t>(std::min<size_t>(100, progress));
          }
          target->Notify();
        }, this);
    }
    return UpdateResult::Failure("Unknown update operation");
  }

  HWND window;
  std::mutex mutex;
  std::atomic<bool> alive{true};
  bool busy = false;
  bool configured = false;
  bool automatic = ReadPreference(L"AutomaticChecks") != 0;
  bool install_succeeded = false;
  Phase phase = Phase::Idle;
  std::string available;
  std::string error;
  int32_t progress = 0;
  VelopackSession session;
};

AppUpdates::AppUpdates(HWND window, flutter::BinaryMessenger* messenger)
    : state_(std::make_shared<State>(window)),
      channel_(std::make_unique<flutter::MethodChannel<Value>>(
          messenger, "deskup", &flutter::StandardMethodCodec::GetInstance())) {
  channel_->SetMethodCallHandler([this](const auto& call, auto result) {
    const auto& method = call.method_name();
    if (method == "status") {
      result->Success(Status());
    } else if (method == "ready") {
      if (!ready_) {
        ready_ = true;
        // Registration precedes SetChildContent in standard Flutter runners.
        // Resolve the host after the view has been attached to its parent.
        state_->window = GetAncestor(state_->window, GA_ROOT);
        SetTimer(state_->window, kTimerId, 3600000, nullptr);
        Start(Operation::Initialize);
      }
      result->Success();
    } else if (method == "automaticChecks") {
      const auto* enabled = call.arguments() ? std::get_if<bool>(call.arguments()) : nullptr;
      if (!enabled || !WritePreference(L"AutomaticChecks", *enabled ? 1 : 0)) {
        result->Error("preferences", "Could not save update preference");
        return;
      }
      {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->automatic = *enabled;
      }
      state_->Notify();
      if (*enabled) Start(Operation::AutomaticCheck);
      result->Success();
    } else if (method == "check" || method == "download" || method == "install" ||
               method == "validateInstall") {
      {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (!state_->configured || state_->busy) {
          result->Error("unavailable", "Updates are unavailable or busy");
          return;
        }
        if ((method == "install" || method == "validateInstall") && state_->phase != Phase::Ready) {
          result->Error("notDownloaded", "Download an update before installing");
          return;
        }
        if (method == "download" && state_->phase != Phase::Available) {
          result->Error("noUpdate", "Check for updates before downloading");
          return;
        }
      }
      if (method == "validateInstall") {
        result->Success();
      } else if (method == "install") {
        install_result_ = std::move(result);
        Start(Operation::Install);
      } else {
        Start(method == "check" ? Operation::Check : Operation::Download);
        result->Success();
      }
    } else {
      result->NotImplemented();
    }
  });
}

AppUpdates::~AppUpdates() {
  state_->alive.store(false);
  KillTimer(state_->window, kTimerId);
  channel_->SetMethodCallHandler(nullptr);
  // The worker owns the session until completion, so closing the window neither
  // blocks on a network request nor frees an in-use SDK manager.
}

Value AppUpdates::Status() {
  std::lock_guard<std::mutex> lock(state_->mutex);
  const auto version = ExecutableVersion();
  return Value(Map{
      {Value("supportsDownload"), Value(true)},
      {Value("supportsInstall"), Value(true)},
      {Value("configured"), Value(state_->configured)},
      {Value("canCheck"), Value(state_->configured && !state_->busy)},
      {Value("automaticChecks"), Value(state_->automatic)},
      {Value("version"), Value(version.first)},
      {Value("build"), Value(version.second)},
      {Value("phase"), Value(PhaseName(state_->phase))},
      {Value("availableVersion"), Value(state_->available)},
      {Value("progress"), Value(state_->progress)},
      {Value("error"), Value(state_->error)},
  });
}

void AppUpdates::Start(Operation operation) {
  const auto state = state_;
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->busy) return;
    if (operation == Operation::AutomaticCheck) {
      const DWORD last = ReadPreference(L"LastCheck");
      if (!state->automatic || !state->configured || state->phase == Phase::Ready ||
          (Now() >= last && Now() - last < 86400)) return;
    }
    state->busy = true;
    if (operation == Operation::Download) state->progress = 0;
    state->error.clear();
    state->phase = operation == Operation::Download ? Phase::Downloading :
        operation == Operation::Install ? Phase::Installing : Phase::Checking;
  }
  state->Notify();
  std::thread([state, operation]() {
    auto result = state->Run(operation);
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      state->configured = state->session.configured();
      state->busy = false;
      state->phase = result.phase;
      state->available = std::move(result.available);
      state->error = std::move(result.error);
      state->install_succeeded = operation == Operation::Install && result.succeeded;
    }
    state->Notify();
  }).detach();
}

bool AppUpdates::HandleMessage(UINT message, WPARAM wparam) {
  if (message == WM_TIMER && wparam == kTimerId) {
    Start(Operation::AutomaticCheck);
    return true;
  }
  if (message != StatusMessage() || wparam != reinterpret_cast<WPARAM>(state_.get())) return false;
  channel_->InvokeMethod("statusChanged", std::make_unique<Value>(Status()));
  bool busy;
  bool succeeded;
  std::string error;
  {
    std::lock_guard<std::mutex> lock(state_->mutex);
    busy = state_->busy;
    succeeded = state_->install_succeeded;
    error = state_->error;
  }
  if (install_result_ && !busy) {
    if (succeeded) {
      install_result_->Success();
      PostMessage(state_->window, WM_CLOSE, 0, 0);
    } else {
      install_result_->Error("installFailed", error);
    }
    install_result_.reset();
  } else if (!busy && error.empty()) {
    Start(Operation::AutomaticCheck);
  }
  return true;
}
