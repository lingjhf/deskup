#include "update_controller.h"
#include <flutter/standard_method_codec.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include "update_environment.h"
#include "velopack_session.h"

namespace deskup {
namespace {
using Value = flutter::EncodableValue;
using Map = flutter::EncodableMap;
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

}  // namespace

struct UpdateController::State {
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

UpdateController::UpdateController(HWND window, flutter::BinaryMessenger* messenger)
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

UpdateController::~UpdateController() {
  state_->alive.store(false);
  KillTimer(state_->window, kTimerId);
  channel_->SetMethodCallHandler(nullptr);
  // The worker owns the session until completion, so closing the window neither
  // blocks on a network request nor frees an in-use SDK manager.
}

Value UpdateController::Status() {
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

void UpdateController::Start(Operation operation) {
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

bool UpdateController::HandleMessage(UINT message, WPARAM wparam) {
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

}  // namespace deskup
