#include "velopack_session.h"
#include <windows.h>
#include <Velopack.h>
#include <filesystem>
#include "update_environment.h"

namespace deskup {
namespace {
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

template <typename T, void (*Release)(T*)>
struct SdkDeleter {
  void operator()(T* value) const { Release(value); }
};

template <typename T, void (*Release)(T*)>
using SdkPointer = std::unique_ptr<T, SdkDeleter<T, Release>>;

}  // namespace
// Only the current worker accesses the SDK. Reverse member destruction frees
// releases before the manager, and the manager before its source.
struct VelopackSession::Impl {
 public:
  bool configured() const { return manager_ != nullptr; }

  UpdateResult Initialize(const WindowsConfiguration& config) {
    config_ = config;
    if (!config.enabled || !HasPackageManifest()) return {};
    const auto& url = config.url;
    vpkc_http_options_t http{};
    http.TimeoutMilliseconds = config.timeout_ms;
    if (config.gitea) {
      source_.reset(vpkc_new_source_gitea(
          url.c_str(), nullptr, config.prereleases));
    } else {
      source_.reset(vpkc_new_source_http_url_with_options(url.c_str(), &http));
    }
    vpkc_update_options_t options{};
    options.MaximumDeltasBeforeFallback = config.maximum_deltas;
    options.AllowVersionDowngrade = config.allow_downgrade;
    options.ExplicitChannel = config.channel.empty() ? nullptr : const_cast<char*>(config.channel.c_str());
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
    installed_id_ = id;
    if (!config.expected_id.empty() && installed_id_ != config.expected_id) {
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
      } else if (std::string(update_->TargetFullRelease->PackageId) != installed_id_) {
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
    if (result.succeeded) WritePreference(preference_key(), L"LastCheck", Now());
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

  UpdateResult Install(const InstallOptions& options) {
    auto result = WithPending({});
    if (!pending_) return UpdateResult::Failure("Downloaded update is missing");
    std::vector<char*> arguments;
    for (const auto& argument : options.arguments) arguments.push_back(const_cast<char*>(argument.c_str()));
    result.succeeded = vpkc_wait_exit_then_apply_updates(
        manager_.get(), pending_.get(), options.silent, options.restart,
        arguments.empty() ? nullptr : arguments.data(), arguments.size());
    if (!result.succeeded) result.error = LastError();
    return result;
  }

  std::wstring preference_key() const {
    const auto key = "Software\\" + config_.preference_namespace + "\\" + installed_id_ + "\\Updates";
    return std::wstring(key.begin(), key.end());
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
    const auto* asset = pending_ ? pending_.get() :
        (update_ ? update_->TargetFullRelease : nullptr);
    if (asset) {
      result.notes_markdown = asset->NotesMarkdown ? asset->NotesMarkdown : "";
      result.notes_html = asset->NotesHtml ? asset->NotesHtml : "";
      result.package_size = static_cast<int64_t>(asset->Size);
    }
    return result;
  }

  SdkPointer<vpkc_update_source_t, vpkc_free_source> source_;
  SdkPointer<vpkc_update_manager_t, vpkc_free_update_manager> manager_;
  SdkPointer<vpkc_update_info_t, vpkc_free_update_info> update_;
  SdkPointer<vpkc_asset_t, vpkc_free_asset> pending_;
  WindowsConfiguration config_;
  std::string installed_id_;
};

VelopackSession::VelopackSession() : impl_(std::make_unique<Impl>()) {}
VelopackSession::~VelopackSession() = default;
bool VelopackSession::configured() const { return impl_->configured(); }
UpdateResult VelopackSession::Initialize(const WindowsConfiguration& config) { return impl_->Initialize(config); }
std::wstring VelopackSession::preference_key() const { return impl_->preference_key(); }
UpdateResult VelopackSession::Check() { return impl_->Check(); }
UpdateResult VelopackSession::Download(void (*progress)(void*, size_t), void* data) {
  return impl_->Download(progress, data);
}
UpdateResult VelopackSession::Install(const InstallOptions& options) { return impl_->Install(options); }
}  // namespace deskup
