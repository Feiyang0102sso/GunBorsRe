#include "gun_bros_re/ui/system/CMenuNavigationBar.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"

namespace MenuDetail {
/** CMenuNavigationBar :143357 binds Header0..16 and InfoCluster0..3.
 * NAVBAR_MAIN is extracted from native statics; all art/layout/timing is BIG. */
int CMenuNavigationBar::Draw(ZMenuSurface &view, const CProfileManager &profile, const CPlayerProgress &progress, unsigned currentPage) {
    auto &movies = view.movies;
    const unsigned ordinal = movies.Ordinal("GLU_MOVIE_HEADER");
    const CMovie *header = movies.GetMovie(ordinal);
    unsigned idleStart = 0, idleEnd = 0, hideStart = 0, hideEnd = 0;
    if (header == nullptr || !header->GetChapterRange(2, idleStart, idleEnd) ||
        !header->GetChapterRange(3, hideStart, hideEnd)) { return -3; }
    const auto now = view.clock;
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
        // ShowButtons :143763 starts this chapter now; the preceding frame's
        // elapsed time belongs to the hidden bar, not the new entrance.
        elapsed = 0;
    }
    originalHeaderTick = now;
    originalHeaderTime += elapsed;
    originalHeaderButtonTime += elapsed;
    if (visible && originalHeaderTime > idleEnd) {
        originalHeaderTime = idleStart + (originalHeaderTime - idleStart) % (idleEnd - idleStart + 1);
    } else if (!visible) { originalHeaderTime = std::min(originalHeaderTime, hideEnd); }
    if (!view.animateNavigation) {
        originalHeaderTime = hideEnd;
        if (visible) { originalHeaderTime = idleStart; }
        originalHeaderButtonTime = idleStart;
    }
    navigationReady = visible && originalHeaderTime >= idleStart;
    const unsigned activePage = MenuBranchPage(currentPage);
    // Host page routing only; order and branch IDs are original NAVBAR_MAIN.
    constexpr unsigned branchPages[] = {0, 0, 2, 4, 5, 3, 6, 7};
    int choice = -1;
    class InfoCallback : public ZMovieRegionCallback {
    public:
        InfoCallback(ZMenuSurface &menu, const CPlayerProgress &experience) : view(menu), progress(experience) {
            char digits[16];
            std::snprintf(digits, sizeof(digits), "%.3u", progress.GetLevel());
            level = digits;
        }
        bool DrawMovieRegion(const ZMovieRegion &region) override {
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
        ZMenuSurface &view;
        const CPlayerProgress &progress;
        std::string level;
    } info(view, progress);
    class HeaderCallback : public ZMovieRegionCallback {
    public:
        HeaderCallback(ZMenuSurface &menu, const CProfileManager &account, InfoCallback &cluster,
            unsigned selected, const unsigned *pages, bool touch, int &result, unsigned time) : view(menu), profile(account), info(cluster),
            activePage(selected), branchPages(pages), enabled(touch), choice(result), buttonTime(time) {}
        bool DrawMovieRegion(const ZMovieRegion &region) override {
            if (region.index < std::size(kNavigationBranches)) {
                const unsigned branch = kNavigationBranches[region.index];
                const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_TRUNK", branch - 1);
                if (entry == nullptr) { return false; }
                const CMovie *movie = view.movies.GetMovie(view.movies.Ordinal(entry->movies[0]));
                unsigned start = 0, end = 0;
                if (movie == nullptr || !movie->GetChapterRange(0, start, end)) { return false; }
                unsigned chapter = 2, time = UINT32_MAX;
                if (activePage == branchPages[branch]) { chapter = 3; }
                if (buttonTime <= end) { chapter = 0; time = buttonTime; }
                ZMovieRegion origin = region;
                origin.x += region.width / 2;
                origin.y += region.height / 2;
                bool pressed = false;
                if (!CMenuMovieButton::DrawFrame(view, *entry, origin, "", 0, enabled, pressed,
                    chapter, buttonTime, time)) { return false; }
                if (pressed) { choice = static_cast<int>(region.index); }
            } else if (region.index < 14) {
                const unsigned index = region.index - 7;
                if (index >= std::size(kNavigationBranches)) { return false; }
                const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_TRUNK", kNavigationBranches[index] - 1);
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
                return view.movies.Draw(ordinal, buttonTime % movie->duration,
                    region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &info);
            }
            return true;
        }
        ZMenuSurface &view;
        const CProfileManager &profile;
        InfoCallback &info;
        unsigned activePage;
        const unsigned *branchPages;
        bool enabled;
        int &choice;
        unsigned buttonTime;
    } callback(view, profile, info, activePage, branchPages,
        navigationReady, choice, originalHeaderButtonTime);
    if (!movies.Draw(ordinal, originalHeaderTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callback)) { return -3; }
    return choice;
}
} // namespace MenuDetail
