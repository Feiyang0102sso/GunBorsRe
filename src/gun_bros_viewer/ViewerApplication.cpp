/** Launcher only: select a scene and pass presentation options. */
#include "gun_bros_viewer/ViewerApplication.h"
#include "gun_bros_viewer/ViewerMenu.h"
#include "gun_bros_viewer/scenes/MapPreview.h"
#include "gun_bros_viewer/scenes/MeshPreview.h"
#include "gun_bros_viewer/scenes/EnemyPreview.h"
#include "gun_bros_viewer/scenes/ArenaPreview.h"
#include "gun_bros_viewer/scenes/ResourceInfo.h"
#include "gun_bros_re/debug/DebugKeys.h"
#include "gun_bros_viewer/ViewerSettings.h"
#include "engine/core/ZPaths.h"
#include "engine/platform/ZAudioPlayer.h"
#include <charconv>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
enum class View { Menu, Map, Mesh, Enemy, Weapon, Armor, Arena };

struct Options {
    View view = View::Menu;
    std::string bigDirectory = (Paths::Root() / Paths::BigDirectory).u8string();
    std::string mapPack;
    std::string screenshot;
    int index = 0;
    int gun = 0;
    int armor = -1;
    int state = -1;
    int frame = 0;
    int advance = 0;
    bool fire = false;
    bool collisions = false;
    bool spawns = false;
};

bool ReadIndex(const char *text, int &value) {
    const char *end = text + std::strlen(text);
    const auto result = std::from_chars(text, end, value);
    return result.ec == std::errc() && result.ptr == end && value >= 0;
}

int RunView(const Options &options) {
    switch (options.view) {
    case View::Map:
        return RunMapPreview(options.bigDirectory, options.mapPack, options.index,
            options.screenshot, options.advance, options.spawns, options.collisions, ZMapViewMode::Preview);
    case View::Mesh:
        return RunMeshPreview(options.bigDirectory, options.index, 0, options.frame, options.screenshot, options.advance);
    case View::Enemy:
        return RunEnemyPreview(options.bigDirectory, options.index, 0, options.screenshot, options.advance, -1, true, options.state);
    case View::Weapon:
        return RunPlayerEquipmentPreview(options.bigDirectory, options.index, 0, options.screenshot, options.advance, options.fire);
    case View::Armor:
        return RunPlayerEquipmentPreview(options.bigDirectory, options.gun, 0, options.screenshot, options.advance, options.fire, options.index);
    case View::Arena:
        return RunArena(options.bigDirectory, options.index, options.gun, options.screenshot,
            options.advance, options.fire, options.collisions, options.armor);
    default:
        return 0;
    }
}
}

int RunViewerApplication(int argc, char **argv) {
    Options options;
    std::filesystem::path configPath = Paths::Root() / Paths::ConfigFilename(Paths::ViewerName);
    for (int argument = 1; argument < argc; ++argument) {
        const std::string name = argv[argument];
        if (name == "--help" || name == "-h") { PrintUsage(); return 0; }
        if (name == "--mute") { ZAudioPlayer::SetMuted(true); continue; }
        if (name == "--fire") { options.fire = true; continue; }
        if (name == "--collisions") { options.collisions = true; continue; }
        if (name == "--spawns") { options.spawns = true; continue; }
        // Kept only as a spelling for opening the new menu, without old modes.
        if (name == "--viewer") { continue; }
        View selected = View::Menu;
        if (name == "--map") { selected = View::Map; }
        else if (name == "--mesh") { selected = View::Mesh; }
        else if (name == "--enemy") { selected = View::Enemy; }
        else if (name == "--weapon") { selected = View::Weapon; }
        else if (name == "--armor") { selected = View::Armor; }
        else if (name == "--arena") { selected = View::Arena; }
        if (selected != View::Menu) {
            if (options.view != View::Menu) {
                std::printf("[viewer] choose only one view\n"); return 1;
            }
            options.view = selected;
            if (argument + 1 < argc && argv[argument + 1][0] != '-') {
                if (selected == View::Map) {
                    options.mapPack = argv[++argument];
                    if (argument + 1 >= argc) { std::printf("[viewer] map requires pack and index\n"); return 1; }
                }
                if (!ReadIndex(argv[++argument], options.index)) {
                    std::printf("[viewer] invalid index\n"); return 1;
                }
            }
            continue;
        }
        if (name == "--big" && argument + 1 < argc) {
            options.bigDirectory = Paths::Resolve(std::filesystem::u8path(argv[++argument])).u8string();
            continue;
        }
        if (name == "--config" && argument + 1 < argc && argv[argument + 1][0] != '-') {
            configPath = Paths::Resolve(std::filesystem::u8path(argv[++argument]));
            continue;
        }
#if GB_ENABLE_CAPTURE
        if (name == "--screenshot" && argument + 1 < argc) {
            options.screenshot = Paths::Resolve(std::filesystem::u8path(argv[++argument])).u8string();
            continue;
        }
#endif
        int *value = nullptr;
        if (name == "--gun") { value = &options.gun; }
        else if (name == "--equipment") { value = &options.armor; }
        else if (name == "--state") { value = &options.state; }
        else if (name == "--frame") { value = &options.frame; }
#if GB_ENABLE_CAPTURE
        else if (name == "--advance") { value = &options.advance; }
#endif
        if (value != nullptr && argument + 1 < argc && ReadIndex(argv[++argument], *value)) { continue; }
        std::printf("[viewer] invalid or incomplete option: %s\n", name.c_str());
        return 1;
    }

    GameCheats::Bind();
    if (!GetViewerSettings().Load(configPath)) { return 1; }
    ZAudioPlayer::SetEffectsGain(GetViewerSettings().effectsVolume * 0.1f);
    ZBigVersion version = ZBigVersion::Unknown;
    if (!DetectViewerBigVersion(options.bigDirectory, version)) { return 1; }
    if (options.view != View::Menu) { return RunView(options); }
    for (;;) {
        const int choice = PromptForViewer();
        if (choice == 0) { return 0; }
        Options selected = options;
        selected.view = static_cast<View>(choice);
        const int result = RunView(selected);
        if (result != 0) { std::printf("[viewer] scene failed: %d\n", result); }
    }
}
