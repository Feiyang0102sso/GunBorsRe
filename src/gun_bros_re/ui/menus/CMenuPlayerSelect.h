#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/data/CProfileManager.h"

namespace MenuDetail {
class CMenuSystem;
class ZMenuSurface;
/** CMenuPlayerSelect :194878..195343; original Movie70 owns both portraits,
 * highlights, chapter timings, title and touch rectangles. */
class CMenuPlayerSelect {
public:
    bool Draw(ZMenuSurface &view, CMenuSystem &state, CProfileManager &profile,
        const std::filesystem::path &savePath, bool &launchTutorial);

    bool playerSelectBound = false, playerSelectReady = false;
    int playerSelection = -1;
    unsigned playerSelectTime = 0, playerSelectChapter = 1;
    std::uint64_t playerSelectLastTick = 0;

};
}
