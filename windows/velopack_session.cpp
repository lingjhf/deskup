#include "velopack_session.h"
#include <windows.h>
#include <Velopack.h>
#include <filesystem>
#include "update_config.h"
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

VelopackSession::VelopackSession() : impl_(std::make_unique<Impl>()) {}
VelopackSession::~VelopackSession() = default;
bool VelopackSession::configured() const { return impl_->configured(); }
UpdateResult VelopackSession::Initialize() { return impl_->Initialize(); }
UpdateResult VelopackSession::Check() { return impl_->Check(); }
UpdateResult VelopackSession::Download(void (*progress)(void*, size_t), void* data) {
  return impl_->Download(progress, data);
}
UpdateResult VelopackSession::Install() { return impl_->Install(); }
}  // namespace deskup
