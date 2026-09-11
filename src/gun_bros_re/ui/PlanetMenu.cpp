#include "gun_bros_re/ui/StoreRegionClip.h"
#include "gun_bros_re/ui/MenuInternal.h"
namespace MenuDetail {

class StarPlanetCallbacks : public IMovieRegionCallback {
public:
    StarPlanetCallbacks(GameMenu &menu, MenuState &state) : view(menu), state(state) {}
    bool DrawMovieRegion(const MovieRegion &region) override {
        if (region.index == 0) { return true; }
        int index = view.PlanetForMapSlot(region.index);
        if (index < 0) { index = view.PlanetForMapSlot(0); }
        if (index < 0) { return false; }
        const float fade = std::min(1.0f, state.starMap.starFadeTime / 350.0f); // OnShow :162368.
        bounds.push_back(view.DrawPlanetThumb(index, region, fade));
        return true;
    }
    GameMenu &view;
    MenuState &state;
    std::vector<MovieRegion> bounds;
};
// Page callback implementations.

/** Original UpdatePosition :161040 advances along the dominant axis. */
void MoveStarPoint(float &x, float &y, float targetX, float targetY, unsigned elapsed, float speed) {
    const float dx = targetX - x, dy = targetY - y;
    const float distance = std::max(std::abs(dx), std::abs(dy));
    if (distance == 0) { return; }
    const float step = std::min(1.0f, speed * elapsed / 1000 / distance);
    x += dx * step;
    y += dy * step;
}

/** CMenuMission :161015..163482, MENU_MISSION_ROOT VA 0x402d38.
 * The map is a bounded Movie timeline, not host coordinates or depth factors. */
bool DrawOriginalStarMap(GameMenu &view, MenuState &state, const CProfileManager &profile) {
    const unsigned mapOrdinal = view.movies.Ordinal("GLU_MOVIE_MAP_PARALAX_COPY");
    const unsigned reticleOrdinal = view.movies.Ordinal("GLU_MOVIE_MAP_RETICLE");
    const unsigned flagOrdinal = view.movies.Ordinal("GLU_MOVIE_PLANET_FLAG");
    const CMovie *map = view.movies.GetMovie(mapOrdinal);
    const CMovie *reticle = view.movies.GetMovie(reticleOrdinal);
    const CMovie *flag = view.movies.GetMovie(flagOrdinal);
    unsigned reticleStart = 0, reticleEnd = 0, closeStart = 0, closeEnd = 0, flagStart = 0, flagEnd = 0;
    if (map == nullptr || reticle == nullptr || flag == nullptr ||
        !reticle->GetChapterRange(1, reticleStart, reticleEnd) || !reticle->GetChapterRange(2, closeStart, closeEnd) ||
        !flag->GetChapterRange(1, flagStart, flagEnd)) { return false; }
    if (!state.starMap.starBound) {
        state.starMap.starBound = true;
        state.starMap.starLastTick = view.clock;
        state.starMap.starFadeTime = 0;
        if (!view.animateNavigation) { state.starMap.starFadeTime = 350; }
        // SetSelectedIndex :162296 queues the first slot for the end of OnShow.
        if (state.starMap.starSelectedSlot < 1) {
            state.starMap.starSelectedSlot = 1;
            state.starMap.starLocked = false;
            unsigned chapterStart = 0, chapterEnd = 0;
            if (!map->GetChapterRange(0, chapterStart, chapterEnd)) { return false; }
            state.starMap.starTargetTime = chapterEnd;
        }
    }
    const unsigned elapsed = static_cast<unsigned>(view.clock - state.starMap.starLastTick);
    state.starMap.starLastTick = view.clock;
    state.starMap.starFadeTime = std::min(350u, state.starMap.starFadeTime + elapsed);
    MovieRegion viewport;
    if (!view.movies.Region(mapOrdinal, 0, state.starMap.starTime, viewport)) { return false; }
    bool interactive = state.page == 0 && state.mode.modeSelected && state.mode.modePhase == 2 && !state.starMap.starEntering && state.starMap.starFadeTime == 350;
    MovieRegion modeButton;
    if (view.movies.Region(view.movies.Ordinal("GLU_MOVIE_MULTIPLAYER_AND_VERSUS_MAP"), state.gameMode * 2 + 1, state.mode.modeTime, modeButton) &&
        view.MouseIn(modeButton.x, modeButton.y, modeButton.width, modeButton.height)) { interactive = false; }
    if (interactive && view.MouseIn(viewport.x, viewport.y, viewport.width, viewport.height)) {
        const float wheel = view.window.TakeWheelDelta();
        if (view.dragX != 0 && elapsed != 0) {
            // Update :162434 caps drag speed at 2; 600 is the native px/sec divisor.
            state.starMap.starSpeed = std::min(2.0f, std::abs(view.dragX) / (elapsed * 0.001f) / 600);
            state.starMap.starReverse = view.dragX > 0;
            state.starMap.starTargetTime = -1;
        } else if (wheel != 0) {
            // Windows wheel selects an adjacent authored chapter boundary.
            unsigned chapter = 0;
            while (chapter + 1 < map->chapters.size() && map->chapters[chapter + 1] <= state.starMap.starTime) { ++chapter; }
            if (wheel < 0 && chapter + 1 < map->chapters.size()) { ++chapter; }
            if (wheel > 0 && chapter > 0) { --chapter; }
            state.starMap.starTargetTime = map->chapters[chapter];
        }
    }
    if (state.starMap.starTargetTime >= 0) {
        const unsigned movement = static_cast<unsigned>(elapsed * 0.75f); // Original state 2.
        const unsigned target = static_cast<unsigned>(state.starMap.starTargetTime);
        if (state.starMap.starTime < target) { state.starMap.starTime = std::min(target, state.starMap.starTime + movement); }
        else { state.starMap.starTime -= std::min(movement, state.starMap.starTime - target); }
        if (state.starMap.starTime == target) { state.starMap.starTargetTime = -1; state.starMap.starSpeed = 0; }
    } else {
        if (view.dragX == 0) {
            const float seconds = elapsed * 0.001f;
            state.starMap.starSpeed = std::max(0.0f, state.starMap.starSpeed - seconds * seconds * 125); // :162520, -dt^2/2*250.
        }
        const unsigned movement = static_cast<unsigned>(state.starMap.starSpeed * elapsed);
        if (state.starMap.starReverse) { state.starMap.starTime -= std::min(state.starMap.starTime, movement); }
        else { state.starMap.starTime = std::min(map->duration, state.starMap.starTime + movement); }
    }
    StarPlanetCallbacks callbacks(view, state);
    if (!view.movies.Draw(mapOrdinal, state.starMap.starTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callbacks)) { return false; }
    // Original hit testing scans slot order, independent of each sprite's layer.
    for (unsigned slot = 1; slot <= map->chapters.size(); ++slot) {
        for (const auto &area : callbacks.bounds) {
            if (area.index != slot) { continue; }
            if (interactive && view.MouseIn(viewport.x, viewport.y, viewport.width, viewport.height) &&
                view.Hit(area.x, area.y, area.width, area.height)) {
                if (state.starMap.starSelectedSlot == static_cast<int>(slot) && state.starMap.starLocked) {
                    state.starMap.starEntering = true;
                    state.starMap.starReticleTime = closeStart;
                    state.starMap.starSpeed = 0;
                } else {
                    state.starMap.starSelectedSlot = slot;
                    state.starMap.starLocked = false;
                    state.starMap.starFlagTime = 0;
                    unsigned chapterStart = 0, chapterEnd = 0;
                    if (!map->GetChapterRange(slot - 1, chapterStart, chapterEnd)) { return false; }
                    state.starMap.starTargetTime = chapterEnd;
                }
            }
            if (state.starMap.starSelectedSlot != static_cast<int>(slot)) { continue; }
            const float centerX = area.x + area.width / 2, centerY = area.y + area.height / 2;
            if (state.starMap.starLocked) { state.starMap.starSelectorX = centerX; state.starMap.starSelectorY = centerY; }
            else {
                MoveStarPoint(state.starMap.starSelectorX, state.starMap.starSelectorY, centerX, centerY, elapsed, kMenuWidth / 480 * 750);
                if (state.starMap.starSelectorX == centerX && state.starMap.starSelectorY == centerY) { state.starMap.starLocked = true; }
            }
        }
    }
    if (state.starMap.starSelectedSlot < 1) { return true; }
    if (state.starMap.starEntering) { state.starMap.starReticleTime = std::min(closeEnd, state.starMap.starReticleTime + elapsed); }
    else {
        state.starMap.starReticleTime += elapsed;
        if (state.starMap.starReticleTime > reticleEnd) { state.starMap.starReticleTime = reticleStart + (state.starMap.starReticleTime - reticleStart) % (reticleEnd - reticleStart + 1); }
    }
    if (!view.movies.Draw(reticleOrdinal, state.starMap.starReticleTime, state.starMap.starSelectorX, state.starMap.starSelectorY)) { return false; }
    // CrossHairsCallback :161422 draws screen-spanning native one-pixel lines.
    for (const auto &region : view.movies.Regions(reticleOrdinal, state.starMap.starReticleTime, state.starMap.starSelectorX, state.starMap.starSelectorY)) {
        if (region.index != 0) { continue; }
        view.movies.Rectangle(region.x + region.width / 2, 0, 1, kMenuHeight, 124.0f / 255, 201.0f / 255, 243.0f / 255, region.alpha * 0.5f);
        view.movies.Rectangle(0, region.y + region.height / 2, kMenuWidth, 1, 124.0f / 255, 201.0f / 255, 243.0f / 255, region.alpha * 0.5f);
    }
    int selected = view.PlanetForMapSlot(state.starMap.starSelectedSlot);
    if (selected < 0) { selected = view.PlanetForMapSlot(0); }
    if (selected < 0) { return false; }
    if (state.starMap.starLocked && !state.starMap.starEntering && state.starMap.starSelectorX >= 0 && state.starMap.starSelectorX <= kMenuWidth) {
        MovieRegion flagBounds, reticleBounds;
        if (!view.movies.Region(flagOrdinal, 0, flagEnd, flagBounds) ||
            !view.movies.Region(reticleOrdinal, 1, state.starMap.starReticleTime, reticleBounds)) { return false; }
        float offsetX = (reticleBounds.width + flagBounds.width) / 2;
        float offsetY = (reticleBounds.height + flagBounds.height) / 2;
        if (state.starMap.starSelectorX > viewport.x + viewport.width / 2) { offsetX = -offsetX; }
        if (state.starMap.starSelectorY > viewport.y + viewport.height / 2) { offsetY = -offsetY; }
        MoveStarPoint(state.starMap.starFlagX, state.starMap.starFlagY, offsetX, offsetY, elapsed, kMenuWidth / 480 * 800);
        state.starMap.starFlagTime = std::min(flagEnd, state.starMap.starFlagTime + elapsed);
        if (!view.movies.Draw(flagOrdinal, state.starMap.starFlagTime, state.starMap.starSelectorX + state.starMap.starFlagX, state.starMap.starSelectorY + state.starMap.starFlagY)) { return false; }
        for (const auto &region : view.movies.Regions(flagOrdinal, state.starMap.starFlagTime, state.starMap.starSelectorX + state.starMap.starFlagX, state.starMap.starSelectorY + state.starMap.starFlagY)) {
            if (region.index == 0) { view.PlanetFlagLines(state.starMap.starSelectorX, state.starMap.starSelectorY, region); }
            CPlayerProgress playerProgress;
            playerProgress.Bind(profile.nativeArchive->progression);
            playerProgress.SetExperience(profile.experience);
            const unsigned requiredLevel = view.planetEntries[selected].data.requiredLevel;
            const bool locked = requiredLevel > playerProgress.GetLevel();
            unsigned titleRegion = 1;
            if (locked) { titleRegion = 2; }
            if (region.index != titleRegion && !(locked && region.index == 3)) { continue; }
            std::string label = view.names[selected];
            if (region.index == 3) {
                // Planet::CreateRequirementString :170158, ARM 0xe9fd8 reads mem+82.
                const std::string format = view.movies.NamedString("IDS_PLANET_REQUIRED_LVL");
                char text[512]{};
                std::snprintf(text, sizeof(text), format.c_str(), requiredLevel);
                label = text;
            }
            MovieRegion textArea = region;
            textArea.x += (textArea.width - flagBounds.width) / 2;
            textArea.y += (textArea.height - flagBounds.height) / 2;
            textArea.width = flagBounds.width;
            textArea.height = flagBounds.height;
            const auto lines = FormatStoreText(view.movies, label, textArea.width, {1, 2, 2, 2, 2});
            float height = 0;
            for (const auto &line : lines) { height += line.height; }
            float y = textArea.y + (textArea.height - height) / 2;
            StoreRegionClip clip(view, textArea);
            for (const auto &line : lines) {
                for (const auto &run : line.runs) { view.movies.Text(run.text, textArea.x + (textArea.width - line.width) / 2 + run.x, y, run.font, 1, 0, region.alpha); }
                y += line.height;
            }
        }
    }
    if (state.starMap.starEntering && state.starMap.starReticleTime == closeEnd) {
        state.starMap.starEntering = false;
        state.planet = selected;
        state.missions.missionScroll = 0;
        state.starMap.startingWave = -1;
        if (!view.planetEntries[selected].missions.empty()) { state.Navigate(21); }
        else { std::printf("[planet-menu] slot=%u has no mission entries\n", state.starMap.starSelectedSlot); }
        state.starMap.starReticleTime = 0;
    }
    return true;
}

/** CMissionWaveStatus collection 1003; the four live retail projections may
 * contain progress that has not reached the next disk checkpoint yet. */
unsigned NativeMissionProgress(const CProfileManager &profile, const GameObjectRef &level) {
    for (unsigned slot = 0; slot < profile.nativeArchive->survivalLevels.size(); ++slot) {
        if (SameObject(level, profile.nativeArchive->survivalLevels[slot])) { return profile.clearedWaves[slot]; }
    }
    const auto &bytes = profile.nativeArchive->records[3].payload;
    CArrayInputStream input(bytes);
    const unsigned count = input.ReadUInt32();
    for (unsigned index = 0; index < count; ++index) {
        const unsigned hash = input.ReadUInt32(), ordinal = input.ReadUInt8(), type = input.ReadUInt8();
        input.Skip(2);
        const unsigned progress = input.ReadUInt16();
        input.Skip(514);
        if (hash == level.packHash && ordinal == level.localIndex && type == 7) { return progress; }
    }
    return 0; // Original collection GetWaveProgress returns zero for absent keys.
}
} // namespace MenuDetail
