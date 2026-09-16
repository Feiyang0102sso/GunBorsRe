#include "gun_bros_re/ui/CMenuIncentives.h"

bool CMenuIncentives::DrawRegion(ZMovieRenderer &movies, const ZMovieRegion &region) {
    const char *name = nullptr;
    bool heading = false;
    if (region.index == 0) { name = "IDS_POPUP_INCENTIVES_TITLE"; heading = true; }
    if (region.index == 1) { name = "IDS_POPUP_INCENTIVES_ADCOLONY"; }
    if (region.index == 2) { name = "IDS_POPUP_INCENTIVES_TAPJOY"; }
    return DrawPopupText(movies, region, name, heading, false);
}

bool CMenuIncentives::DrawControls(ZMovieRenderer &movies, unsigned ordinal, unsigned time,
    std::vector<std::pair<ZMovieRegion, unsigned>> &hits) {
    return DrawPopupControls(movies, ordinal, time, 3, 4, {16, 17}, {121, 120}, hits);
}
