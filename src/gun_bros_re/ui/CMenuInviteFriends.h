#pragma once
#include "gun_bros_re/ui/ZTextLayout.h"

/** Native invite-menu bindings: Init/Bind/Draw/Update :248330..248861.
 * Geometry, animation and text payloads are still read through ui_movie.bt.
 */
class CMenuInviteFriends {
public:
    static const char *MovieName() { return "GLU_MOVIE_ADD_FRIENDS_POPUP_SMALL"; }
    static bool DrawControls(ZMovieRenderer &movies, unsigned ordinal, unsigned time,
        std::vector<std::pair<ZMovieRegion, unsigned>> &hits);
    static bool DrawRegion(ZMovieRenderer &movies, const ZMovieRegion &region);
};
