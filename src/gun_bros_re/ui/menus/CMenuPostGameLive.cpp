#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
/** Original provider94: two peer groups, six rows; MissionType1 ends in perfect waves.
 * MENU_POST_GAME_WRAPUP_MP / CMenuPostGame::OverviewCallback :164593.
 */
#define NOMINMAX
#include "gun_bros_re/ui/menus/CMenuPostGameOption.h"

namespace MenuDetail {
bool DrawLivePostGameList(ZMenuSurface &view, CMenuSystem &state, const ZMovieRegion &region, bool interactive) {
    const unsigned scroll = view.movies.Ordinal("GLU_MOVIE_WRAPUP_MENU_SCROLL");
    const auto *movie = view.movies.GetMovie(scroll);
    unsigned start = 0, end = 0, next = 0, nextEnd = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, start, end) || !movie->GetChapterRange(2, next, nextEnd)) { return false; }
    int totalTravel = 0;
    for (unsigned index = 1; index <= 3; ++index) {
        ZMovieRegion row, later;
        if (!view.movies.Region(scroll, index, start, row) || !view.movies.Region(scroll, index, next, later)) { return false; }
        totalTravel += static_cast<int>(std::abs(later.y - row.y));
    }
    const float travel = static_cast<float>(totalTravel / 3);
    if (travel == 0) { return false; }
    if (interactive && view.MouseIn(region.x, region.y, region.width, region.height)) {
        state.postGame.livePosition -= view.window.TakeWheelDelta();
        state.postGame.livePosition -= view.dragY / travel;
    }
    float lastPosition = 4;
    if (state.result.deathmatch) { lastPosition = 3; }
    state.postGame.livePosition = std::clamp(state.postGame.livePosition, 0.0f, lastPosition);
    const unsigned first = static_cast<unsigned>(std::floor(state.postGame.livePosition));
    unsigned time = start + static_cast<unsigned>((state.postGame.livePosition - first) * (next - start));
    if (state.postGame.postGameItemTime < start) { time = state.postGame.postGameItemTime; }
    class Rows : public ZMovieRegionCallback {
    public:
        Rows(ZMenuSurface &menu, CMenuSystem &result, unsigned offset) : view(menu), state(result), first(offset) {}
        bool DrawMovieRegion(const ZMovieRegion &area) override {
            if (area.type < 2) { return true; }
            const unsigned row = first + area.type - 2;
            unsigned rowCount = 6;
            if (state.result.deathmatch) { rowCount = 5; }
            if (row >= rowCount) { return true; }
            unsigned icon = row;
            if (row == 5) { icon = 6; }
            if (row == 5 && state.result.horde) { icon = 7; }
            const auto *entry = CMenuDataProvider::Find("MDS_ICON_POSTGAME_MP", icon);
            if (state.result.deathmatch) { entry = CMenuDataProvider::Find("MDS_ICON_POSTGAME_MP_DM", row); }
            const unsigned card = view.movies.Ordinal("GLU_MOVIE_WRAPUP_BOX");
            ZMovieRegion bounds;
            if (entry == nullptr || !view.movies.Region(card, 0, 0, bounds)) { return false; }
            for (unsigned peer = 0; peer < 2; ++peer) {
                const auto &stats = state.result.peers[peer];
                std::uint64_t bonus = stats.perfectWaves;
                if (state.result.horde) { bonus = stats.bestStreak; }
                const std::uint64_t values[] = {stats.kills, stats.assists, stats.deaths, stats.xplodium, stats.experience, bonus};
                const std::uint64_t dmValues[] = {stats.kills, stats.deaths, stats.xplodium, stats.experience, stats.bestStreak};
                std::uint64_t value = values[row];
                if (state.result.deathmatch) { value = dmValues[row]; }
                const std::string text = std::to_string(value);
                CMenuPostGameOption callback(view, *entry, text, state.postGame.postGameIconTime, true);
                float x = area.x;
                if (peer == 1) { x += area.width - bounds.width; }
                const auto *movie = view.movies.GetMovie(card);
                if (movie == nullptr || !view.movies.Draw(card, std::min(state.postGame.postGameItemTime, movie->duration),
                    x, area.y, kMenuWidth, kMenuHeight, 0, area.alpha, &callback)) { return false; }
            }
            return true;
        }
        ZMenuSurface &view; CMenuSystem &state; unsigned first;
    } callback(view, state, first);
    view.Clip(0, region.y, kMenuWidth, region.height);
    const bool drawn = view.movies.Draw(scroll, time, region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &callback);
    view.EndClip();
    return drawn;
}
}
