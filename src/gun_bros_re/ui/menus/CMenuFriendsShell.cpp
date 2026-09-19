/** Original social bindings plus the Windows Game Center boundary. */
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/controls/CTextBox.h"

namespace MenuDetail {

class ZLocalSocialCallbacks : public ZMovieRegionCallback {
public:
    ZLocalSocialCallbacks(ZMenuSurface &menu, CMenuSystem &menuState, const CProfileManager &profile) :
        view(menu), state(menuState), profile(profile) {}

    bool DrawMovieRegion(const ZMovieRegion &region) override {
        const bool challenges = state.stack.page == 5;
        if (region.index == 1) {
            const char *table = "MDS_BUTTON_FRIENDS_CATEGORIES";
            if (challenges) { table = "MDS_BUTTON_CHALLENGE_CATEGORIES"; }
            float widths[3]{};
            const CMenuDataProvider::Entry *buttons[3]{};
            for (unsigned index = 0; index < 3; ++index) {
                buttons[index] = CMenuDataProvider::Find(table, index);
                if (buttons[index] == nullptr) { return false; }
                ZMovieRegion bounds;
                if (!view.movies.Region(view.movies.Ordinal(buttons[index]->movies[0]), 1, 0, bounds)) { return false; }
                widths[index] = bounds.width;
            }
            // CMenuFriends::TabButtonCallback :195569 and
            // CMenuChallenges::CategoryCallback :235569 distribute remaining width.
            const float gap = (region.width - widths[0] - widths[1] - widths[2]) / 2;
            ZMovieRegion origin = region;
            for (unsigned index = 0; index < 3; ++index) {
                unsigned chapter = 0;
                if (state.social.socialTab == index) { chapter = 1; }
                bool pressed = false;
                if (!CMenuMovieButton::DrawFrame(view, *buttons[index], origin,
                    view.movies.NamedString(buttons[index]->strings[0]), 5, true, pressed,
                    chapter, 0, UINT32_MAX, true)) { return false; }
                if (pressed && state.social.socialTab != index) {
                    state.social.socialTab = index;
                    state.social.scrollPosition = 0;
                    state.social.scrollMotion = ZMenuScrollMotion{};
                }
                origin.x += widths[index] + gap;
            }
        }
        if (!challenges && region.index == 0 && state.social.socialTab != 1) {
            // TitleCallback :195524; the active default brother is local.
            const auto *title = CMenuDataProvider::Find("MDS_FRIEND_MENU", 0);
            if (title == nullptr) { return false; }
            // TitleCallback paints without a width limit; region0 is an anchor,
            // not a clipping rectangle for the full title.
            view.movies.Text(view.movies.NamedString(title->strings[0]), region.x, region.y, 0, 1, 0, region.alpha);
        }
        if (challenges && region.index == 2) {
            // TitleCallback :235637 anchors title at main region0's top, with
            // the original font6, then paints the body below it using font0.
            // Correction to the historical note: BindContent :236691 binds
            // font1 and centers the body; the heading still uses font6.
            const auto *entry = CMenuDataProvider::Find("MDS_CHALLENGE_MENU", 0);
            ZMovieRegion main;
            if (entry == nullptr || !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_BROBUFF_MENU"),
                0, state.social.socialTime, main)) { return false; }
            ZMovieRegion textArea = region;
            textArea.y = main.y;
            DrawMissionText(view, textArea, view.movies.NamedString(entry->strings[0]), 6, true);
            textArea.y += view.movies.TextHeight(6);
            DrawMissionText(view, textArea, view.movies.NamedString(entry->strings[1]), 1, true);
        }
        return state.social.DrawContent(view, state, profile, region);
    }

private:
    ZMenuSurface &view;
    CMenuSystem &state;
    const CProfileManager &profile;
};

bool CMenuFriends::Draw(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile, bool hasCredentials) {
    UpdateLocalConnection(state);
    const bool onlinePage = state.online.IsConnected() && (state.stack.page == 4 || state.stack.page == 5);
    if (state.social.onlinePage != onlinePage) {
        state.social.socialBound = false;
        state.social.onlinePage = onlinePage;
    }
    if (!onlinePage) { return state.social.DrawOffline(view, state, hasCredentials); }
    if (state.social.contentPage != state.stack.page) {
        state.social.contentPage = state.stack.page;
        state.social.socialTab = 0;
        state.social.scrollPosition = 0;
        state.social.scrollMotion = ZMenuScrollMotion{};
        state.challenges.selected = 0;
        state.challenges.optionTimes.clear();
        state.challenges.sidebarBound = false;
    }
    if (!state.social.BindContent(view, state, profile)) { return false; }
    // Original ARMv7 MENU_FRIENDS 0x402d70+4 and MENU_CHALLENGES 0x402eb0+4.
    // Both load this BIG movie; their region callbacks supply different content.
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_BROBUFF_MENU");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0, loopStart = 0, loopEnd = 0;
    if (movie == nullptr || !movie->GetChapterRange(0, start, end) ||
        !movie->GetChapterRange(1, loopStart, loopEnd)) { return false; }
    if (!state.social.socialBound) {
        state.social.socialBound = true;
        state.social.socialLastTick = view.clock;
        state.social.socialTime = start;
        if (!view.animateNavigation) { state.social.socialTime = loopStart; }
        std::printf("[local-online] social page=%u ready remote-records=0\n", state.stack.page);
    }
    const auto elapsed = static_cast<unsigned>(view.clock - state.social.socialLastTick);
    state.social.socialLastTick = view.clock;
    // CMenuFriends::OnShow :196222 / CMenuChallenges::OnFocus :236109
    // loop chapter 1 after the entrance; its tiled sprite scrolls the warning tape.
    state.social.socialTime += elapsed;
    if (state.social.socialTime > loopEnd) {
        state.social.socialTime = loopStart + (state.social.socialTime - loopStart) % (loopEnd - loopStart + 1);
    }
    state.social.contentElapsed = elapsed;
    state.social.renderedEntries = 0;
    ZLocalSocialCallbacks callbacks(view, state, profile);
    if (!view.movies.Draw(ordinal, state.social.socialTime, kMenuWidth / 2, kMenuHeight / 2,
        kMenuWidth, kMenuHeight, 0, 1, &callbacks)) { return false; }
    return state.social.DrawModel(view, state, profile);
}
} // namespace MenuDetail
