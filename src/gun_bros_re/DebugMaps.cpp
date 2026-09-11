/** @file DebugMaps.cpp
 * @brief Host research UI. Its layout is desktop-only, not an original Movie.
 * Resource chain: mission_entry.bt / Mission::Init :164402,
 * level_template.bt / CLevel::Template::Init :114770, map.bt / CMap::Init :92498.
 */
#include "gun_bros_re/DebugMaps.h"
#if GB_ENABLE_TESTS
#include "gun_bros_re/DebugKeys.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "gun_bros_re/gameplay/CMap.h"
#include "gun_bros_re/gameplay/MapScene.h"
#include "gun_bros_re/gameplay/SurvivalGameContext.h"
#include "engine/glu/movie/MovieRenderer.h"
#include "engine/platform/GLLoader.h"
#include <algorithm>
#include <cstdio>

namespace {
// Only host layout values live here. Map identities and relationships come from BIG.
constexpr int kRowsPerPage = 15;
constexpr float kRowTop = 132, kRowHeight = 31;
struct MapRow {
    DebugMapSelection selection;
    std::string label;
    std::string status;
    bool playable = false;
};
struct LevelEntry {
    GameObjectRef resource;
    CLevel::Template data;
};
bool SameRef(const GameObjectRef &a, const GameObjectRef &b) {
    return a.packHash == b.packHash && a.localIndex == b.localIndex;
}

bool LoadRows(CResTOCManager &toc, PackTables &tables, std::vector<MapRow> &rows) {
    std::vector<MissionEntry> missions;
    std::vector<LevelEntry> levels;
    if (!LoadMissionCatalog(toc, tables, missions)) { return false; }
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const auto &pack = *toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(GameSection::Level);
        for (unsigned index = 0; index < count; ++index) {
            LevelEntry level;
            level.resource.packHash = pack.GetPackHash();
            level.resource.localIndex = static_cast<std::uint8_t>(index);
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(pack.GetPackHash(), GameSection::Level, index, bytes)) { return false; }
            CArrayInputStream input(bytes);
            if (!level.data.Init(input) || input.Available() != 0) { return false; }
            levels.push_back(std::move(level));
        }
    }
    unsigned mapCount = 0;
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const auto &pack = *toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(GameSection::TileLayer);
        for (unsigned index = 0; index < count; ++index) {
            ++mapCount;
            MapRow base;
            base.selection.pack = pack.GetShortName();
            base.selection.map = index;
            base.label = pack.GetShortName() + " MAP " + std::to_string(index);
            std::vector<std::uint8_t> bytes;
            CMap map;
            bool valid = tables.ReadSectionResource(pack.GetPackHash(), GameSection::TileLayer, index, bytes);
            CArrayInputStream input(bytes);
            if (valid) { valid = map.Init(input) && input.Available() == 0; }
            bool hasPlayer = false;
            if (valid) {
                for (unsigned layer = 0; layer < map.GetObjectLayerCount(); ++layer) {
                    for (const auto &object : map.GetObjectLayer(layer).GetObjects()) {
                        if (object.objectType == static_cast<unsigned>(PlacedObjectType::Player)) { hasPlayer = true; }
                    }
                }
            }
            base.status = "No linked LEVEL; gameplay script unknown";
            if (!valid) { base.status = "Map parsing failed; original data preserved"; }
            else if (!hasPlayer) { base.status = "No authored player spawn"; }
            bool linked = false;
            for (const auto &level : levels) {
                if (level.data.mapRef.packHash != pack.GetPackHash() || level.data.mapRef.localIndex != index) { continue; }
                linked = true;
                MapRow row = base;
                row.selection.level = level.resource;
                row.label += "  /  " + tables.GetPackName(level.resource.packHash) + " LEVEL " + std::to_string(level.resource.localIndex);
                row.playable = valid && hasPlayer;
                if (row.playable) {
                    row.status = "Script present; full completion unverified";
                    if (!level.data.script.IsPresent()) { row.status = "LEVEL has no script; map exploration only"; }
                }
                bool hasMission = false;
                for (const auto &mission : missions) {
                    if (!SameRef(mission.data.level, level.resource)) { continue; }
                    hasMission = true;
                    MapRow missionRow = row;
                    missionRow.selection.mission = mission;
                    missionRow.selection.hasMission = true;
                    missionRow.label += "  /  " + mission.owner + " " + mission.title;
                    if (mission.data.type == 0) { missionRow.label += " - CAMPAIGN"; }
                    if (mission.data.type == 3) {
                        missionRow.playable = false;
                        missionRow.status = "Multiplayer session is not implemented";
                    }
                    rows.push_back(std::move(missionRow));
                }
                if (!hasMission) { rows.push_back(std::move(row)); }
            }
            if (!linked) { rows.push_back(std::move(base)); }
        }
    }
    std::printf("[debug-maps] maps=%u levels=%zu rows=%zu\n", mapCount, levels.size(), rows.size());
    for (const auto &row : rows) {
        std::printf("[debug-maps] %s playable=%d: %s\n", row.label.c_str(), row.playable, row.status.c_str());
    }
    return !rows.empty();
}
}

bool ShowDebugMapPicker(CResTOCManager &toc, PackTables &tables, CWindow &window, DebugMapSelection &selection,
    const std::string &message) {
    std::vector<MapRow> rows;
    if (!LoadRows(toc, tables, rows)) { return false; }
    MovieRenderer renderer;
    auto &core = *toc.GetPack(toc.GetCorePackIndex());
    if (!renderer.Init(core, core)) { return false; }
    int selected = 0;
    for (int index = 0; index < static_cast<int>(rows.size()); ++index) {
        const auto &candidate = rows[index].selection;
        if (selection.pack.empty()) {
            if (candidate.hasMission && candidate.mission.data.type == 0) { selected = index; break; }
        } else if (candidate.pack == selection.pack && candidate.map == selection.map &&
            SameRef(candidate.level, selection.level) && candidate.hasMission == selection.hasMission &&
            (!candidate.hasMission || SameRef(candidate.mission.resource, selection.mission.resource))) {
            selected = index;
            break;
        }
    }
    bool previousMouse = window.IsLeftMouseDown();
    window.SetEscapeCloses(false);
    // This modal suspends its caller. It owns clicks/keys until accepted or cancelled.
    while (window.PumpEvents()) {
        bool load = false;
        for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None; key = window.TakeKeyPress()) {
            if (key == GameDebugKeys::MapBack || GameDebugKeys::OpensMapBrowser(key, window)) { return false; }
            if (key == GameDebugKeys::MapPrevious) { --selected; }
            if (key == GameDebugKeys::MapNext) { ++selected; }
            if (key == GameDebugKeys::MapPreviousPage) { selected -= kRowsPerPage; }
            if (key == GameDebugKeys::MapNextPage) { selected += kRowsPerPage; }
            if (key == GameDebugKeys::MapLoad) { load = true; }
        }
        selected -= static_cast<int>(window.TakeWheelDelta());
        selected = std::clamp(selected, 0, static_cast<int>(rows.size()) - 1);
        const int first = selected / kRowsPerPage * kRowsPerPage;
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        if (width <= 0 || height <= 0) { continue; }
        float x = -1, y = -1;
        window.GetMousePosition(x, y);
        x *= 1024.0f / width;
        y *= 768.0f / height;
        const bool mouse = window.IsLeftMouseDown();
        const bool clicked = mouse && !previousMouse;
        previousMouse = mouse;
        if (clicked && x >= 24 && x < 1000 && y >= kRowTop && y < kRowTop + kRowsPerPage * kRowHeight) {
            const int row = first + static_cast<int>((y - kRowTop) / kRowHeight);
            if (row < static_cast<int>(rows.size())) { selected = row; }
        }
        if (clicked && y >= 692 && y < 740) {
            if (x >= 24 && x < 240) { load = true; }
            if (x >= 780 && x < 1000) { return false; }
        }
        if (load && rows[selected].playable) {
            selection = rows[selected].selection;
            selection.ready = true;
            return true;
        }
        glViewport(0, 0, width, height);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.035f, 0.055f, 0.08f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderer.Text("DEBUG MAP BROWSER", 24, 24, 0, 0.8f);
        renderer.Text("All BIG maps / linked LEVEL and Mission entries", 24, 68, 1, 0.7f);
        renderer.Text("UP/DOWN select   LEFT/RIGHT page   ENTER load   ESC back", 24, 100, 1, 0.6f);
        for (int offset = 0; offset < kRowsPerPage && first + offset < static_cast<int>(rows.size()); ++offset) {
            const int index = first + offset;
            const auto &row = rows[index];
            const float top = kRowTop + offset * kRowHeight;
            if (index == selected) { renderer.Rectangle(24, top, 976, kRowHeight, 0.12f, 0.3f, 0.4f); }
            float alpha = 0.45f;
            if (row.playable) { alpha = 1; }
            renderer.Text(row.label, 32, top + 5, 1, 0.65f, 955, alpha);
        }
        renderer.Text(std::to_string(selected + 1) + " / " + std::to_string(rows.size()) + "  " + rows[selected].status, 24, 614, 1, 0.65f, 976);
        std::string notice = message;
        if (notice.empty()) { notice = "Current save equipment / no progress saved. Campaign completion is unverified."; }
        renderer.Text(notice, 24, 650, 1, 0.6f, 976);
        renderer.Rectangle(24, 692, 216, 48, 0.12f, 0.3f, 0.4f);
        std::string button = "UNAVAILABLE";
        if (rows[selected].playable) { button = "LOAD MAP"; }
        renderer.Text(button, 40, 704, 1, 0.75f);
        renderer.Text("BACK", 840, 704, 1, 0.75f);
        window.Present();
    }
    return false;
}

SurvivalLaunch MakeDebugMapLaunch(const std::string &bigDirectory, const DebugMapSelection &selection,
    SurvivalGameContext &context) {
    context.persistProgress = false;
    SurvivalLaunch launch;
    launch.bigDirectory = bigDirectory;
    launch.packShortName = selection.pack;
    launch.mapIndex = selection.map;
    launch.gameContext = &context;
    launch.withBrother = context.profile.brotherEnabled;
    launch.debugMap = &selection;
    if (selection.hasMission) {
        launch.archiveMission = &selection.mission;
        launch.startWave = selection.mission.data.value64;
    }
    return launch;
}

void RunDebugMaps(const std::string &bigDirectory, CWindow &window, DebugMapSelection &selection,
    const CProfileManager &profile) {
    while (selection.ready) {
        const DebugMapSelection current = selection;
        selection.ready = false;
        // Reuse the retail equipment path (CBrother::Bind), including mastery,
        // armor and both gun slots. Purchases and pickups affect only this copy.
        CProfileManager previewProfile = profile;
        SurvivalGameContext context{previewProfile, {}};
        SurvivalLaunch launch = MakeDebugMapLaunch(bigDirectory, current, context);
        launch.window = &window;
        launch.debugSelection = &selection;
        std::printf("[debug-maps] launch %s map=%u level=%u:%u\n", current.pack.c_str(), current.map, current.level.packHash, current.level.localIndex);
        const int result = RunSurvival(launch);
        if (result == kDebugMapSessionChoice) { continue; }
        if (result != 0) {
            std::string message = "Map load failed. See GunBrosRe log for the resource and error.";
            if (result == kDebugMapSessionComplete) {
                message = "Mission complete: " + current.pack + " / MAP " + std::to_string(current.map) + ". Select a map to continue.";
                std::printf("[debug-maps] mission complete; returning to map browser\n");
            } else {
                std::printf("[debug-maps] load failed result=%d; returning to map browser\n", result);
            }
            CResTOCManager toc;
            if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return; }
            PackTables tables(toc);
            ShowDebugMapPicker(toc, tables, window, selection, message);
        }
    }
}
#endif
