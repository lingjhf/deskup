#pragma once
#include <Velopack.h>
// Call at the beginning of wWinMain, before Flutter or COM initialization.
inline void DeskupRunStartup() {
  vpkc_app_set_auto_apply_on_startup(false);
  vpkc_app_run(nullptr);
}
