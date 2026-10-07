#include "include/deskup/deskup_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "deskup_plugin.h"

void DeskupPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  deskup::DeskupPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar), FlutterDesktopPluginRegistrarGetMessenger(registrar));
}
