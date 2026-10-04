#pragma once
#include <flutter/plugin_registrar_windows.h>
#include "app_updates.h"
namespace deskup {
class DeskupPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar);
  explicit DeskupPlugin(flutter::PluginRegistrarWindows* registrar);
  ~DeskupPlugin() override;
 private:
  flutter::PluginRegistrarWindows* registrar_;
  int delegate_id_;
  std::unique_ptr<AppUpdates> updates_;
};
}
