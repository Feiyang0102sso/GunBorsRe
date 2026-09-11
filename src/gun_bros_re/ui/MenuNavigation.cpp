#include "gun_bros_re/ui/MenuInternal.h"
namespace MenuDetail {

std::int64_t CurrentSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

bool SameObject(const GameObjectRef &first, const GameObjectRef &second) {
    return first.packHash == second.packHash && first.localIndex == second.localIndex;
}

/** Which navigation branch a host page sits in, named by that branch's own page.
 *
 * CMenuSystem::SetBranch :96614 leaves through its first test when the branch
 * asked for is the one already shown, and PushMenu/SetMenu :96666/:96700 route
 * every in-branch menu through that same early exit -- only the other path
 * restarts the WIPE movie with CMovie::SetTime(..., 0). So the sweep belongs to
 * navigation between branches. Menus inside one branch never play it: the store
 * category buttons carry action 64, which DoAction :93478 hands to the store
 * menu's own handler :95106 without going near SetBranch, and a planet click
 * pushes the REV list into the branch it is already in.
 *
 * The groups below are the ones the header already lights up as one option.
 */
unsigned MenuBranchPage(unsigned page) {
    if (page == 1 || page == 17 || page == 18) { return 2; }
    if (page == 16 || page == 19 || page == 21 || page == 22 || page == 23) { return 0; }
    if (page == 8 || page == 9 || page == 11) { return 6; }
    if (page == 13) { return 5; }
    if (page == 29) { return 4; }
    // CMenuPostGame changes its current view inside the same menu (:165242).
    if (page == 28) { return 27; }
    return page;
}

/** CMenuNavigationBar :143357 binds Header0..16 and InfoCluster0..3.
 * NAVBAR_MAIN is extracted from native statics; all art/layout/timing is BIG. */
int GameMenu::Header(const CProfileManager &profile, const CPlayerProgress &progress, unsigned currentPage) {
    const unsigned ordinal = movies.Ordinal("GLU_MOVIE_HEADER");
    const CMovie *header = movies.GetMovie(ordinal);
    unsigned idleStart = 0, idleEnd = 0, hideStart = 0, hideEnd = 0;
    if (header == nullptr || !header->GetChapterRange(2, idleStart, idleEnd) ||
        !header->GetChapterRange(3, hideStart, hideEnd)) { return -3; }
    const auto now = clock;
    const bool visible = currentPage < 25;
    unsigned elapsed = 0;
    if (originalHeaderBound) { elapsed = static_cast<unsigned>(std::min<std::uint64_t>(now - originalHeaderTick, 1000)); }
    if (!originalHeaderBound) {
        originalHeaderBound = true;
        navigationVisible = visible;
        originalHeaderTime = 0;
        if (!visible) { originalHeaderTime = hideEnd; }
    } else if (visible != navigationVisible) {
        navigationVisible = visible;
        if (visible) {
            unsigned end = 0;
            if (!header->GetChapterRange(1, originalHeaderTime, end)) { return -3; }
            originalHeaderButtonTime = 0;
        } else { originalHeaderTime = hideStart; }
    }
    originalHeaderTick = now;
    originalHeaderTime += elapsed;
    originalHeaderButtonTime += elapsed;
    if (visible && originalHeaderTime > idleEnd) {
        originalHeaderTime = idleStart + (originalHeaderTime - idleStart) % (idleEnd - idleStart + 1);
    } else if (!visible) { originalHeaderTime = std::min(originalHeaderTime, hideEnd); }
    if (!animateNavigation) {
        originalHeaderTime = hideEnd;
        if (visible) { originalHeaderTime = idleStart; }
        originalHeaderButtonTime = idleStart;
    }
    const unsigned activePage = MenuBranchPage(currentPage);
    // Host page routing only; order and branch IDs are original NAVBAR_MAIN.
    constexpr unsigned branchPages[] = {0, 0, 2, 4, 5, 3, 6, 7};
    int choice = -1;
    class InfoCallback : public IMovieRegionCallback {
    public:
        InfoCallback(GameMenu &menu, const CPlayerProgress &experience) : view(menu), progress(experience) {
            char digits[16];
            std::snprintf(digits, sizeof(digits), "%.3u", progress.GetLevel());
            level = digits;
        }
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index == 0) {
                // Original GetPercentToNextLevel :193337 clamps to 1, including max level.
                const float fraction = std::min(1.0f, static_cast<float>(progress.GetExperienceInLevel()) / progress.GetExperienceDelta());
                // ExperienceCallback VA0xbc3fc passes ARGB 0xff0195d7.
                view.movies.Rectangle(region.x, region.y, std::floor(region.width * fraction), region.height,
                    1.0f / 255, 149.0f / 255, 215.0f / 255, region.alpha);
            } else if (region.index <= 3 && region.index <= level.size()) {
                view.movies.Text(level.substr(region.index - 1, 1), region.x,
                    region.y + region.height / 2 - std::floor(view.movies.TextHeight(7) / 2), 7, 1, 0, region.alpha);
            }
            return true;
        }
        GameMenu &view;
        const CPlayerProgress &progress;
        std::string level;
    } info(*this, progress);
    class HeaderCallback : public IMovieRegionCallback {
    public:
        HeaderCallback(GameMenu &menu, const CProfileManager &account, InfoCallback &cluster,
            unsigned selected, const unsigned *pages, bool touch, int &result) : view(menu), profile(account), info(cluster),
            activePage(selected), branchPages(pages), enabled(touch), choice(result) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index < std::size(kOriginalNavigationBranches)) {
                const unsigned branch = kOriginalNavigationBranches[region.index];
                const auto *entry = OriginalMenuData("MDS_BUTTON_TRUNK", branch - 1);
                if (entry == nullptr) { return false; }
                const CMovie *movie = view.movies.GetMovie(view.movies.Ordinal(entry->movies[0]));
                unsigned start = 0, end = 0;
                if (movie == nullptr || !movie->GetChapterRange(0, start, end)) { return false; }
                unsigned chapter = 2, time = UINT32_MAX;
                if (activePage == branchPages[branch]) { chapter = 3; }
                if (view.originalHeaderButtonTime <= end) { chapter = 0; time = view.originalHeaderButtonTime; }
                MovieRegion origin = region;
                origin.x += region.width / 2;
                origin.y += region.height / 2;
                bool pressed = false;
                if (!DrawOriginalMovieButton(view, *entry, origin, "", 0, enabled, pressed,
                    chapter, view.originalHeaderButtonTime, time)) { return false; }
                if (pressed) { choice = static_cast<int>(region.index); }
            } else if (region.index < 14) {
                const unsigned index = region.index - 7;
                if (index >= std::size(kOriginalNavigationBranches)) { return false; }
                const auto *entry = OriginalMenuData("MDS_BUTTON_TRUNK", kOriginalNavigationBranches[index] - 1);
                if (entry == nullptr) { return false; }
                const auto label = view.movies.NamedString(entry->strings[0]);
                view.movies.Text(label, region.x + region.width / 2 - std::floor(view.movies.TextWidth(label, 1) / 2),
                    region.y, 1, 1, 0, region.alpha);
            } else if (region.index == 14 || region.index == 15) {
                std::uint64_t value = profile.coins;
                if (region.index == 15) { value = profile.warbucks; }
                // Provider78/79 reads the original low 32-bit value; no invented compact notation.
                view.movies.Text(std::to_string(static_cast<std::int32_t>(value)), region.x, region.y, 0, 1, 0, region.alpha);
            } else if (region.index == 16) {
                const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_INFO_CLUSTER");
                const CMovie *movie = view.movies.GetMovie(ordinal);
                if (movie == nullptr || movie->duration == 0) { return false; }
                return view.movies.Draw(ordinal, view.originalHeaderButtonTime % movie->duration,
                    region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &info);
            }
            return true;
        }
        GameMenu &view;
        const CProfileManager &profile;
        InfoCallback &info;
        unsigned activePage;
        const unsigned *branchPages;
        bool enabled;
        int &choice;
    } callback(*this, profile, info, activePage, branchPages,
        visible && originalHeaderTime >= idleStart, choice);
    if (!movies.Draw(ordinal, originalHeaderTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callback)) { return -3; }
    return choice;
}
} // namespace MenuDetail
