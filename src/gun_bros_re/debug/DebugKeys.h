#pragma once
#include "gun_bros_re/debug/DebugConfig.h"
#include "gun_bros_re/cheats/CheatKeys.h"
#include "gun_bros_re/cheats/CheatCodes.h"
#include "gun_bros_re/HostSettings.h"

namespace GameDebugKeys {
inline bool StartsTutorial(KeyCode key, const CWindow &window) {
    return GameHostSettings().debugMode && key == GameCheats::Keys::Tutorial && window.WasShiftPressed();
}
// In-game collision overlay; one action per key press.
inline constexpr KeyCode ToggleCollision = GameCheats::Keys::Collision;
inline constexpr KeyCode ToggleInfo = GameCheats::Keys::Info;
inline bool TogglesCollision(KeyCode key, const CWindow &window) {
    return GameHostSettings().debugMode && key == ToggleCollision && (!GameCheats::Keys::CollisionShift || window.WasShiftPressed());
}
inline bool TogglesInfo(KeyCode key, const CWindow &window) {
    return GameHostSettings().debugMode && key == ToggleInfo && (!GameCheats::Keys::InfoShift || window.WasShiftPressed());
}
// Desktop map research controls. Shift is checked at key-down, not at polling time.
inline constexpr KeyCode MapBrowser = GameCheats::Keys::MapBrowser;
inline constexpr KeyCode MapPrevious = GameCheats::Keys::Previous;
inline constexpr KeyCode MapNext = GameCheats::Keys::Next;
inline constexpr KeyCode MapPreviousPage = GameCheats::Keys::PreviousPage;
inline constexpr KeyCode MapNextPage = GameCheats::Keys::NextPage;
inline constexpr KeyCode MapLoad = GameCheats::Keys::Load;
inline constexpr KeyCode MapBack = GameCheats::Keys::Back;
inline bool OpensMapBrowser(KeyCode key, const CWindow &window) {
    return GameHostSettings().debugMode && key == MapBrowser && (!GameCheats::Keys::MapBrowserShift || window.WasShiftPressed());
}
}
