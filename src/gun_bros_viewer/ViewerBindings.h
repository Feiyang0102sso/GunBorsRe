#pragma once
/** Viewer input and help have one source of truth. Repeated bindings are intentional.
 * Change a key/input here; scene handling and the sidebar follow the same definition.
 * Press fires once, Hold continues while held; pointer gestures are routed by the host.
 */
#include "engine/platform/CWindow.h"
#include <cstddef>

enum class ViewerAction {
    Previous, Next, PreviousPage, NextPage, Drag, Zoom, ResetView, Tiles, Props, Spawns, Collisions, Cover, Barrel, Spire, Pause, Step, Back, PreviousVariant, NextVariant, Tilt, Category1, Category2, Category3, Category4, Category5, Category6, Category7, Fire, MoveUp, MoveLeft, MoveDown, MoveRight, ClearArmor, Spawn, ResetBattle, Grenade, FreezeGrenade, ShockGrenade, Aim, Turret
};
enum class ViewerInput { Press, Hold, LeftDrag, RightDrag, Wheel, Pointer, LeftHold, RightHold };
struct ViewerBinding {
    ViewerAction action;
    KeyCode key;
    ViewerInput input;
    const wchar_t *group;
    const wchar_t *description;
};
struct ViewerBindingSet {
    const wchar_t *name;
    const ViewerBinding *entries;
    std::size_t count;
};

namespace mapview {
// Legacy playable research uses S for movement; its separate shortcut stays compatible.
inline constexpr KeyCode GameViewSpire = KeyCode::F;
inline constexpr ViewerBinding Previous{ViewerAction::Previous, KeyCode::Left, ViewerInput::Press, L"Maps", L"Previous map"};
inline constexpr ViewerBinding Next{ViewerAction::Next, KeyCode::Right, ViewerInput::Press, L"Maps", L"Next map"};
inline constexpr ViewerBinding PreviousPage{ViewerAction::PreviousPage, KeyCode::Up, ViewerInput::Press, L"Maps", L"Previous pack"};
inline constexpr ViewerBinding NextPage{ViewerAction::NextPage, KeyCode::Down, ViewerInput::Press, L"Maps", L"Next pack"};
inline constexpr ViewerBinding Drag{ViewerAction::Drag, KeyCode::None, ViewerInput::LeftDrag, L"View", L"Pan map"};
inline constexpr ViewerBinding Zoom{ViewerAction::Zoom, KeyCode::None, ViewerInput::Wheel, L"View", L"Zoom"};
inline constexpr ViewerBinding ResetView{ViewerAction::ResetView, KeyCode::Home, ViewerInput::Press, L"View", L"Fit map"};
inline constexpr ViewerBinding Tiles{ViewerAction::Tiles, KeyCode::G, ViewerInput::Press, L"Layers", L"Toggle tiles"};
inline constexpr ViewerBinding Props{ViewerAction::Props, KeyCode::P, ViewerInput::Press, L"Layers", L"Toggle props"};
inline constexpr ViewerBinding Spawns{ViewerAction::Spawns, KeyCode::K, ViewerInput::Press, L"Layers", L"Toggle spawn points"};
inline constexpr ViewerBinding Collisions{ViewerAction::Collisions, KeyCode::C, ViewerInput::Press, L"Layers", L"Toggle collisions"};
inline constexpr ViewerBinding Cover{ViewerAction::Cover, KeyCode::B, ViewerInput::Press, L"Object states", L"Cycle cover state"};
inline constexpr ViewerBinding Barrel{ViewerAction::Barrel, KeyCode::E, ViewerInput::Press, L"Object states", L"Cycle barrel state"};
inline constexpr ViewerBinding Spire{ViewerAction::Spire, KeyCode::S, ViewerInput::Press, L"Object states", L"Cycle spire state"};
inline constexpr ViewerBinding Turret{ViewerAction::Turret, KeyCode::T, ViewerInput::Press, L"Object states", L"Cycle turret state"};
inline constexpr ViewerBinding Pause{ViewerAction::Pause, KeyCode::Space, ViewerInput::Press, L"Playback / return", L"Pause / resume"};
inline constexpr ViewerBinding Step{ViewerAction::Step, KeyCode::Period, ViewerInput::Press, L"Playback / return", L"Pause and step"};
inline constexpr ViewerBinding Back{ViewerAction::Back, KeyCode::Escape, ViewerInput::Press, L"Playback / return", L"Return to menu"};
inline constexpr ViewerBinding All[] = {
    Previous, Next, PreviousPage, NextPage, Drag, Zoom, ResetView, Tiles, Props, Spawns, Collisions, Cover, Barrel, Spire, Turret, Pause, Step, Back
};
inline constexpr ViewerBindingSet Bindings{L"Map", All, sizeof(All) / sizeof(All[0])};
}

namespace meshview {
inline constexpr ViewerBinding Previous{ViewerAction::Previous, KeyCode::Left, ViewerInput::Press, L"Meshes", L"Previous mesh"};
inline constexpr ViewerBinding Next{ViewerAction::Next, KeyCode::Right, ViewerInput::Press, L"Meshes", L"Next mesh"};
inline constexpr ViewerBinding PreviousPage{ViewerAction::PreviousPage, KeyCode::Up, ViewerInput::Press, L"Meshes", L"Back 10 meshes"};
inline constexpr ViewerBinding NextPage{ViewerAction::NextPage, KeyCode::Down, ViewerInput::Press, L"Meshes", L"Forward 10 meshes"};
inline constexpr ViewerBinding PreviousVariant{ViewerAction::PreviousVariant, KeyCode::N, ViewerInput::Press, L"Raw animation", L"Previous frame / pause"};
inline constexpr ViewerBinding NextVariant{ViewerAction::NextVariant, KeyCode::M, ViewerInput::Press, L"Raw animation", L"Next frame / pause"};
inline constexpr ViewerBinding Pause{ViewerAction::Pause, KeyCode::Space, ViewerInput::Press, L"Raw animation", L"Pause / resume"};
inline constexpr ViewerBinding Step{ViewerAction::Step, KeyCode::Period, ViewerInput::Press, L"Raw animation", L"Single step"};
inline constexpr ViewerBinding Drag{ViewerAction::Drag, KeyCode::None, ViewerInput::LeftDrag, L"View", L"Rotate / tilt"};
inline constexpr ViewerBinding Zoom{ViewerAction::Zoom, KeyCode::None, ViewerInput::Wheel, L"View", L"Zoom"};
inline constexpr ViewerBinding Tilt{ViewerAction::Tilt, KeyCode::G, ViewerInput::Press, L"View", L"Game / menu tilt"};
inline constexpr ViewerBinding ResetView{ViewerAction::ResetView, KeyCode::Home, ViewerInput::Press, L"View", L"Reset rotation / zoom"};
inline constexpr ViewerBinding Back{ViewerAction::Back, KeyCode::Escape, ViewerInput::Press, L"Return", L"Return to menu"};
inline constexpr ViewerBinding All[] = {
    Previous, Next, PreviousPage, NextPage, PreviousVariant, NextVariant, Pause, Step, Drag, Zoom, Tilt, ResetView, Back
};
inline constexpr ViewerBindingSet Bindings{L"Raw mesh", All, sizeof(All) / sizeof(All[0])};
}

namespace enemyview {
inline constexpr ViewerBinding Previous{ViewerAction::Previous, KeyCode::Left, ViewerInput::Press, L"Enemies", L"Previous enemy"};
inline constexpr ViewerBinding Next{ViewerAction::Next, KeyCode::Right, ViewerInput::Press, L"Enemies", L"Next enemy"};
inline constexpr ViewerBinding PreviousPage{ViewerAction::PreviousPage, KeyCode::Up, ViewerInput::Press, L"Enemies", L"Back 10 enemies"};
inline constexpr ViewerBinding NextPage{ViewerAction::NextPage, KeyCode::Down, ViewerInput::Press, L"Enemies", L"Forward 10 enemies"};
inline constexpr ViewerBinding PreviousVariant{ViewerAction::PreviousVariant, KeyCode::N, ViewerInput::Press, L"State animation", L"Previous state"};
inline constexpr ViewerBinding NextVariant{ViewerAction::NextVariant, KeyCode::M, ViewerInput::Press, L"State animation", L"Next state"};
inline constexpr ViewerBinding Pause{ViewerAction::Pause, KeyCode::Space, ViewerInput::Press, L"State animation", L"Pause / resume"};
inline constexpr ViewerBinding Step{ViewerAction::Step, KeyCode::Period, ViewerInput::Press, L"State animation", L"Single step"};
inline constexpr ViewerBinding Drag{ViewerAction::Drag, KeyCode::None, ViewerInput::LeftDrag, L"View", L"Rotate / tilt"};
inline constexpr ViewerBinding Zoom{ViewerAction::Zoom, KeyCode::None, ViewerInput::Wheel, L"View", L"Zoom"};
inline constexpr ViewerBinding Tilt{ViewerAction::Tilt, KeyCode::G, ViewerInput::Press, L"View", L"Game / menu view"};
inline constexpr ViewerBinding ResetView{ViewerAction::ResetView, KeyCode::Home, ViewerInput::Press, L"View", L"Reset view"};
inline constexpr ViewerBinding Back{ViewerAction::Back, KeyCode::Escape, ViewerInput::Press, L"Return", L"Return to menu"};
inline constexpr ViewerBinding All[] = {
    Previous, Next, PreviousPage, NextPage, PreviousVariant, NextVariant, Pause, Step, Drag, Zoom, Tilt, ResetView, Back
};
inline constexpr ViewerBindingSet Bindings{L"Enemy", All, sizeof(All) / sizeof(All[0])};
}

namespace weaponview {
inline constexpr ViewerBinding Previous{ViewerAction::Previous, KeyCode::Left, ViewerInput::Press, L"Weapons", L"Previous weapon (all)"};
inline constexpr ViewerBinding Next{ViewerAction::Next, KeyCode::Right, ViewerInput::Press, L"Weapons", L"Next weapon (all)"};
inline constexpr ViewerBinding PreviousPage{ViewerAction::PreviousPage, KeyCode::Up, ViewerInput::Press, L"Weapons", L"Back 10 weapons"};
inline constexpr ViewerBinding NextPage{ViewerAction::NextPage, KeyCode::Down, ViewerInput::Press, L"Weapons", L"Forward 10 weapons"};
inline constexpr ViewerBinding PreviousVariant{ViewerAction::PreviousVariant, KeyCode::N, ViewerInput::Press, L"Weapons", L"Previous in category"};
inline constexpr ViewerBinding NextVariant{ViewerAction::NextVariant, KeyCode::M, ViewerInput::Press, L"Weapons", L"Next in category"};
inline constexpr ViewerBinding Category1{ViewerAction::Category1, KeyCode::Digit1, ViewerInput::Press, L"Weapon category", L"Pistol"};
inline constexpr ViewerBinding Category2{ViewerAction::Category2, KeyCode::Digit2, ViewerInput::Press, L"Weapon category", L"Rifle"};
inline constexpr ViewerBinding Category3{ViewerAction::Category3, KeyCode::Digit3, ViewerInput::Press, L"Weapon category", L"Shotgun"};
inline constexpr ViewerBinding Category4{ViewerAction::Category4, KeyCode::Digit4, ViewerInput::Press, L"Weapon category", L"Spread"};
inline constexpr ViewerBinding Category5{ViewerAction::Category5, KeyCode::Digit5, ViewerInput::Press, L"Weapon category", L"Heavy"};
inline constexpr ViewerBinding Category6{ViewerAction::Category6, KeyCode::Digit6, ViewerInput::Press, L"Weapon category", L"Special"};
inline constexpr ViewerBinding Category7{ViewerAction::Category7, KeyCode::Digit7, ViewerInput::Press, L"Weapon category", L"Laser"};
inline constexpr ViewerBinding Fire{ViewerAction::Fire, KeyCode::F, ViewerInput::Hold, L"Presentation", L"Fire preview"};
inline constexpr ViewerBinding MoveUp{ViewerAction::MoveUp, KeyCode::W, ViewerInput::Hold, L"Presentation", L"Walk animation (in place)"};
inline constexpr ViewerBinding MoveLeft{ViewerAction::MoveLeft, KeyCode::A, ViewerInput::Hold, L"Presentation", L"Walk animation (in place)"};
inline constexpr ViewerBinding MoveDown{ViewerAction::MoveDown, KeyCode::S, ViewerInput::Hold, L"Presentation", L"Walk animation (in place)"};
inline constexpr ViewerBinding MoveRight{ViewerAction::MoveRight, KeyCode::D, ViewerInput::Hold, L"Presentation", L"Walk animation (in place)"};
inline constexpr ViewerBinding Drag{ViewerAction::Drag, KeyCode::None, ViewerInput::LeftDrag, L"View", L"Rotate / tilt"};
inline constexpr ViewerBinding Zoom{ViewerAction::Zoom, KeyCode::None, ViewerInput::Wheel, L"View", L"Zoom"};
inline constexpr ViewerBinding Tilt{ViewerAction::Tilt, KeyCode::G, ViewerInput::Press, L"View", L"Game / menu tilt"};
inline constexpr ViewerBinding ResetView{ViewerAction::ResetView, KeyCode::Home, ViewerInput::Press, L"View", L"Reset view"};
inline constexpr ViewerBinding Pause{ViewerAction::Pause, KeyCode::Space, ViewerInput::Press, L"Playback / return", L"Pause / resume"};
inline constexpr ViewerBinding Step{ViewerAction::Step, KeyCode::Period, ViewerInput::Press, L"Playback / return", L"Single step"};
inline constexpr ViewerBinding Back{ViewerAction::Back, KeyCode::Escape, ViewerInput::Press, L"Playback / return", L"Return to menu"};
inline constexpr ViewerBinding All[] = {
    Previous, Next, PreviousPage, NextPage, PreviousVariant, NextVariant, Category1, Category2, Category3, Category4, Category5, Category6, Category7, Fire, MoveUp, MoveLeft, MoveDown, MoveRight, Drag, Zoom, Tilt, ResetView, Pause, Step, Back
};
inline constexpr ViewerBindingSet Bindings{L"Player weapon", All, sizeof(All) / sizeof(All[0])};
}

namespace armorview {
inline constexpr ViewerBinding Previous{ViewerAction::Previous, KeyCode::Left, ViewerInput::Press, L"Armor", L"Previous armor"};
inline constexpr ViewerBinding Next{ViewerAction::Next, KeyCode::Right, ViewerInput::Press, L"Armor", L"Next armor"};
inline constexpr ViewerBinding ClearArmor{ViewerAction::ClearArmor, KeyCode::B, ViewerInput::Press, L"Armor", L"Remove all armor"};
inline constexpr ViewerBinding PreviousPage{ViewerAction::PreviousPage, KeyCode::Up, ViewerInput::Press, L"Equipped weapon", L"Back 10 weapons"};
inline constexpr ViewerBinding NextPage{ViewerAction::NextPage, KeyCode::Down, ViewerInput::Press, L"Equipped weapon", L"Forward 10 weapons"};
inline constexpr ViewerBinding PreviousVariant{ViewerAction::PreviousVariant, KeyCode::N, ViewerInput::Press, L"Equipped weapon", L"Previous in category"};
inline constexpr ViewerBinding NextVariant{ViewerAction::NextVariant, KeyCode::M, ViewerInput::Press, L"Equipped weapon", L"Next in category"};
inline constexpr ViewerBinding Category1{ViewerAction::Category1, KeyCode::Digit1, ViewerInput::Press, L"Weapon category", L"Pistol"};
inline constexpr ViewerBinding Category2{ViewerAction::Category2, KeyCode::Digit2, ViewerInput::Press, L"Weapon category", L"Rifle"};
inline constexpr ViewerBinding Category3{ViewerAction::Category3, KeyCode::Digit3, ViewerInput::Press, L"Weapon category", L"Shotgun"};
inline constexpr ViewerBinding Category4{ViewerAction::Category4, KeyCode::Digit4, ViewerInput::Press, L"Weapon category", L"Spread"};
inline constexpr ViewerBinding Category5{ViewerAction::Category5, KeyCode::Digit5, ViewerInput::Press, L"Weapon category", L"Heavy"};
inline constexpr ViewerBinding Category6{ViewerAction::Category6, KeyCode::Digit6, ViewerInput::Press, L"Weapon category", L"Special"};
inline constexpr ViewerBinding Category7{ViewerAction::Category7, KeyCode::Digit7, ViewerInput::Press, L"Weapon category", L"Laser"};
inline constexpr ViewerBinding Fire{ViewerAction::Fire, KeyCode::F, ViewerInput::Hold, L"Presentation", L"Fire preview"};
inline constexpr ViewerBinding MoveUp{ViewerAction::MoveUp, KeyCode::W, ViewerInput::Hold, L"Presentation", L"Walk animation (in place)"};
inline constexpr ViewerBinding MoveLeft{ViewerAction::MoveLeft, KeyCode::A, ViewerInput::Hold, L"Presentation", L"Walk animation (in place)"};
inline constexpr ViewerBinding MoveDown{ViewerAction::MoveDown, KeyCode::S, ViewerInput::Hold, L"Presentation", L"Walk animation (in place)"};
inline constexpr ViewerBinding MoveRight{ViewerAction::MoveRight, KeyCode::D, ViewerInput::Hold, L"Presentation", L"Walk animation (in place)"};
inline constexpr ViewerBinding Drag{ViewerAction::Drag, KeyCode::None, ViewerInput::LeftDrag, L"View", L"Rotate / tilt"};
inline constexpr ViewerBinding Zoom{ViewerAction::Zoom, KeyCode::None, ViewerInput::Wheel, L"View", L"Zoom"};
inline constexpr ViewerBinding Tilt{ViewerAction::Tilt, KeyCode::G, ViewerInput::Press, L"View", L"Game / menu tilt"};
inline constexpr ViewerBinding ResetView{ViewerAction::ResetView, KeyCode::Home, ViewerInput::Press, L"View", L"Reset view"};
inline constexpr ViewerBinding Pause{ViewerAction::Pause, KeyCode::Space, ViewerInput::Press, L"Playback / return", L"Pause / resume"};
inline constexpr ViewerBinding Step{ViewerAction::Step, KeyCode::Period, ViewerInput::Press, L"Playback / return", L"Single step"};
inline constexpr ViewerBinding Back{ViewerAction::Back, KeyCode::Escape, ViewerInput::Press, L"Playback / return", L"Return to menu"};
inline constexpr ViewerBinding All[] = {
    Previous, Next, ClearArmor, PreviousPage, NextPage, PreviousVariant, NextVariant, Category1, Category2, Category3, Category4, Category5, Category6, Category7, Fire, MoveUp, MoveLeft, MoveDown, MoveRight, Drag, Zoom, Tilt, ResetView, Pause, Step, Back
};
inline constexpr ViewerBindingSet Bindings{L"Player armor", All, sizeof(All) / sizeof(All[0])};
}

namespace arena {
inline constexpr ViewerBinding Previous{ViewerAction::Previous, KeyCode::Left, ViewerInput::Press, L"Enemy / arena", L"Previous enemy"};
inline constexpr ViewerBinding Next{ViewerAction::Next, KeyCode::Right, ViewerInput::Press, L"Enemy / arena", L"Next enemy"};
inline constexpr ViewerBinding Spawn{ViewerAction::Spawn, KeyCode::X, ViewerInput::Press, L"Enemy / arena", L"Spawn same enemy nearby"};
inline constexpr ViewerBinding ResetBattle{ViewerAction::ResetBattle, KeyCode::R, ViewerInput::Press, L"Enemy / arena", L"Reset battle"};
inline constexpr ViewerBinding Grenade{ViewerAction::Grenade, KeyCode::G, ViewerInput::Press, L"Combat", L"Throw default grenade"};
inline constexpr ViewerBinding FreezeGrenade{ViewerAction::FreezeGrenade, KeyCode::Q, ViewerInput::Press, L"Combat", L"Throw freeze grenade"};
inline constexpr ViewerBinding ShockGrenade{ViewerAction::ShockGrenade, KeyCode::E, ViewerInput::Press, L"Combat", L"Throw shock grenade"};
inline constexpr ViewerBinding Zoom{ViewerAction::Zoom, KeyCode::None, ViewerInput::Wheel, L"View", L"Zoom arena"};
inline constexpr ViewerBinding ResetView{ViewerAction::ResetView, KeyCode::Home, ViewerInput::Press, L"View", L"Reset zoom"};
inline constexpr ViewerBinding Collisions{ViewerAction::Collisions, KeyCode::C, ViewerInput::Press, L"Enemy / arena", L"Toggle collisions"};
inline constexpr ViewerBinding MoveUp{ViewerAction::MoveUp, KeyCode::W, ViewerInput::Hold, L"Combat", L"Move up"};
inline constexpr ViewerBinding MoveLeft{ViewerAction::MoveLeft, KeyCode::A, ViewerInput::Hold, L"Combat", L"Move left"};
inline constexpr ViewerBinding MoveDown{ViewerAction::MoveDown, KeyCode::S, ViewerInput::Hold, L"Combat", L"Move down"};
inline constexpr ViewerBinding MoveRight{ViewerAction::MoveRight, KeyCode::D, ViewerInput::Hold, L"Combat", L"Move right"};
inline constexpr ViewerBinding Aim{ViewerAction::Aim, KeyCode::None, ViewerInput::Pointer, L"Combat", L"Aim"};
inline constexpr ViewerBinding Fire{ViewerAction::Fire, KeyCode::None, ViewerInput::LeftHold, L"Combat", L"Fire"};
inline constexpr ViewerBinding PreviousVariant{ViewerAction::PreviousVariant, KeyCode::N, ViewerInput::Press, L"Equipped weapon", L"Previous in category"};
inline constexpr ViewerBinding NextVariant{ViewerAction::NextVariant, KeyCode::M, ViewerInput::Press, L"Equipped weapon", L"Next in category"};
inline constexpr ViewerBinding Category1{ViewerAction::Category1, KeyCode::Digit1, ViewerInput::Press, L"Weapon category", L"Pistol"};
inline constexpr ViewerBinding Category2{ViewerAction::Category2, KeyCode::Digit2, ViewerInput::Press, L"Weapon category", L"Rifle"};
inline constexpr ViewerBinding Category3{ViewerAction::Category3, KeyCode::Digit3, ViewerInput::Press, L"Weapon category", L"Shotgun"};
inline constexpr ViewerBinding Category4{ViewerAction::Category4, KeyCode::Digit4, ViewerInput::Press, L"Weapon category", L"Spread"};
inline constexpr ViewerBinding Category5{ViewerAction::Category5, KeyCode::Digit5, ViewerInput::Press, L"Weapon category", L"Heavy"};
inline constexpr ViewerBinding Category6{ViewerAction::Category6, KeyCode::Digit6, ViewerInput::Press, L"Weapon category", L"Special"};
inline constexpr ViewerBinding Category7{ViewerAction::Category7, KeyCode::Digit7, ViewerInput::Press, L"Weapon category", L"Laser"};
inline constexpr ViewerBinding Pause{ViewerAction::Pause, KeyCode::Space, ViewerInput::Press, L"Playback / return", L"Pause / resume"};
inline constexpr ViewerBinding Step{ViewerAction::Step, KeyCode::Period, ViewerInput::Press, L"Playback / return", L"Pause and step"};
inline constexpr ViewerBinding Back{ViewerAction::Back, KeyCode::Escape, ViewerInput::Press, L"Playback / return", L"Return to menu"};
inline constexpr ViewerBinding All[] = {
    Previous, Next, Spawn, ResetBattle, Collisions, MoveUp, MoveLeft, MoveDown, MoveRight, Aim, Fire, Grenade, FreezeGrenade, ShockGrenade, PreviousVariant, NextVariant, Category1, Category2, Category3, Category4, Category5, Category6, Category7, Zoom, ResetView, Pause, Step, Back
};
inline constexpr ViewerBindingSet Bindings{L"Arena", All, sizeof(All) / sizeof(All[0])};
}
