#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/data/CProfileManager.h"
#include "engine/glu/sprite/CSpritePlayer.h"

namespace MenuDetail {
class CMenuSystem;
class ZMenuSurface;
class CMenuGameResources {
public:
    class Effects;
    bool DrawOverlay(ZMenuSurface &view, const CMenuSystem &state) const;

    bool Draw(ZMenuSurface &view, CMenuSystem &state, CProfileManager &profile,
        const CRefinementManager::Template &data, const std::filesystem::path &savePath, std::int64_t now);

    struct Chamber {
        CSpritePlayer eye;
        bool locked = false;
        bool opening = false;
        unsigned overlayTime = 0;
        bool clickPlaying = false;
        unsigned clickTime = 0;
    };
    std::array<Chamber, kRefinementSlotCount> chambers;
    bool refineryCancelTransfer = false;
    unsigned refineryTab = 0, casualtyPage = 0;
    bool refineryBound = false;
    bool refineryExitPending = false;
    unsigned refineryTime = 0, refineryElapsed = 0;
    // Shared Movie 10 survives page rebinds, independently of chamber playback.
    unsigned backgroundTime = 0;
    std::uint64_t refineryLastTick = 0;
    std::array<unsigned, kRefinementSlotCount> refineryStatusTime{}, refineryFillTime{};
    std::array<unsigned, kRefinementSlotCount> refineryStatusChapter{};
    int refineryTransfer = -1;
    unsigned refineryTransferTime = 0, refineryTransferSprite = 0;
    std::uint64_t refineryTransferAmount = 0;
    float refineryTransferX = 0, refineryTransferY = 0, refineryTargetX = 0, refineryTargetY = 0;

};
}

namespace MenuDetail {

/** CMenuGameResources::Init/Bind :172652/172835, MENU_GAME_RESOURCES
 * VA0x402d50. Original provider SLOT_PHASE_OFFSETS={0,6}, COUNT={6,6}.
 * Resource dimensions, durations, text, sprite geometry and prices stay BIG. */

/** CMenuGameResources::DrawOverlay :173407 runs above the navigation bar. */

}

namespace MenuDetail {
/** Standard intervals occupy 6..11; premium intervals occupy 0..5. */

/** Original menu provider 69; strings are BIG resources except the native
 * GetTimeIntervalString printf patterns at ARM VA 0x3c4980/0x3c49c4. */
std::string RefineryNumber(ZMenuSurface &view, const char *name, std::uint64_t value);

bool SetRefineryStatus(ZMenuSurface &view, CMenuSystem &state, unsigned slot, unsigned chapter);

}
