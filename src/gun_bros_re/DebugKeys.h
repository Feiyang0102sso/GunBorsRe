#pragma once
#include "engine/platform/CWindow.h"
#include "gun_bros_re/CheatCodes.h"
namespace GameCheats {
inline void Bind() {
#if GB_ENABLE_CHEATS
    CWindow::SetCommandMatcher(Consume);
#endif
}
}

#if GB_ENABLE_TESTS
namespace GameDebugKeys {
// In-game collision overlay; one action per key press.
inline constexpr KeyCode ToggleCollision = KeyCode::C;
}
#endif
