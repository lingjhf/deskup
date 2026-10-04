#include "deskup_plugin.h"
#include <optional>
namespace deskup {
void DeskupPlugin::RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar) {
  if (!registrar->GetView()) return;
  registrar->AddPlugin(std::make_unique<DeskupPlugin>(registrar));
}
DeskupPlugin::DeskupPlugin(flutter::PluginRegistrarWindows* registrar)
    : registrar_(registrar) {
  const HWND window = GetAncestor(registrar->GetView()->GetNativeWindow(), GA_ROOT);
  updates_ = std::make_unique<AppUpdates>(window, registrar->messenger());
  delegate_id_ = registrar->RegisterTopLevelWindowProcDelegate(
      [this](HWND, UINT message, WPARAM wparam, LPARAM) -> std::optional<LRESULT> {
        if (updates_->HandleMessage(message, wparam)) return 0;
        return std::nullopt;
      });
}
DeskupPlugin::~DeskupPlugin() {
  registrar_->UnregisterTopLevelWindowProcDelegate(delegate_id_);
  updates_.reset();
}
}
