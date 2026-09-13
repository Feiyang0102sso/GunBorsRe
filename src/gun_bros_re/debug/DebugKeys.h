#pragma once
#include "gun_bros_re/debug/DebugConfig.h"
#include "gun_bros_re/debug/CheatCodes.h"
namespace GameCheats {
inline void Bind() {
#if GB_ENABLE_CHEATS
    CWindow::SetCommandMatcher(Consume);
#endif
}
}

namespace GameDebugKeys {
inline bool StartsTutorial(KeyCode key, const CWindow &window) {
    return key == DebugConfig::Keys::Tutorial && window.WasShiftPressed();
}
// In-game collision overlay; one action per key press.
inline constexpr KeyCode ToggleCollision = DebugConfig::Keys::Collision;
inline constexpr KeyCode ToggleInfo = DebugConfig::Keys::Info;
inline bool TogglesCollision(KeyCode key, const CWindow &window) {
    return key == ToggleCollision && (!DebugConfig::Keys::CollisionShift || window.WasShiftPressed());
}
inline bool TogglesInfo(KeyCode key, const CWindow &window) {
    return key == ToggleInfo && (!DebugConfig::Keys::InfoShift || window.WasShiftPressed());
}
// Desktop map research controls. Shift is checked at key-down, not at polling time.
inline constexpr KeyCode MapBrowser = DebugConfig::Keys::MapBrowser;
inline constexpr KeyCode MapPrevious = DebugConfig::Keys::Previous;
inline constexpr KeyCode MapNext = DebugConfig::Keys::Next;
inline constexpr KeyCode MapPreviousPage = DebugConfig::Keys::PreviousPage;
inline constexpr KeyCode MapNextPage = DebugConfig::Keys::NextPage;
inline constexpr KeyCode MapLoad = DebugConfig::Keys::Load;
inline constexpr KeyCode MapBack = DebugConfig::Keys::Back;
inline bool OpensMapBrowser(KeyCode key, const CWindow &window) {
    return key == MapBrowser && (!DebugConfig::Keys::MapBrowserShift || window.WasShiftPressed());
}
}
