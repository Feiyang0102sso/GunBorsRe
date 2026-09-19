#include "gun_bros_re/ui/host/ZStoreRegionClip.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuMissionInfo.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
namespace MenuDetail {
class ZMissionInfoCallbacks : public ZMovieRegionCallback {
public:
    ZMissionInfoCallbacks(ZMenuSurface &menu, CMenuSystem &menuState) : view(menu), state(menuState) {}
    bool DrawMovieRegion(const ZMovieRegion &region) override {
        if (region.index == 0) {
            view.planets.DrawPlanetOriginal(state.planet, region);
            // PlanetImageCallback :188957 draws shared movie 3 after the planet.
            // OnShow starts at zero, then loops chapter 1 (CMenuSystem::Init :97428).
            const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_RADIAL_WIDGET");
            const CMovie *radial = view.movies.GetMovie(ordinal);
            unsigned start = 0, end = 0;
            if (radial == nullptr || !radial->GetChapterRange(1, start, end)) { return false; }
            unsigned time = state.missions.missionTime;
            if (time > end) { time = start + (time - start) % (end - start + 1); }
            return view.movies.Draw(ordinal, time, region.x + region.width / 2, region.y + region.height / 2,
                kMenuWidth, kMenuHeight, 0, region.alpha);
        }
        if (region.index != 1) { return true; }
        view.movies.Rectangle(region.x, region.y, region.width, region.height, 0, 0, 0, region.alpha * 0.5f);
        const float lineHeight = view.movies.TextHeight(0);
        const auto lines = CTextBox::Format(view.movies, view.planets.descriptions[state.planet], region.width, {0, 0, 0, 0, 0});
        float bodyHeight = 0;
        for (const auto &line : lines) { bodyHeight += line.height; }
        const float y = region.y + (region.height - bodyHeight - lineHeight * 2) / 2;
        view.movies.Text(view.planets.names[state.planet], region.x + (region.width - view.movies.TextWidth(view.planets.names[state.planet], 0)) / 2,
            y, 0, 1, 0, region.alpha);
        ZMovieRegion body = region;
        body.y = y + lineHeight * 2;
        body.height = bodyHeight;
        DrawMissionText(view, body, view.planets.descriptions[state.planet], 0, true);
        return true;
    }
    ZMenuSurface &view;
    CMenuSystem &state;
};

class ZMissionWaveCallbacks : public ZMovieRegionCallback {
public:
    ZMissionWaveCallbacks(ZMenuSurface &menu, CMenuSystem &menuState, const CProfileManager &account,
        unsigned missionIndex, bool allowTouch, bool &launch) : view(menu), state(menuState), profile(account), index(missionIndex),
        touchEnabled(allowTouch), launched(launch) {}
    bool DrawMovieRegion(const ZMovieRegion &region) override {
        if (region.index == 0) { return true; }
        const auto &planet = view.planets.planetEntries[state.planet];
        const auto &mission = planet.missions[index];
        const auto &info = planet.missionInfo[index];
        const unsigned page = state.missions.wavePage + region.index - 1;
        const unsigned progress = NativeMissionProgress(profile, mission.level);
        const bool locked = IsMissionLocked(profile, mission, info);
        ZMovieRegion tabGraphic, scrollbar;
        const auto *tab = CMenuDataProvider::Find("MDS_BUTTON_MISSION_INFO", mission.type);
        if (tab == nullptr || !view.movies.Region(view.movies.Ordinal(tab->movies[0]), 1, 0, tabGraphic) ||
            !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_SCROLLBAR_HORIZ"), 0, 0, scrollbar)) { return false; }
        // The original reads first-tab height at mem+202, not wave-button height.
        const unsigned rowStep = static_cast<unsigned>(region.height + tabGraphic.height - scrollbar.height) / 3;
        for (unsigned cell = 0; cell < 10; ++cell) {
            const unsigned localWave = page * 10 + cell;
            if (localWave >= info.waveCount) { break; }
            const unsigned wave = mission.value64 + localWave;
            unsigned status = 0;
            if (!locked && wave <= progress) {
                status = 1;
                if (state.planet < profile.perfectedWaves.size() && wave < profile.perfectedWaves[state.planet].size() &&
                    profile.perfectedWaves[state.planet].test(wave)) { status = 2; }
            }
            const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_MISSION_WAVE", status);
            if (entry == nullptr) { return false; }
            ZMovieRegion position = region;
            position.x += (cell % 5 + 1) * std::floor(region.width / 6);
            position.y += rowStep * (cell / 5 + 1) - tabGraphic.height;
            std::string label;
            if (status != 0) { label = std::to_string(localWave + 1); }
            bool pressed = false;
            if (!CMenuMovieButton::DrawFrame(view, *entry, position, label, 6,
                touchEnabled && status != 0 && !state.missions.missionWaveMoving && state.mode.modePhase == 2, pressed)) { return false; }
            if (pressed) {
                state.selectedMission = planet.data.missions[index];
                state.starMap.startingWave = wave;
                state.missions.revolution = index;
                launched = true;
            }
        }
        return true;
    }
    ZMenuSurface &view;
    CMenuSystem &state;
    const CProfileManager &profile;
    unsigned index;
    bool touchEnabled;
    bool &launched;
};

class ZMissionCardCallbacks : public ZMovieRegionCallback {
public:
    ZMissionCardCallbacks(ZMenuSurface &menu, CMenuSystem &menuState, const CProfileManager &account,
        unsigned missionIndex, bool hasFocus, bool &launch) : view(menu), state(menuState), profile(account),
        index(missionIndex), focused(hasFocus), launched(launch) {}
    bool DrawMovieRegion(const ZMovieRegion &region) override {
        const auto &planet = view.planets.planetEntries[state.planet];
        const auto &mission = planet.missions[index];
        const auto &info = planet.missionInfo[index];
        const bool locked = IsMissionLocked(profile, mission, info);
        if (region.index == 5) { view.movies.Text(info.title, region.x, region.y, 0, 1, 0, region.alpha); }
        if (region.index == 4 && (mission.type == 1 || mission.type == 2)) {
            unsigned animation = 24 + mission.value66; // Provider25 sprite3 :149869.
            if (mission.type == 2) { animation = 42 + mission.value66; }
            view.movies.DrawSprite(5, animation, state.missions.missionTime, region.x + region.width / 2, region.y + region.height / 2, 1, region.alpha);
            if (locked) { view.movies.DrawSprite(5, 19, state.missions.missionTime, region.x + region.width / 2, region.y + region.height / 2, 1, region.alpha); }
        }
        // CornerCallback is BX LR in the original (VA 0x10f26c).
        if (!focused || state.missions.missionClosing) { return true; }
        if (region.index == 7 && region.alpha > 0) {
            unsigned tabs[3] = {mission.type, 3, 4};
            ZMovieRegion buttonAreas[3];
            float total = 8; // ButtonCallback :189858 includes two native 4px gaps.
            for (unsigned tab = 0; tab < 3; ++tab) {
                const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_MISSION_INFO", tabs[tab]);
                if (entry == nullptr || !view.movies.Region(view.movies.Ordinal(entry->movies[0]), 1, 0, buttonAreas[tab])) { return false; }
                total += buttonAreas[tab].width;
            }
            float x = region.x + (region.width - total) / 2;
            for (unsigned tab = 0; tab < 3; ++tab) {
                CMenuDataProvider::Entry entry = *CMenuDataProvider::Find("MDS_BUTTON_MISSION_INFO", tabs[tab]);
                if (state.missions.missionTab != tab && entry.sprites[1] != 0) { entry.sprites[0] = entry.sprites[1]; }
                ZMovieRegion position = region;
                position.x = x;
                bool pressed = false;
                // CMenuOptionGroup::Init :175044 assigns font 5 to slot 1.
                unsigned chapter = 2;
                if (state.missions.missionTab == tab) { chapter = 3; }
                if (!CMenuMovieButton::DrawFrame(view, entry, position, view.movies.NamedString(entry.strings[0]), 5,
                    true, pressed, chapter, state.missions.missionTime)) { return false; }
                if (pressed) { state.missions.missionTab = tab; }
                x += buttonAreas[tab].width + 4;
            }
        }
        if (region.index != 8 || region.alpha == 0) { return true; }
        if (state.missions.missionTab == 1 || state.missions.missionTab == 2) {
            ZMovieRegion body = region;
            body.y += view.movies.TextHeight(1); // DetailCallback :190033.
            body.height -= view.movies.TextHeight(1);
            if (state.missions.missionTab == 1) { DrawMissionText(view, body, info.description, 0); }
            else { DrawMissionText(view, body, info.requirements, 0); }
            return true;
        }
        if (mission.type == 2) {
            const unsigned animation = 42 + mission.value66;
            ZMovieRegion sprite;
            if (!view.movies.SpriteBounds(5, animation, sprite)) { return false; }
            view.movies.DrawSprite(5, animation, state.missions.missionTime, region.x + sprite.width / 2,
                region.y + region.height / 2, 1, region.alpha);
            ZMovieRegion body = region;
            body.x += sprite.width;
            body.width -= sprite.width;
            body.y += (region.height - sprite.height) / 2;
            DrawMissionText(view, body, info.overview, 0);
            // CMissionHighScore::GetHighScore reads collection 1016, keyed by Mission.
            unsigned highScore = 0;
            CArrayInputStream scores(profile.nativeArchive->records[16].payload);
            const unsigned count = scores.ReadUInt32();
            for (unsigned item = 0; item < count; ++item) {
                const unsigned hash = scores.ReadUInt32(), ordinal = scores.ReadUInt8(), type = scores.ReadUInt8();
                scores.Skip(4); // Serialized key is two bytes wider than mem+0.
                const unsigned score = scores.ReadUInt32();
                const auto &ref = planet.data.missions[index];
                if (hash == ref.packHash && ordinal == ref.localIndex && type == 9) { highScore = score; }
            }
            const std::string label = view.movies.NamedString("IDS_MISSION_HIGH_SCORE") + std::to_string(highScore);
            view.movies.Text(label, region.x + (region.width - view.movies.TextWidth(label, 0)) / 2,
                region.y + view.movies.TextHeight(0) / 2, 0, 1, 0, region.alpha);
            if (!locked) {
                const auto *play = CMenuDataProvider::Find("MDS_BUTTON_PLAY", 0);
                ZMovieRegion graphic;
                if (play == nullptr || !view.movies.Region(view.movies.Ordinal(play->movies[0]), 1, 0, graphic)) { return false; }
                ZMovieRegion position = region;
                position.x = body.x + (body.width - graphic.width) / 2;
                position.y = region.y + region.height - graphic.height;
                bool pressed = false;
                if (!CMenuMovieButton::DrawFrame(view, *play, position, view.movies.NamedString(play->strings[0]), 6, true, pressed)) { return false; }
                if (pressed) { state.hordeStart = index; state.selectedMission = planet.data.missions[index]; launched = true; }
            }
            return true;
        }
        if (mission.type != 1 || info.waveCount == 0) { return false; }
        const unsigned pageCount = (info.waveCount + 9) / 10;
        const unsigned waveOrdinal = view.movies.Ordinal("GLU_MOVIE_WAVE_SELECT");
        const CMovie *waveMovie = view.movies.GetMovie(waveOrdinal);
        if (waveMovie == nullptr) { return false; }
        unsigned waveStart = 0, waveEnd = 0;
        ZMovieRegion waveFirst, waveNext;
        if (!waveMovie->GetChapterRange(1, waveStart, waveEnd) ||
            !view.movies.Region(waveOrdinal, 1, waveStart, waveFirst) ||
            !view.movies.Region(waveOrdinal, 2, waveStart, waveNext)) { return false; }
        if (!ScrollMissionMovie(view, state.missions.waveMotion, state.missions.wavePosition, *waveMovie, region,
            waveNext.x - waveFirst.x, pageCount - 1, focused && !state.missions.missionClosing && state.mode.modePhase == 2,
            state.missions.wavePage, state.missions.missionWaveTime, state.missions.missionWaveMoving)) { return false; }
        {
            ZStoreRegionClip clip(view, region);
            ZMissionWaveCallbacks callbacks(view, state, profile, index, view.MouseIn(region.x, region.y, region.width, region.height), launched);
            if (!view.movies.Draw(waveOrdinal, state.missions.missionWaveTime, region.x, region.y, kMenuWidth, kMenuHeight,
                0, region.alpha, &callbacks)) { return false; }
        }
        ZMovieRegion scrollbar;
        if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_SCROLLBAR_HORIZ"), 0, 0, scrollbar)) { return false; }
        // Native scrollbar :190079 receives GetOptionProgress over pageCount.
        const unsigned scrollOrdinal = view.movies.Ordinal("GLU_MOVIE_SCROLLBAR_HORIZ");
        const CMovie *bar = view.movies.GetMovie(scrollOrdinal);
        if (bar == nullptr) { return false; }
        unsigned barTime = 0;
        if (pageCount > 1) { barTime = static_cast<unsigned>(bar->duration * state.missions.wavePosition / ((waveNext.x - waveFirst.x) * (pageCount - 1))); }
        return view.movies.Draw(scrollOrdinal, barTime, region.x + (region.width - scrollbar.width) / 2, region.y + region.height - 4);
    }
    ZMenuSurface &view;
    CMenuSystem &state;
    const CProfileManager &profile;
    unsigned index;
    bool focused;
    bool &launched;
};

class ZMissionListCallbacks : public ZMovieRegionCallback {
public:
    ZMissionListCallbacks(ZMenuSurface &menu, CMenuSystem &menuState, const CProfileManager &account, bool &launch)
        : view(menu), state(menuState), profile(account), launched(launch) {}
    bool DrawMovieRegion(const ZMovieRegion &region) override {
        if (region.index == 0) { return true; }
        const unsigned index = state.missions.missionFirst + region.index - 1;
        if (index >= view.planets.planetEntries[state.planet].missions.size() || state.missions.missionFocused == static_cast<int>(index)) { return true; }
        ZMissionCardCallbacks card(view, state, profile, index, false, launched);
        const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_MISSION_BOX");
        const CMovie *movie = view.movies.GetMovie(ordinal);
        unsigned start = 0, end = 0;
        if (movie == nullptr || !movie->GetChapterRange(0, start, end) ||
            !view.movies.Draw(ordinal, std::min(state.missions.missionTime, end), region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &card)) { return false; }
        ZMovieRegion viewport;
        if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_MISSION_LIST"), 0, state.missions.missionListTime, viewport)) { return false; }
        if (state.missions.missionFocused < 0 && !state.missions.missionListMoving && state.mode.modePhase == 2 &&
            view.MouseIn(viewport.x, viewport.y, viewport.width, viewport.height) && view.Hit(region.x, region.y, region.width, region.height)) {
            state.missions.missionFocused = index;
            state.missions.missionClosing = false;
            state.missions.missionCardTime = end;
            state.missions.missionFocusTime = 0;
            state.missions.missionFocusX = region.x + region.width / 2;
            state.missions.missionFocusY = region.y + region.height / 2;
            state.missions.missionTab = 0;
            state.missions.missionScroll = 0;
            state.missions.missionWaveTime = 0;
            state.missions.missionWaveMoving = false;
            state.missions.waveMotion = ZMenuScrollMotion{};
            state.missions.wavePosition = 0;
            const auto &mission = view.planets.planetEntries[state.planet].missions[index];
            const unsigned progress = NativeMissionProgress(profile, mission.level);
            state.missions.wavePage = 0;
            if (mission.type == 1 && progress > mission.value64) { state.missions.wavePage = (progress - mission.value64) / 10; }
            const unsigned count = view.planets.planetEntries[state.planet].missionInfo[index].waveCount;
            if (count != 0) { state.missions.wavePage = std::min(state.missions.wavePage, (count - 1) / 10); }
        }
        return true;
    }
    ZMenuSurface &view;
    CMenuSystem &state;
    const CProfileManager &profile;
    bool &launched;
};
// Page callback implementations.

bool IsMissionLocked(const CProfileManager &profile, const Mission &mission, const MenuDetail::CMenuMission::MissionInfo &info) {
    if (!mission.script.IsPresent()) { return false; }
    CPlayerProgress progress;
    progress.Bind(profile.nativeArchive->progression);
    progress.SetExperience(profile.experience);
    if (info.requiredLevel > static_cast<int>(progress.GetLevel())) { return true; }
    for (const auto &requirement : info.prerequisites) {
        if (requirement.type == 7 && (mission.type == 1 || mission.type == 2) &&
            NativeMissionProgress(profile, requirement.object) < mission.value64) { return true; }
        if (requirement.type == 22) {
            // Mission::IsLocked checks original purchase collection type 22.
            CArrayInputStream input(profile.nativeArchive->records[2].payload);
            const unsigned count = input.ReadUInt32();
            bool found = false;
            for (unsigned index = 0; index < count; ++index) {
                const unsigned hash = input.ReadUInt32(), ordinal = input.ReadUInt8(), type = input.ReadUInt8();
                input.Skip(2);
                const unsigned quantity = input.ReadUInt8();
                input.Skip(1);
                if (type == 22 && hash == requirement.object.packHash && ordinal == requirement.object.localIndex && quantity != 0) { found = true; }
            }
            if (!found) { return true; }
        }
    }
    return false;
}

void DrawMissionText(ZMenuSurface &view, const ZMovieRegion &region, const std::string &text, unsigned font, bool centered ) {
    const auto lines = CTextBox::Format(view.movies, text, region.width, {font, font, font, font, font});
    float y = region.y;
    ZStoreRegionClip clip(view, region);
    for (const auto &line : lines) {
        float x = region.x;
        if (centered) { x += (region.width - line.width) / 2; }
        for (const auto &run : line.runs) { view.movies.Text(run.text, x + run.x, y, run.font, 1, 0, region.alpha); }
        y += line.height;
    }
}

/** A page transition is the authored chapter 1; chapter 2 is the next page's
 * matching pose. The wheel is a Windows adapter for one native page gesture.
 * Historical note above described the former one-page adapter. Continuous
 * control now maps arbitrary positions onto those same authored poses.
 */
/** Map continuous scroll onto the original repeating Movie chapter. */
bool ScrollMissionMovie(ZMenuSurface &view, ZMenuScrollMotion &motion, float &position,
    const CMovie &movie, const ZMovieRegion &viewport, float stride, unsigned maximum,
    bool enabled, unsigned &page, unsigned &time, bool &moving) {
    unsigned start = 0, end = 0, next = 0, nextEnd = 0;
    if (!movie.GetChapterRange(1, start, end) || !movie.GetChapterRange(2, next, nextEnd) || stride <= 0 || next <= start) { return false; }
    // Saved progress and research jumps choose a whole option at rest.
    if (!motion.captured && motion.velocity == 0 && time == start && page != static_cast<unsigned>(position / stride)) {
        position = page * stride;
    }
    const bool opening = time < start;
    view.Scroll(motion, position, viewport, enabled && !opening, maximum * stride, stride, next - start);
    if (opening) { moving = false; return true; }
    page = static_cast<unsigned>(position / stride);
    time = start + static_cast<unsigned>((position / stride - page) * (next - start));
    moving = motion.captured || motion.velocity != 0;
    return true;
}

/** MENU_MISSION_DETAIL: main48/list49/box50; original callbacks and references. */
bool CMenuMissionInfo::Draw(ZMenuSurface &view, CMenuSystem &state, const CProfileManager &profile, bool &launched) {
    launched = false;
    const unsigned mainOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_MENU");
    const unsigned listOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_LIST");
    const unsigned cardOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_BOX");
    const CMovie *main = view.movies.GetMovie(mainOrdinal), *list = view.movies.GetMovie(listOrdinal), *card = view.movies.GetMovie(cardOrdinal);
    unsigned start = 0, mainEnd = 0, listStart = 0, listEnd = 0, cardStart = 0, cardEnd = 0;
    if (main == nullptr || list == nullptr || card == nullptr || !main->GetChapterRange(0, start, mainEnd) ||
        !list->GetChapterRange(1, listStart, listEnd) || !card->GetChapterRange(1, cardStart, cardEnd)) { return false; }
    if (!state.missions.missionBound) {
        state.missions.missionBound = true;
        state.missions.missionLastTick = view.clock;
        state.missions.missionTime = 0;
        state.missions.missionListTime = 0;
        state.missions.missionFirst = 0;
        state.missions.missionListMoving = false;
        state.missions.missionFocused = -1;
        state.missions.missionScroll = 0;
        state.missions.missionPosition = 0;
        state.missions.missionMotion = ZMenuScrollMotion{};
        if (!view.animateNavigation) { state.missions.missionTime = mainEnd; state.missions.missionListTime = listStart; }
    }
    const unsigned elapsed = static_cast<unsigned>(view.clock - state.missions.missionLastTick);
    state.missions.missionLastTick = view.clock;
    state.missions.missionTime += elapsed;
    if (state.missions.missionListTime < listStart) { state.missions.missionListTime = std::min(listStart, state.missions.missionListTime + elapsed); }
    const CMovie *waveMovie = view.movies.GetMovie(view.movies.Ordinal("GLU_MOVIE_WAVE_SELECT"));
    unsigned waveStart = 0, waveEnd = 0;
    if (waveMovie == nullptr || !waveMovie->GetChapterRange(1, waveStart, waveEnd)) { return false; }
    if (state.missions.missionWaveTime < waveStart) { state.missions.missionWaveTime = std::min(waveStart, state.missions.missionWaveTime + elapsed); }
    ZMissionInfoCallbacks info(view, state);
    // CMenuMissionInfo::Init :189710 / Draw :189174 use shared Movie 4.
    // Its Update does not advance this shared star-map instance.
    if (!view.movies.DrawNamed("GLU_MOVIE_MAP_PARALAX_COPY", state.starMap.starTime)) { return false; }
    if (!view.movies.Draw(mainOrdinal, std::min(mainEnd, state.missions.missionTime), 512, 384, kMenuWidth, kMenuHeight, 0, 1, &info)) { return false; }
    ZMovieRegion viewport, firstSlot, nextSlot;
    if (!view.movies.Region(listOrdinal, 0, listStart, viewport) || !view.movies.Region(listOrdinal, 1, listStart, firstSlot) ||
        !view.movies.Region(listOrdinal, 2, listStart, nextSlot)) { return false; }
    const unsigned count = static_cast<unsigned>(view.planets.planetEntries[state.planet].missions.size());
    unsigned maximum = 0;
    if (count > 3) { maximum = count - 3; }
    if (!ScrollMissionMovie(view, state.missions.missionMotion, state.missions.missionPosition, *list, viewport, nextSlot.x - firstSlot.x,
        maximum, state.missions.missionFocused < 0 && state.mode.modePhase == 2, state.missions.missionFirst, state.missions.missionListTime, state.missions.missionListMoving)) { return false; }
    {
        ZStoreRegionClip clip(view, viewport);
        ZMissionListCallbacks callbacks(view, state, profile, launched);
        if (!view.movies.Draw(listOrdinal, state.missions.missionListTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callbacks)) { return false; }
    }
    if (state.missions.missionFocused >= 0) {
        state.missions.missionFocusTime = std::min(125u, state.missions.missionFocusTime + elapsed);
        if (state.missions.missionClosing) {
            state.missions.missionCardTime = std::max(cardStart, state.missions.missionCardTime);
            state.missions.missionCardTime -= std::min(elapsed * 4, state.missions.missionCardTime - cardStart);
        }
        else { state.missions.missionCardTime = std::min(cardEnd, state.missions.missionCardTime + elapsed * 4); }
        ZMovieRegion bounds;
        if (!view.movies.Region(cardOrdinal, 0, state.missions.missionCardTime, bounds)) { return false; }
        float amount = state.missions.missionFocusTime / 125.0f;
        if (state.missions.missionClosing) { amount = 1 - amount; }
        const float x = state.missions.missionFocusX + (kMenuWidth / 2 - state.missions.missionFocusX) * amount - bounds.width / 2;
        const float y = state.missions.missionFocusY + (kMenuHeight / 2 - state.missions.missionFocusY) * amount - bounds.height / 2;
        ZMissionCardCallbacks callbacks(view, state, profile, state.missions.missionFocused, true, launched);
        if (!view.movies.Draw(cardOrdinal, state.missions.missionCardTime, x, y, kMenuWidth, kMenuHeight, 0, 1, &callbacks)) { return false; }
        if (state.missions.missionClosing && state.missions.missionFocusTime == 125) { state.missions.missionFocused = -1; state.missions.missionClosing = false; }
    }
    ZMovieRegion planetRegion;
    if (!view.movies.Region(mainOrdinal, 0, mainEnd, planetRegion)) { return false; }
    const unsigned backOrdinal = view.movies.Ordinal("GLU_MOVIE_BACK_BUTTON");
    const CMovie *back = view.movies.GetMovie(backOrdinal);
    unsigned backEnd = 0;
    if (back == nullptr || !back->GetChapterRange(0, start, backEnd)) { return false; }
    const float backX = planetRegion.x + planetRegion.width / 2, backY = planetRegion.y + planetRegion.height / 2;
    if (!view.movies.Draw(backOrdinal, std::min(state.missions.missionTime, backEnd), backX, backY)) { return false; }
    // The authored touch-only region has visible=0 at both keyframes. The
    // original button queries it independently of drawing visibility.
    for (const auto &region : view.movies.Regions(backOrdinal, std::min(state.missions.missionTime, backEnd), backX, backY, true)) {
        if (region.index != 0 || !view.Hit(region.x, region.y, region.width, region.height)) { continue; }
        if (state.missions.missionFocused < 0) { state.Navigate(0, true); }
        else { state.missions.missionClosing = true; state.missions.missionFocusTime = 0; }
    }
    return state.mode.Draw(view, state);
}

/** Horizontal revolution/horde cards use the same two-dimensional sprites as iOS. */

/** Returns true only after an unlocked wave/horde is explicitly launched. */

const CGun::Entry *FindMasteryWeapon(const std::vector<CGun::Entry> &weapons, const GameObjectRef &ref) {
    for (const auto &entry : weapons) {
        if (entry.packHash == ref.packHash && entry.ordinal == ref.localIndex) { return &entry; }
    }
    return nullptr;
}
} // namespace MenuDetail
