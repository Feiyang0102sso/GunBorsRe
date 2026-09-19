#pragma once
#include "gun_bros_re/ui/system/CMenuSystem.h"
namespace MenuDetail {

/** CMenuFriends::Bind :197028 and CMenuChallenges::Bind :236612 select
 * chapter 1 while profile validity is false. Region 0 owns button 165/0,
 * region 1 owns centered font-0 text. No host flag can validate an NGS user.
 * ui_movie.bt and MENU_CHALLENGES VA 0x402eb0 identify the original Movie. */

bool BeginLocalMatch(CMenuSystem &state);
bool TakeLocalMatch(CMenuSystem &state, std::uint64_t clock);
void UpdateLocalConnection(CMenuSystem &state);

}
