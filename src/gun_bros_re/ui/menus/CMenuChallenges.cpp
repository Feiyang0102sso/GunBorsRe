#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
#include "gun_bros_re/ui/host/ZStoreRegionClip.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
namespace MenuDetail {
namespace {
bool Chapter(ZMovieRenderer &movies, const char *name, unsigned chapter, unsigned &start, unsigned &end) {
    const auto *movie = movies.GetMovie(movies.Ordinal(name));
    return movie != nullptr && movie->GetChapterRange(chapter, start, end);
}
}

bool CMenuChallenges::DrawDetails(ZMenuSurface &view, CMenuSystem &state,
    const CProfileManager &profile, const ZMovieRegion &region) {

    unsigned start = 0, end = 0;
    if (!Chapter(view.movies, "GLU_MOVIE_BRO_OPS_DETAILS", 1, start, end)) { return false; }
    if (!sidebarBound) {
        sidebarBound = true;
        sidebarChallenge = selected;
        sidebarTime = start;
        if (!view.animateNavigation) { sidebarTime = end; }
        sidebarReverse = false;
    }
    // CMenuChallenges::Refresh action106 :236507 reverses the old sidebar.
    // Update :236264 uses 2*dt and rebinds only when it reaches the hidden pose.
    if (sidebarChallenge != selected) { sidebarReverse = true; }
    unsigned elapsed = 2 * state.social.contentElapsed;
    if (sidebarReverse) {
        const unsigned remaining = sidebarTime - start;
        if (elapsed < remaining) {
            sidebarTime -= elapsed;
            elapsed = 0;
        } else {
            elapsed -= remaining;
            sidebarTime = start;
            sidebarChallenge = selected;
            sidebarReverse = false;
        }
    }
    if (!sidebarReverse) { sidebarTime = std::min(end, sidebarTime + elapsed); }
    return DrawOption(view, state, profile, region, sidebarChallenge, true);

}
bool CMenuChallenges::UpdateOptions(ZMovieRenderer &movies, unsigned elapsed) {
    const unsigned count = static_cast<unsigned>(manager.current.size());
    if (count == 0) { optionTimes.clear(); selected = 0; return true; }
    if (selected >= count) { selected = 0; }
    unsigned idleStart = 0, idleEnd = 0, focusStart = 0, focusEnd = 0;
    if (!Chapter(movies, "GLU_MOVIE_BRO_OP_BOX", 0, idleStart, idleEnd) ||
        !Chapter(movies, "GLU_MOVIE_BRO_OP_BOX", 1, focusStart, focusEnd)) { return false; }
    if (optionTimes.size() != count) {
        optionTimes.assign(count, idleEnd);
        optionTimes[selected] = focusStart;
    }
    // Focus loops the authored glow. UnFocus plays that same chapter in reverse.
    for (unsigned index = 0; index < count; ++index) {
        auto &time = optionTimes[index];
        if (index == selected) {
            if (time < focusStart) { time = focusStart; }
            time = focusStart + (time - focusStart + elapsed) % (focusEnd - focusStart + 1);
        } else if (time >= focusStart) {
            if (elapsed >= time - focusStart) { time = idleEnd; }
            else { time -= elapsed; }
        }
    }

    return true;
}
}
