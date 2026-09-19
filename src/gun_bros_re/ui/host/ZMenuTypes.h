#pragma once
/** Shared resource types and desktop canvas units; no page state. */
#include "gun_bros_re/data/ZWeaponCatalog.h"
#include "gun_bros_re/data/ZArmorCatalog.h"
#include "gun_bros_re/data/ZPlanetCatalog.h"
#include "gun_bros_re/data/ZMissionCatalog.h"
#include "engine/glu/movie/ZMovieRenderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <chrono>

namespace MenuDetail {

constexpr unsigned kArmorSlots[] = {0, 0, 2, 1, 0};
constexpr float kMenuWidth = 1024;
// The store's character column starts where the black content area does; the
// original character reaches up past the category tabs on that side.

constexpr float kMenuHeight = 768;

std::int64_t CurrentSeconds();

bool SameObject(const GameObjectRef &first, const GameObjectRef &second);
/** Which navigation branch a host page sits in, named by that branch's own page.
 *
 * CMenuSystem::SetBranch :96614 leaves through its first test when the branch
 * asked for is the one already shown, and PushMenu/SetMenu :96666/:96700 route
 * every in-branch menu through that same early exit -- only the other path
 * restarts the WIPE movie with CMovie::SetTime(..., 0). So the sweep belongs to
 * navigation between branches. Menus inside one branch never play it: the store
 * category buttons carry action 64, which DoAction :93478 hands to the store
 * menu's own handler :95106 without going near SetBranch, and a planet click
 * pushes the REV list into the branch it is already in.
 *
 * The groups below are the ones the header already lights up as one option.
 */
unsigned MenuBranchPage(unsigned page);

}
