#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/data/profile/CProfileManager.h"
#include "gun_bros_re/gameplay/game/CGameFlow.h"

namespace MenuDetail {
class CMenuSystem;
class ZMenuSurface;
class CMenuPostGame {
public:
    void Refresh(CMenuSystem &state, const CGameFlow &context, const std::vector<CGun::Entry> &weapons);

    bool Draw(ZMenuSurface &view, CMenuSystem &state, CResTOCManager &toc, CGunBros &tables,
        const CProfileManager &profile);

    float livePosition = 0;
    bool liveReplay = false;
    std::uint64_t liveReplayAt = 0;
    bool postGameMusic = false;
    bool postGameBound = false, postGameUpgradePending = false, postGameClosing = false;
    unsigned postGameTime = 0, postGameItemTime = 0, postGameCloseTime = 0;
    unsigned postGameIconTime = 0;
    float postGameGalleryPosition = 0, postGameGalleryVelocity = 0;
    unsigned postGameDelta = 0;
    std::uint64_t postGameLastTick = 0;

};
}

namespace MenuDetail {
/** Resource printf substitution for the original CGame result strings. */
std::string PostGameFormat(ZMenuSurface &view, const char *name, const std::vector<std::string> &values);

/** CMenuPostGame::OverviewCallback :164593; single-player default bro
 * provider93 count=3, last data index=7+Mission.type-4. */

/** MENU_POST_GAME_WRAPUP VA0x403350, CMenuPostGame :164559..166204.
 * Native menu/provider logic below; layouts, fonts and artwork stay in BIG. */

}
