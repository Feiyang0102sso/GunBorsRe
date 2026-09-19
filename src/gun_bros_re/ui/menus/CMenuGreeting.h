#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/data/CProfileManager.h"

namespace MenuDetail {
class CMenuSystem;
class ZMenuSurface;
class CMenuGreeting {
public:
    bool Draw(ZMenuSurface &view, CMenuSystem &state, CResTOCManager &toc, ZPackTables &tables,
        CProfileManager &profile, const CDailyBonusTracking &daily, const std::vector<ZStoreEntry> &store,
        CPlayerProgress &progress, const std::filesystem::path &savePath, std::int64_t seconds);

    bool greetingBound = false, greetingExitRequested = false, greetingClosing = false;
    unsigned greetingTime = 0, greetingElapsed = 0, greetingTarget = 0;
    std::uint64_t greetingLastTick = 0;

};
}

namespace MenuDetail {

/** CMenuGreeting callbacks :207876..208207. Local daily rewards are the
 * user-authorized clock adapter; social entry follows the service availability. */

/** User-authorized cht advances the native elapsed-day accumulator. The saved
 * launch timestamp stays on the real clock, so restarting cannot underflow it. */

}
