#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/data/CProfileManager.h"

namespace MenuDetail {
class CMenuSystem;
class ZMenuSurface;
class CMenuMission {
public:
    class Presentation;
    bool Draw(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile);

    int startingWave = -1;
    float starPanX = 0, starPanY = 0;
    bool starBound = false, starReverse = false, starLocked = false, starEntering = false;
    unsigned starTime = 0, starReticleTime = 0, starFlagTime = 0, starFadeTime = 0;
    int starSelectedSlot = -1, starTargetTime = -1;
    float starSpeed = 0;
    float starSelectorX = 0, starSelectorY = 0, starFlagX = 0, starFlagY = 0;
    std::uint64_t starLastTick = 0;

};
}
