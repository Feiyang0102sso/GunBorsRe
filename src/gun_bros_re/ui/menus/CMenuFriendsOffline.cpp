#include "gun_bros_re/ui/host/ZStoreRegionClip.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
namespace MenuDetail {

/** CMenuFriends::Bind :197028 and CMenuChallenges::Bind :236612 select
 * chapter 1 while profile validity is false. Region 0 owns button 165/0,
 * region 1 owns centered font-0 text. No host flag can validate an NGS user.
 * ui_movie.bt and MENU_CHALLENGES VA 0x402eb0 identify the original Movie. */
bool CMenuFriends::DrawOffline(ZMenuSurface &view, CMenuSystem &state, bool hasCredentials) {
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_OFFLINE_BROHOOD");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, start, end)) { return false; }
    if (!state.social.socialBound) {
        state.social.socialBound = true;
        state.social.socialTime = start;
        state.social.socialLastTick = view.clock;
    }
    const auto elapsed = view.clock - state.social.socialLastTick;
    state.social.socialLastTick = view.clock;
    state.social.socialTime = start + static_cast<unsigned>((state.social.socialTime - start + elapsed) % (end - start + 1));
    if (!view.movies.Draw(ordinal, state.social.socialTime)) { return false; }
    const char *table = "MDS_OFFLINE_CHALLENGES";
    if (state.stack.page == 4) { table = "MDS_OFFLINE_FRIENDS"; }
    // GetElementValueInt32(81) :150866: invalid+credentials=>1, absent=>2;
    // each menu subtracts one to index its two original description strings.
    unsigned description = 1;
    if (hasCredentials) { description = 0; }
    const auto *entry = CMenuDataProvider::Find(table, description);
    const auto *button = CMenuDataProvider::Find("MDS_BUTTON_CONNECTIVITY", 0);
    if (entry == nullptr || button == nullptr) { return false; }
    for (const auto &region : view.movies.Regions(ordinal, state.social.socialTime)) {
        if (region.index == 0) {
            bool pressed = false;
            if (!CMenuMovieButton::DrawFrame(view, *button, region, view.movies.NamedString(button->strings[0]), 6, true, pressed)) { return false; }
            if (pressed) {
                // Action 86 requests NGS connectivity. The absent remote service
                // cannot mint a validated identity, friendship or reward locally.
                std::printf("[social] connectivity action=%u unavailable; profile remains offline\n", button->action);
            }
        }
        if (region.index == 1) {
            const auto lines = CTextBox::Format(view.movies, view.movies.NamedString(entry->strings[0]), region.width, {0, 0, 0, 0, 0});
            ZStoreRegionClip clip(view, region);
            float y = region.y;
            for (const auto &line : lines) {
                const float x = region.x + (region.width - line.width) / 2;
                for (const auto &run : line.runs) {
                    view.movies.Text(run.text, x + run.x, y, run.font, 1, 0, region.alpha);
                }
                y += line.height;
            }
        }
    }
    return true;
}
} // namespace MenuDetail
