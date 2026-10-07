#include "deskup_plugin.h"
#include <optional>
namespace deskup {
void DeskupPlugin::RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar, FlutterDesktopMessengerRef core) {
  if (!registrar->GetView()) return;
  registrar->AddPlugin(std::make_unique<DeskupPlugin>(registrar, core));
}
DeskupPlugin::DeskupPlugin(flutter::PluginRegistrarWindows* registrar, FlutterDesktopMessengerRef core)
    : registrar_(registrar), core_(FlutterDesktopMessengerAddRef(core)) {
  const HWND window = GetAncestor(registrar->GetView()->GetNativeWindow(), GA_ROOT);
  updates_ = std::make_unique<UpdateController>(window, registrar->messenger());
  delegate_id_ = registrar->RegisterTopLevelWindowProcDelegate(
      [this](HWND, UINT message, WPARAM wparam, LPARAM) -> std::optional<LRESULT> {
        if (updates_->HandleMessage(message, wparam)) return 0;
        return std::nullopt;
      });
}
DeskupPlugin::~DeskupPlugin() {
  registrar_->UnregisterTopLevelWindowProcDelegate(delegate_id_);
  // Engine shutdown invalidates the core messenger before registrar teardown.
  // Deregister handlers only while the engine still accepts messenger calls.
  updates_->Shutdown(FlutterDesktopMessengerIsAvailable(core_));
  updates_.reset();
  FlutterDesktopMessengerRelease(core_);
}
}
