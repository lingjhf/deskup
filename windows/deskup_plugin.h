#pragma once
#include <flutter/plugin_registrar_windows.h>
#include "update_controller.h"
namespace deskup {
class DeskupPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar, FlutterDesktopMessengerRef core);
  explicit DeskupPlugin(flutter::PluginRegistrarWindows* registrar, FlutterDesktopMessengerRef core);
  ~DeskupPlugin() override;
 private:
  flutter::PluginRegistrarWindows* registrar_;
  FlutterDesktopMessengerRef core_;
  int delegate_id_;
  std::unique_ptr<UpdateController> updates_;
};
}
