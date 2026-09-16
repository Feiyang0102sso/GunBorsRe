#pragma once
#include "engine/platform/ZWindow.h"

namespace GameCheats {
namespace Keys {
// Pair each action key with its Shift flag; false means Shift is not required.
inline constexpr ZKeyCode Collision = ZKeyCode::C, Info = ZKeyCode::I, MapBrowser = ZKeyCode::M;
inline constexpr ZKeyCode Tutorial = ZKeyCode::T;
inline constexpr bool CollisionShift = true, InfoShift = true, MapBrowserShift = true;
inline constexpr ZKeyCode Previous = ZKeyCode::Up, Next = ZKeyCode::Down;
inline constexpr ZKeyCode PreviousPage = ZKeyCode::Left, NextPage = ZKeyCode::Right;
inline constexpr ZKeyCode Load = ZKeyCode::Enter, Back = ZKeyCode::Escape;
}
}
