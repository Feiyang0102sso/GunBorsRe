#include "gun_bros_re/ui/CMenuInviteFriends.h"

bool CMenuInviteFriends::DrawRegion(ZMovieRenderer &movies, const ZMovieRegion &region) {
    // Callback bindings are native code, not resource layout values.
    static constexpr const char *inviteTitles[] = {
        "IDS_POPUP_INVITE_FRIENDS_INVITE_MORE", "IDS_POPUP_INVITE_FRIENDS_FRIENDS",
        "IDS_POPUP_INVITE_FRIENDS_GET_MORE", "IDS_POPUP_INVITE_FRIENDS_MONEY",
        "IDS_POPUP_INVITE_FRIENDS_EQUALS"};
    const char *name = nullptr;
    bool heading = false;
    if (region.index < 5) { name = inviteTitles[region.index]; heading = true; }
    if (region.index == 5) { name = "IDS_POPUP_INVITE_FRIENDS_BODY"; }
    return DrawPopupText(movies, region, name, heading, true);
}

bool CMenuInviteFriends::DrawControls(ZMovieRenderer &movies, unsigned ordinal, unsigned time,
    std::vector<std::pair<ZMovieRegion, unsigned>> &hits) {
    return DrawPopupControls(movies, ordinal, time, 6, 7, {6, 5}, {126, 127}, hits);
}
