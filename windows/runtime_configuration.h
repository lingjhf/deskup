#pragma once
#include <flutter/encodable_value.h>
#include <string>
#include <vector>

namespace deskup {
struct WindowsConfiguration {
  bool enabled = false;
  bool gitea = false;
  bool prereleases = false;
  bool allow_downgrade = false;
  std::string url;
  std::string expected_id;
  std::string channel;
  std::string preference_namespace = "Deskup";
  int32_t timeout_ms = 600000;
  int32_t interval_seconds = 86400;
  int32_t maximum_deltas = 10;
};
struct InstallOptions {
  bool silent = true;
  bool restart = true;
  std::vector<std::string> arguments;
};
WindowsConfiguration ParseConfiguration(const flutter::EncodableValue* value);
InstallOptions ParseInstallOptions(const flutter::EncodableValue* value);
}  // namespace deskup
