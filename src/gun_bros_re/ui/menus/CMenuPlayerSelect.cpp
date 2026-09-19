#include "gun_bros_re/ui/host/ZStoreRegionClip.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
namespace MenuDetail {

/** CMenuPlayerSelect :194878..195343; original Movie70 owns both portraits,
 * highlights, chapter timings, title and touch rectangles. */
bool CMenuPlayerSelect::Draw(ZMenuSurface &view, CMenuSystem &state, CProfileManager &profile,
    const std::filesystem::path &savePath, bool &launchTutorial) {
    launchTutorial = false;
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_PLAYER_SELECT");
    const auto *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(state.selection.playerSelectChapter, start, end)) { return false; }
    if (!state.selection.playerSelectBound) {
        state.selection.playerSelectBound = true;
        state.selection.playerSelectReady = false;
        state.selection.playerSelection = -1;
        state.selection.playerSelectChapter = 1;
        state.selection.playerSelectTime = 0;
        state.selection.playerSelectLastTick = view.clock;
        if (!movie->GetChapterRange(1, start, end)) { return false; }
    }
    const unsigned delta = static_cast<unsigned>(view.clock - state.selection.playerSelectLastTick);
    state.selection.playerSelectLastTick = view.clock;
    const auto next = static_cast<std::uint64_t>(state.selection.playerSelectTime) + delta;
    const bool finished = next > end;
    if (finished && state.selection.playerSelection < 0) { state.selection.playerSelectReady = true; }
    state.selection.playerSelectTime = static_cast<unsigned>(std::min<std::uint64_t>(next, end));
    if (!view.animateNavigation && state.selection.playerSelection < 0) { state.selection.playerSelectTime = end; }
    if (finished && state.selection.playerSelection >= 0) {
        state.selection.playerSelectBound = false;
        if (state.stack.page == 25) { launchTutorial = true; }
        else { state.Navigate(6, true); }
        return true;
    }
    class SelectTitle : public ZMovieRegionCallback {
    public:
        explicit SelectTitle(ZMenuSurface &menu) : view(menu) {}
        bool DrawMovieRegion(const ZMovieRegion &region) override {
            if (region.index != 0) { return true; }
            const auto title = view.movies.NamedString("IDS_PLAYER_SELECT");
            return view.movies.Text(title, region.x + (region.width - view.movies.TextWidth(title, 6)) / 2,
                region.y + (region.height - view.movies.TextHeight(6)) / 2, 6, 1, 0, region.alpha);
        }
        ZMenuSurface &view;
    } title(view);
    view.movies.Rectangle(0, 0, kMenuWidth, kMenuHeight, 0, 0, 0);
    if (!view.movies.Draw(ordinal, state.selection.playerSelectTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &title)) { return false; }
    const bool ready = state.selection.playerSelection < 0 && (state.selection.playerSelectReady || !view.animateNavigation);
    for (unsigned index = 0; index < 2; ++index) {
        ZMovieRegion bounds;
        // Bind/GetUserRegion(...,true) retains the logical, unscaled rectangles.
        if (!view.movies.Region(ordinal, index + 1, 0, bounds)) { return false; }
        if (ready && view.Hit(bounds.x, bounds.y, bounds.width, bounds.height)) {
            state.selection.playerSelection = static_cast<int>(index);
            state.selection.playerSelectChapter = 2;
            if (index != 0) { state.selection.playerSelectChapter = 4; }
            if (!movie->GetChapterRange(state.selection.playerSelectChapter, start, end)) { return false; }
            state.selection.playerSelectTime = start;
            // CMenuAction 0x4E :94882 sets both brothers and clears firstLaunch.
            profile.playerBrother = index;
            profile.firstLaunch = false;
            if (!profile.SaveToDisk(savePath)) { return false; }
        }
    }
    view.inputEnabled = false; // Native selection owns input until its chapter completes.
    return true;
}
} // namespace MenuDetail
