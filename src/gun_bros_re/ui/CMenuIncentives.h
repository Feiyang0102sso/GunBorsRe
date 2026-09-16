#pragma once
#include "gun_bros_re/ui/ZTextLayout.h"

/** Native incentive-menu bindings: Init/Bind/Draw/Update :292917..293260.
 * Platform rewards remain the caller's local-service responsibility.
 */
class CMenuIncentives {
public:
    static const char *MovieName() { return "GLU_MOVIE_INCENTIVES_POPUP"; }
    static bool DrawControls(ZMovieRenderer &movies, unsigned ordinal, unsigned time,
        std::vector<std::pair<ZMovieRegion, unsigned>> &hits);
    static bool DrawRegion(ZMovieRenderer &movies, const ZMovieRegion &region);
};
