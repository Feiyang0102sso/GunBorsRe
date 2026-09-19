#pragma once
#include "gun_bros_re/debug/DebugConfig.h"
#include "gun_bros_re/cheats/CheatKeys.h"
#include "gun_bros_re/cheats/CheatCodes.h"
#include "gun_bros_re/host/ZHostSettings.h"

namespace GameDebugKeys {
inline bool StartsTutorial(ZKeyCode key, const ZWindow &window) {
    return GameHostSettings().debugMode && key == GameCheats::Keys::Tutorial && window.WasShiftPressed();
}
// In-game collision overlay; one action per key press.
inline constexpr ZKeyCode ToggleCollision = GameCheats::Keys::Collision;
inline constexpr ZKeyCode ToggleInfo = GameCheats::Keys::Info;
inline bool TogglesCollision(ZKeyCode key, const ZWindow &window) {
    return GameHostSettings().debugMode && key == ToggleCollision && (!GameCheats::Keys::CollisionShift || window.WasShiftPressed());
}
inline bool TogglesInfo(ZKeyCode key, const ZWindow &window) {
    return GameHostSettings().debugMode && key == ToggleInfo && (!GameCheats::Keys::InfoShift || window.WasShiftPressed());
}
// Desktop map research controls. Shift is checked at key-down, not at polling time.
inline constexpr ZKeyCode MapBrowser = GameCheats::Keys::MapBrowser;
inline constexpr ZKeyCode MapPrevious = GameCheats::Keys::Previous;
inline constexpr ZKeyCode MapNext = GameCheats::Keys::Next;
inline constexpr ZKeyCode MapPreviousPage = GameCheats::Keys::PreviousPage;
inline constexpr ZKeyCode MapNextPage = GameCheats::Keys::NextPage;
inline constexpr ZKeyCode MapLoad = GameCheats::Keys::Load;
inline constexpr ZKeyCode MapBack = GameCheats::Keys::Back;
inline bool OpensMapBrowser(ZKeyCode key, const ZWindow &window) {
    return GameHostSettings().debugMode && key == MapBrowser && (!GameCheats::Keys::MapBrowserShift || window.WasShiftPressed());
}
}
