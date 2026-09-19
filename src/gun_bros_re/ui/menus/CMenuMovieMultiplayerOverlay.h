#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/data/CProfileManager.h"

namespace MenuDetail {
class CMenuSystem;
class ZMenuSurface;
class CMenuMovieMultiplayerOverlay {
public:
    class Effects;
    class RegionCallback;
public:
    bool Draw(ZMenuSurface &view, CMenuSystem &state);

    bool modeSelected = false;
    bool modeBound = false;
    unsigned modeTime = 0, modePhase = 0, modeSpriteTime = 0;
    std::uint64_t modeLastTick = 0;

};
}

namespace MenuDetail {

/** The original mode medallions are MDS_BUTTON_MP_TOGGLE, archetype 8. */

/** CMenuMovieMultiplayerOverlay :250020..250880, region callbacks 0..5,
 * font 0 and MDS_BUTTON_MP_TOGGLE. No locally fabricated online mode. */

}
