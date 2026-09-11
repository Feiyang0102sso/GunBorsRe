#pragma once
/** Each view owns its navigation keys; development bindings currently expose only independent cheat recognition. */
#include "engine/platform/CWindow.h"
#include "gun_bros_viewer/CheatCodes.h"
namespace ViewerCheats {
inline void Bind() {
#if GB_ENABLE_CHEATS
    CWindow::SetCommandMatcher(Consume);
#endif
}
}

