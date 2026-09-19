#pragma once
#include "engine/platform/ZWindow.h"
#include "gun_bros_re/ui/ZHudState.h"

/** Desktop gameplay bindings; viewer catalog and debug controls have their own owners. */
namespace ZGameKeys {
inline constexpr ZKeyCode MoveUp = ZKeyCode::W;
inline constexpr ZKeyCode MoveLeft = ZKeyCode::A;
inline constexpr ZKeyCode MoveDown = ZKeyCode::S;
inline constexpr ZKeyCode MoveRight = ZKeyCode::D;
inline constexpr ZKeyCode OpenShop = ZKeyCode::Digit1;
inline constexpr ZKeyCode SwapWeapon = ZKeyCode::Digit2;
inline constexpr ZKeyCode LeftPowerup = ZKeyCode::Q;
inline constexpr ZKeyCode RightPowerup = ZKeyCode::E;
inline constexpr ZKeyCode Pause = ZKeyCode::Space;
inline constexpr ZKeyCode Back = ZKeyCode::Escape;

inline ZInputPadAction Action(ZKeyCode key) {
    if (key == OpenShop) { return ZInputPadAction::OpenShop; }
    if (key == SwapWeapon) { return ZInputPadAction::SwapWeapon; }
    if (key == LeftPowerup) { return ZInputPadAction::UseLeft; }
    if (key == RightPowerup) { return ZInputPadAction::UseItem; }
    if (key == Pause || key == Back) { return ZInputPadAction::Pause; }
    return ZInputPadAction::None;
}

inline void AppendShortcut(std::vector<ZKeyCode> &inputs, ZKeyCode key) {
    // Desktop binding policy: F/R are not gameplay shortcuts. Pointer Retry
    // and NextItem actions are dispatched separately and remain available.
    // G and viewer N/M catalog selection also stay outside the gameplay path.
    if (Action(key) != ZInputPadAction::None) { inputs.push_back(key); }
}
} // namespace ZGameKeys
