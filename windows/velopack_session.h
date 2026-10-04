#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace deskup {
enum class Phase { Idle, Checking, Current, Available, Downloading, Ready, Installing };

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

// Accessed by one worker at a time. SDK resources outlive detached workers.
class VelopackSession {
 public:
  VelopackSession();
  ~VelopackSession();
  bool configured() const;
  UpdateResult Initialize();
  UpdateResult Check();
  UpdateResult Download(void (*progress)(void*, size_t), void* data);
  UpdateResult Install();
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace deskup
