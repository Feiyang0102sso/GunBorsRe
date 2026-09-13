#pragma once
#include "engine/platform/CWindow.h"

namespace GameCheats {
namespace Keys {
// Pair each action key with its Shift flag; false means Shift is not required.
inline constexpr KeyCode Collision = KeyCode::C, Info = KeyCode::I, MapBrowser = KeyCode::M;
inline constexpr KeyCode Tutorial = KeyCode::T;
inline constexpr bool CollisionShift = true, InfoShift = true, MapBrowserShift = true;
inline constexpr KeyCode Previous = KeyCode::Up, Next = KeyCode::Down;
inline constexpr KeyCode PreviousPage = KeyCode::Left, NextPage = KeyCode::Right;
inline constexpr KeyCode Load = KeyCode::Enter, Back = KeyCode::Escape;
}
}
