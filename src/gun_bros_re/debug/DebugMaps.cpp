/** @file DebugMaps.cpp
 * @brief Host research UI. Its layout is desktop-only, not an original Movie.
 * Resource chain: mission_entry.bt / Mission::Init :164402,
 * level_template.bt / CLevel::Template::Init :114770, map.bt / CMap::Init :92498.
 */
#include "gun_bros_re/debug/DebugMaps.h"
#include "gun_bros_re/debug/DebugKeys.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/map/CMap.h"
#include "gun_bros_re/gameplay/CGameSession.h"
#include "gun_bros_re/gameplay/ZSurvivalGameContext.h"
#include "engine/glu/movie/ZMovieRenderer.h"
#include "engine/platform/ZGLLoader.h"
#include <algorithm>
#include <cstdio>

namespace {
// Only host layout values live here. Map identities and relationships come from BIG.
namespace Layout = DebugConfig::Maps;
namespace Labels = DebugConfig::Maps::Text;
bool Contains(const Layout::Rect &rect, float x, float y) {
    return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
}
void DrawText(ZMovieRenderer &renderer, const std::string &text, const DebugConfig::TextStyle &style) {
    renderer.Text(text, style.x, style.y, style.font, style.scale, style.width, style.alpha);
}
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

bool LoadRows(CResTOCManager &toc, ZPackTables &tables, std::vector<MapRow> &rows) {
    std::vector<ZMissionEntry> missions;
    std::vector<LevelEntry> levels;
    if (!LoadMissionCatalog(toc, tables, missions)) { return false; }
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const auto &pack = *toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Level);
        for (unsigned index = 0; index < count; ++index) {
            LevelEntry level;
            level.resource.packHash = pack.GetPackHash();
            level.resource.localIndex = static_cast<std::uint8_t>(index);
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(pack.GetPackHash(), ZGameSection::Level, index, bytes)) { return false; }
            CArrayInputStream input(bytes);
            if (!level.data.Init(input) || input.Available() != 0) { return false; }
            levels.push_back(std::move(level));
        }
    }
    unsigned mapCount = 0;
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const auto &pack = *toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::TileLayer);
        for (unsigned index = 0; index < count; ++index) {
            ++mapCount;
            MapRow base;
            base.selection.pack = pack.GetShortName();
            base.selection.map = index;
            base.label = pack.GetShortName() + Labels::Map + std::to_string(index);
            std::vector<std::uint8_t> bytes;
            CMap map;
            bool valid = tables.ReadSectionResource(pack.GetPackHash(), ZGameSection::TileLayer, index, bytes);
            CArrayInputStream input(bytes);
            if (valid) { valid = map.Init(input) && input.Available() == 0; }
            bool hasPlayer = false;
            if (valid) {
                for (unsigned layer = 0; layer < map.GetObjectLayerCount(); ++layer) {
                    for (const auto &object : map.GetObjectLayer(layer).GetObjects()) {
                        if (object.objectType == static_cast<unsigned>(CLayerObject::ObjectType::Player)) { hasPlayer = true; }
                    }
                }
            }
            base.status = Labels::NoLevel;
            if (!valid) { base.status = Labels::InvalidMap; }
            else if (!hasPlayer) { base.status = Labels::NoPlayer; }
            bool linked = false;
            for (const auto &level : levels) {
                if (level.data.mapRef.packHash != pack.GetPackHash() || level.data.mapRef.localIndex != index) { continue; }
                linked = true;
                MapRow row = base;
                row.selection.level = level.resource;
                row.label += Labels::Separator + tables.GetPackName(level.resource.packHash) + Labels::Level + std::to_string(level.resource.localIndex);
                row.playable = valid && hasPlayer;
                if (row.playable) {
                    row.status = Labels::ScriptPresent;
                    if (!level.data.script.IsPresent()) { row.status = Labels::NoScript; }
                }
                bool hasMission = false;
                for (const auto &mission : missions) {
                    if (!SameRef(mission.data.level, level.resource)) { continue; }
                    hasMission = true;
                    MapRow missionRow = row;
                    missionRow.selection.mission = mission;
                    missionRow.selection.hasMission = true;
                    missionRow.label += Labels::Separator + mission.owner + " " + mission.title;
                    if (mission.data.type == 0) { missionRow.label += Labels::Campaign; }
                    if (mission.data.type == 3) {
                        missionRow.playable = false;
                        missionRow.status = Labels::NoMultiplayer;
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

bool ShowDebugMapPicker(CResTOCManager &toc, ZPackTables &tables, ZWindow &window, DebugMapSelection &selection,
    const std::string &message) {
    std::vector<MapRow> rows;
    if (!LoadRows(toc, tables, rows)) { return false; }
    ZMovieRenderer renderer;
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
        for (ZKeyCode key = window.TakeKeyPress(); key != ZKeyCode::None; key = window.TakeKeyPress()) {
            if (key == GameDebugKeys::MapBack || GameDebugKeys::OpensMapBrowser(key, window)) { return false; }
            if (key == GameDebugKeys::MapPrevious) { --selected; }
            if (key == GameDebugKeys::MapNext) { ++selected; }
            if (key == GameDebugKeys::MapPreviousPage) { selected -= Layout::RowsPerPage; }
            if (key == GameDebugKeys::MapNextPage) { selected += Layout::RowsPerPage; }
            if (key == GameDebugKeys::MapLoad) { load = true; }
        }
        selected -= static_cast<int>(window.TakeWheelDelta());
        selected = std::clamp(selected, 0, static_cast<int>(rows.size()) - 1);
        const int first = selected / Layout::RowsPerPage * Layout::RowsPerPage;
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        if (width <= 0 || height <= 0) { continue; }
        float x = -1, y = -1;
        window.GetMousePosition(x, y);
        x *= DebugConfig::CanvasWidth / width;
        y *= DebugConfig::CanvasHeight / height;
        const bool mouse = window.IsLeftMouseDown();
        const bool clicked = mouse && !previousMouse;
        previousMouse = mouse;
        if (clicked && x >= Layout::List.x && x < Layout::List.x + Layout::List.width && y >= Layout::List.y && y < Layout::List.y + Layout::RowsPerPage * Layout::List.height) {
            const int row = first + static_cast<int>((y - Layout::List.y) / Layout::List.height);
            if (row < static_cast<int>(rows.size())) { selected = row; }
        }
        if (clicked && Contains(Layout::LoadButton, x, y)) { load = true; }
        if (clicked && Contains(Layout::BackButton, x, y)) { return false; }
        if (load && rows[selected].playable) {
            selection = rows[selected].selection;
            selection.ready = true;
            return true;
        }
        glViewport(0, 0, width, height);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        const auto &background = Layout::Background;
        glClearColor(background.red, background.green, background.blue, background.alpha);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        DrawText(renderer, Labels::Title, Layout::Title);
        DrawText(renderer, Labels::Subtitle, Layout::Subtitle);
        DrawText(renderer, Labels::Help, Layout::Help);
        for (int offset = 0; offset < Layout::RowsPerPage && first + offset < static_cast<int>(rows.size()); ++offset) {
            const int index = first + offset;
            const auto &row = rows[index];
            const float top = Layout::List.y + offset * Layout::List.height;
            if (index == selected) {
                const auto &color = Layout::Selection;
                renderer.Rectangle(Layout::List.x, top, Layout::List.width, Layout::List.height,
                    color.red, color.green, color.blue, color.alpha);
            }
            auto style = Layout::Row;
            style.y += top;
            if (!row.playable) { style.alpha = Layout::DisabledAlpha; }
            DrawText(renderer, row.label, style);
        }
        DrawText(renderer, std::to_string(selected + 1) + " / " + std::to_string(rows.size()) + "  " + rows[selected].status, Layout::Status);
        std::string notice = message;
        if (notice.empty()) { notice = Labels::Notice; }
        DrawText(renderer, notice, Layout::Notice);
        const auto &buttonRect = Layout::LoadButton;
        const auto &buttonColor = Layout::Selection;
        renderer.Rectangle(buttonRect.x, buttonRect.y, buttonRect.width, buttonRect.height,
            buttonColor.red, buttonColor.green, buttonColor.blue, buttonColor.alpha);
        std::string button = Labels::Unavailable;
        if (rows[selected].playable) { button = Labels::Load; }
        DrawText(renderer, button, Layout::Load);
        DrawText(renderer, Labels::Back, Layout::Back);
        window.Present();
    }
    return false;
}

CGame::Launch MakeDebugMapLaunch(const std::string &bigDirectory, const DebugMapSelection &selection,
    ZSurvivalGameContext &context) {
    context.persistProgress = false;
    CGame::Launch launch;
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

void RunDebugMaps(const std::string &bigDirectory, ZWindow &window, DebugMapSelection &selection,
    const CProfileManager &profile) {
    while (selection.ready) {
        const DebugMapSelection current = selection;
        selection.ready = false;
        // Reuse the retail equipment path (CBrother::Bind), including mastery,
        // armor and both gun slots. Purchases and pickups affect only this copy.
        CProfileManager previewProfile = profile;
        ZSurvivalGameContext context{previewProfile, {}};
        CGame::Launch launch = MakeDebugMapLaunch(bigDirectory, current, context);
        launch.window = &window;
        launch.debugSelection = &selection;
        std::printf("[debug-maps] launch %s map=%u level=%u:%u\n", current.pack.c_str(), current.map, current.level.packHash, current.level.localIndex);
        const int result = RunSurvival(launch);
        if (result == kDebugMapSessionChoice) { continue; }
        if (result != 0) {
            std::string message = Labels::LoadFailed;
            if (result == kDebugMapSessionComplete) {
                message = Labels::Complete + current.pack + " / MAP " + std::to_string(current.map) + Labels::SelectNext;
                std::printf("[debug-maps] mission complete; returning to map browser\n");
            } else {
                std::printf("[debug-maps] load failed result=%d; returning to map browser\n", result);
            }
            CResTOCManager toc;
            if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return; }
            ZPackTables tables(toc);
            ShowDebugMapPicker(toc, tables, window, selection, message);
        }
    }
}
