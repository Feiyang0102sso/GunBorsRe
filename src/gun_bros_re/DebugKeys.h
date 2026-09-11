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
// Desktop map research controls. Shift is checked at key-down, not at polling time.
inline constexpr KeyCode MapBrowser = KeyCode::F3;
inline constexpr KeyCode MapPrevious = KeyCode::Up;
inline constexpr KeyCode MapNext = KeyCode::Down;
inline constexpr KeyCode MapPreviousPage = KeyCode::Left;
inline constexpr KeyCode MapNextPage = KeyCode::Right;
inline constexpr KeyCode MapLoad = KeyCode::Enter;
inline constexpr KeyCode MapBack = KeyCode::Escape;
inline bool OpensMapBrowser(KeyCode key, const CWindow &window) {
    return key == MapBrowser && window.WasShiftPressed();
}
}
#endif
