#include "runtime_configuration.h"
#include <regex>
#include <stdexcept>

namespace deskup {
namespace {
using Value = flutter::EncodableValue;
using Map = flutter::EncodableMap;
const Map& AsMap(const Value* value) {
  const auto* map = value ? std::get_if<Map>(value) : nullptr;
  if (!map) throw std::invalid_argument("Expected a configuration map");
  return *map;
}
const Value* Field(const Map& map, const char* name) {
  const auto found = map.find(Value(name));
  return found == map.end() ? nullptr : &found->second;
}
std::string Text(const Map& map, const char* name, bool optional = false) {
  const auto* value = Field(map, name);
  if (optional && (!value || std::holds_alternative<std::monostate>(*value))) return {};
  const auto* text = value ? std::get_if<std::string>(value) : nullptr;
  if (!text || text->find('\0') != std::string::npos) throw std::invalid_argument(name);
  return *text;
}
bool Boolean(const Map& map, const char* name) {
  const auto* value = Field(map, name);
  const auto* flag = value ? std::get_if<bool>(value) : nullptr;
  if (!flag) throw std::invalid_argument(name);
  return *flag;
}
int32_t Number(const Map& map, const char* name, int32_t low, int32_t high) {
  const auto* value = Field(map, name);
  if (!value) throw std::invalid_argument(name);
  int64_t number;
  if (const auto* small = std::get_if<int32_t>(value)) number = *small;
  else if (const auto* large = std::get_if<int64_t>(value)) number = *large;
  else throw std::invalid_argument(name);
  if (number < low || number > high) throw std::invalid_argument(name);
  return static_cast<int32_t>(number);
}
bool Identifier(const std::string& value) {
  return std::regex_match(value, std::regex("[A-Za-z0-9][A-Za-z0-9_.-]*"));
}
}
WindowsConfiguration ParseConfiguration(const Value* value) {
  const auto& root = AsMap(value);
  const auto* windows = Field(root, "windows");
  WindowsConfiguration config;
  if (!windows || std::holds_alternative<std::monostate>(*windows)) return config;
  const auto& map = AsMap(windows);
  const auto& source = AsMap(Field(map, "source"));
  const auto type = Text(source, "type");
  if (type != "web" && type != "gitea") throw std::invalid_argument("Unknown update source");
  config.gitea = type == "gitea";
  config.url = Text(source, "url");
  const bool local = Boolean(source, "allowLocalHttp");
  const bool https = std::regex_match(config.url, std::regex("https://[A-Za-z0-9.-]+(:[0-9]+)?(/[A-Za-z0-9_./%~-]*)?"));
  const bool loopback = local && std::regex_match(config.url, std::regex("http://(localhost|127\\.0\\.0\\.1):[0-9]+(/[A-Za-z0-9_./%~-]*)?"));
  if (!https && !loopback) throw std::invalid_argument("Use HTTPS or explicitly enabled loopback HTTP");
  std::smatch port;
  if (std::regex_match(config.url, port, std::regex("https?://[^/:]+:([0-9]+)(/.*)?"))) {
    if (port[1].length() > 5 || std::stoi(port[1].str()) < 1 || std::stoi(port[1].str()) > 65535) {
      throw std::invalid_argument("Invalid port");
    }
  }
  if (config.gitea) {
    if (!std::regex_match(config.url, std::regex("https?://[^/]+/[A-Za-z0-9_.~-]+/[A-Za-z0-9_.~-]+/?")) ||
        std::regex_search(config.url, std::regex("\\.git/?$"))) throw std::invalid_argument("Invalid Gitea repository URL");
    config.prereleases = Boolean(source, "includePrereleases");
  } else config.timeout_ms = Number(source, "timeoutMilliseconds", 1, 3600000);
  config.expected_id = Text(map, "expectedPackageId", true);
  config.channel = Text(map, "channel", true);
  for (const auto* name : {"expectedPackageId", "channel"}) {
    const auto* field = Field(map, name);
    if (field && !std::holds_alternative<std::monostate>(*field) && Text(map, name).empty()) {
      throw std::invalid_argument(name);
    }
  }
  config.preference_namespace = Text(map, "preferenceNamespace");
  if ((!config.expected_id.empty() && !Identifier(config.expected_id)) ||
      (!config.channel.empty() && !Identifier(config.channel)) || !Identifier(config.preference_namespace)) {
    throw std::invalid_argument("Invalid identity, channel or preference namespace");
  }
  config.interval_seconds = Number(map, "automaticCheckIntervalSeconds", 60, 2592000);
  config.maximum_deltas = Number(map, "maximumDeltas", -1, 100);
  config.allow_downgrade = Boolean(map, "allowDowngrade");
  config.enabled = true;
  return config;
}
InstallOptions ParseInstallOptions(const Value* value) {
  const auto& map = AsMap(value);
  InstallOptions options;
  options.silent = Boolean(map, "silent");
  options.restart = Boolean(map, "restart");
  const auto* arguments = Field(map, "restartArguments");
  const auto* list = arguments ? std::get_if<flutter::EncodableList>(arguments) : nullptr;
  if (!list) throw std::invalid_argument("Expected restart arguments");
  for (const auto& item : *list) {
    const auto* text = std::get_if<std::string>(&item);
    if (!text || text->find('\0') != std::string::npos) throw std::invalid_argument("Invalid restart argument");
    options.arguments.push_back(*text);
  }
  if (!options.restart && !options.arguments.empty()) throw std::invalid_argument("Arguments require restart");
  return options;
}
}  // namespace deskup
