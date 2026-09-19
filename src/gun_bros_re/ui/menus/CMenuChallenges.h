#pragma once
#include "gun_bros_re/data/CChallengeManager.h"
#include "engine/glu/movie/ZMovieRenderer.h"
namespace MenuDetail {
class ZMenuSurface;
class CMenuSystem;
/** CMenuChallenges :236109..237100 owns selection, option glow and sidebar. */
class CMenuChallenges {
public:
    bool DrawOption(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile,
        const ZMovieRegion &region, unsigned index, bool details);
    bool DrawDetails(ZMenuSurface &view, CMenuSystem &state,
        const CProfileManager &profile, const ZMovieRegion &region);
    bool UpdateOptions(ZMovieRenderer &movies, unsigned elapsed);
    CChallengeManager manager;
    unsigned selected = 0;
    std::vector<unsigned> optionTimes;
    unsigned sidebarChallenge = 0, sidebarTime = 0;
    bool sidebarBound = false, sidebarReverse = false;
};
}
