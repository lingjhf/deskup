#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include "runtime_configuration.h"

namespace deskup {
enum class Phase { Idle, Checking, Current, Available, Downloading, Ready, Installing };

struct UpdateResult {
  Phase phase = Phase::Idle;
  std::string available;
  std::string error;
  std::string notes_markdown;
  std::string notes_html;
  int64_t package_size = 0;
  bool succeeded = true;

  static UpdateResult Failure(std::string error) {
    UpdateResult result;
    result.error = std::move(error);
    result.succeeded = false;
    return result;
  }
};

// Accessed by one worker at a time. SDK resources outlive detached workers.
class VelopackSession {
 public:
  VelopackSession();
  ~VelopackSession();
  bool configured() const;
  UpdateResult Initialize(const WindowsConfiguration& config);
  std::wstring preference_key() const;
  UpdateResult Check();
  UpdateResult Download(void (*progress)(void*, size_t), void* data);
  UpdateResult Install(const InstallOptions& options);
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace deskup
