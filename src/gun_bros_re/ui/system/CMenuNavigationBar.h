#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"

namespace MenuDetail {
class ZMenuSurface;
/** CMenuNavigationBar::Init/Update owns Header and InfoCluster playback. */
class CMenuNavigationBar {
public:
    int Draw(ZMenuSurface &view, const CProfileManager &profile,
        const CPlayerProgress &progress, unsigned currentPage);
    bool IsReady() const { return navigationReady; }
    unsigned Time() const { return originalHeaderTime; }
private:
    bool navigationVisible = false;
    bool navigationReady = false;
    bool originalHeaderBound = false;
    unsigned originalHeaderTime = 0, originalHeaderButtonTime = 0;
    std::uint64_t originalHeaderTick = 0;
};
}
