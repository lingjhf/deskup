#include "runtime_configuration.h"
#include <stdexcept>
#include <iostream>
using flutter::EncodableValue;
using flutter::EncodableMap;
using flutter::EncodableList;
using V = EncodableValue;
using M = EncodableMap;
void Require(bool condition) { if (!condition) throw std::runtime_error("Assertion failed"); }
template <typename Action> void Reject(Action action) {
  try { action(); } catch (const std::invalid_argument&) { return; }
  throw std::runtime_error("Invalid input was accepted");
}
int main() {
  try {
    M source{{V("type"), V("web")}, {V("url"), V("https://updates.example.com/x64")},
      {V("allowLocalHttp"), V(false)}, {V("timeoutMilliseconds"), V(45000)}};
    M policy{{V("source"), V(source)}, {V("expectedPackageId"), V("App")}, {V("channel"), V("beta")},
      {V("preferenceNamespace"), V("App")}, {V("automaticCheckIntervalSeconds"), V(300)},
      {V("maximumDeltas"), V(-1)}, {V("allowDowngrade"), V(true)}};
    V root(M{{V("windows"), V(policy)}});
    auto config = deskup::ParseConfiguration(&root);
    Require(config.enabled && config.timeout_ms == 45000 && config.allow_downgrade && config.channel == "beta" && config.maximum_deltas == -1);
    V disabled(M{{V("windows"), V()}});
    Require(!deskup::ParseConfiguration(&disabled).enabled);
    for (auto url : {"http://updates.example.com", "https://user:pass@host/app", "https://host/app?token=a", "https://host:999999/app", "https://host:0/app"}) {
      source[V("url")] = V(url); policy[V("source")] = V(source); root = V(M{{V("windows"), V(policy)}});
      Reject([&] { deskup::ParseConfiguration(&root); });
    }
    source[V("url")] = V("http://127.0.0.1:8765/team/app"); source[V("allowLocalHttp")] = V(true);
    source[V("type")] = V("gitea"); source[V("includePrereleases")] = V(true);
    policy[V("source")] = V(source); root = V(M{{V("windows"), V(policy)}});
    config = deskup::ParseConfiguration(&root);
    Require(config.gitea && config.prereleases && config.interval_seconds == 300);
    policy[V("automaticCheckIntervalSeconds")] = V(int64_t(1) << 40); root = V(M{{V("windows"), V(policy)}});
    Reject([&] { deskup::ParseConfiguration(&root); });
    V install(M{{V("silent"), V(false)}, {V("restart"), V(true)},
      {V("restartArguments"), V(EncodableList{V("--project"), V("document with spaces")})}});
    auto options = deskup::ParseInstallOptions(&install);
    Require(!options.silent && options.restart && options.arguments.size() == 2);
    auto map = std::get<M>(install); map[V("restart")] = V(false); install = V(map);
    Reject([&] { deskup::ParseInstallOptions(&install); });
    map[V("restartArguments")] = V(EncodableList{}); install = V(map);
    Require(!deskup::ParseInstallOptions(&install).restart);
    map[V("restartArguments")] = V(EncodableList{V(42)}); install = V(map);
    Reject([&] { deskup::ParseInstallOptions(&install); });
    std::cout << "Native runtime configuration and install options passed\n";
  } catch (const std::exception& error) { std::cerr << error.what(); return 1; }
}
