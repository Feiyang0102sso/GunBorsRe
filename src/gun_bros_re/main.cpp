#if GB_ENABLE_TESTS
#include "ui/GameMenuStudy.h"
#endif
#include "gun_bros_re/Config.h"
#include "gun_bros_re/DebugKeys.h"
#include "engine/platform/Startup.h"
#include "engine/core/Paths.h"
/** @file main.cpp
 * @brief Windows GUI application. Research tools run in a separate executable.
 */
#define NOMINMAX
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <io.h>
#include "gun_bros_re/ui/GameFrontEnd.h"
#include "engine/platform/CWindow.h"
#include "gun_bros_re/StartupSequence.h"
#include "gun_bros_re/HostSettings.h"
#include "engine/platform/CAudioPlayer.h"

namespace {

int RunApplication(int argc, char **argv) {
    GameCheats::Bind();
    bool skipIntro = false, originalProfile = false, unknown = false;
    unsigned page = 0;
    std::string profile, screenshot;
    std::string big = (Paths::Root() / Paths::BigDirectory).u8string();
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--game") { continue; }
        if (argument == "--mute") { CAudioPlayer::SetMuted(true); continue; }
        if (argument == "--skip-intro") { skipIntro = true; continue; }
        if (argument == "--original-profile") { originalProfile = true; continue; }
        if (argument == "--profile" && index + 1 < argc) { profile = Paths::Resolve(std::filesystem::u8path(argv[++index])).u8string(); continue; }
#if GB_ENABLE_TESTS
        if (argument == "--screenshot" && index + 1 < argc) { screenshot = argv[++index]; continue; }
#endif
        if (argument == "--big" && index + 1 < argc) { big = Paths::Resolve(std::filesystem::u8path(argv[++index])).u8string(); continue; }
#if GB_ENABLE_TESTS
        if (argument == "--menu-page" && index + 1 < argc) { page = static_cast<unsigned>(std::strtoul(argv[++index], nullptr, 10)); continue; }
#endif
        if (argument == "--help" || argument == "-h") {
            std::printf("Gun Bros Windows game\n  --game --mute --skip-intro\n  --profile <directory> --original-profile\n");
            return 0;
        }
        unknown = true;
    }
    if (unknown) { std::fprintf(stderr, "[application] unknown option; use --help\n"); return 2; }
    if (!GameHostSettings().Load(Paths::Root() / GameConfig::Filename)) { return 1; }
    // The original dial is 0..10 and a voice plays at dial x 0.1;
    // CAudioPlayer::SetEffectsGain documents the chain.
    CAudioPlayer::SetEffectsGain(GameHostSettings().effectsVolume * 0.1f);
    // One native surface survives video, loading, menu and gameplay.
    CWindow window;
    if (!window.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
    if (!skipIntro && screenshot.empty()) {
        const int result = RunStartupSequence("", 0, &window);
        if (result != 0) { return result; }
    }
#if GB_ENABLE_TESTS
    return RunGameMenuStudy(big, screenshot, page, originalProfile, profile, &window);
#else
    return RunGameFrontEnd(big, originalProfile, profile, &window);
#endif
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    OpenProductLog(Paths::GameName);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[application] Windows GUI process=%lu console=%u\n", GetCurrentProcessId(), GetConsoleWindow() != nullptr);
    // The Unicode Windows CRT initializes __wargv, not __argv. Convert owned
    // strings before passing the existing UTF-8 runtime entry points.
    Utf8Arguments argv(__argc, __wargv);
    const int result = RunApplication(argv.Count(), argv.Data());
    std::printf("[application] exit=%d\n", result);
    return result;
}
