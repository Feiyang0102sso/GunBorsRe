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
#include "runtime/GameFrontEnd.h"
#include "engine/platform/CWindow.h"
#include "engine/platform/ResearchLauncher.h"
#include "runtime/StartupSequence.h"
#include "runtime/HostSettings.h"
#include "engine/CAudioPlayer.h"

namespace {
/** Retain shell redirection; Explorer launches get a file and no console. */
void OpenGameLog() {
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output != nullptr && output != INVALID_HANDLE_VALUE && GetFileType(output) != FILE_TYPE_UNKNOWN) { return; }
    const auto directory = std::filesystem::path(ASSET_ROOT) / "userdata/logs";
    std::filesystem::create_directories(directory);
    const auto path = directory / ("game-" + std::to_string(GetCurrentProcessId()) + ".log");
    FILE *opened = nullptr;
    if (_wfreopen_s(&opened, path.c_str(), L"w", stdout) == 0) {
        _dup2(_fileno(stdout), _fileno(stderr));
        SetStdHandle(STD_OUTPUT_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout))));
        SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr))));
    }
}


int RunApplication(int argc, char **argv, const wchar_t *arguments) {
    bool skipIntro = false, originalProfile = false, research = false, unknown = false;
    unsigned page = 0;
    std::string profile, screenshot;
    std::string big = std::string(ASSET_ROOT) + "/big";
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--game") { continue; }
        if (argument == "--mute") { CAudioPlayer::SetMuted(true); continue; }
        if (argument == "--skip-intro") { skipIntro = true; continue; }
        if (argument == "--original-profile") { originalProfile = true; continue; }
        if (argument == "--research") { research = true; continue; }
        if (argument == "--profile" && index + 1 < argc) { profile = argv[++index]; continue; }
        if (argument == "--screenshot" && index + 1 < argc) { screenshot = argv[++index]; continue; }
        if (argument == "--big" && index + 1 < argc) { big = argv[++index]; continue; }
        if (argument == "--menu-page" && index + 1 < argc) { page = static_cast<unsigned>(std::strtoul(argv[++index], nullptr, 10)); continue; }
        if (argument == "--help" || argument == "-h") {
            std::printf("Gun Bros Windows game\n  --game --mute --skip-intro\n  --profile <directory> --original-profile\n  --menu-page <0..29> --screenshot <file.png>\n  --research opens the permanent companion tools\nOther research flags are forwarded to gun_bros_research.exe.\n");
            return 0;
        }
        unknown = true;
    }
    if (research || unknown) { return LaunchResearchTools(arguments, research); }
    if (!GameHostSettings().Load(std::filesystem::path(ASSET_ROOT) / "gunbros.cfg")) { return 1; }
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
    return RunGameFrontEnd(big, screenshot, page, originalProfile, profile, &window);
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR arguments, int) {
    OpenGameLog();
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[application] Windows GUI process=%lu console=%u\n", GetCurrentProcessId(), GetConsoleWindow() != nullptr);
    // The Unicode Windows CRT initializes __wargv, not __argv. Convert owned
    // strings before passing the existing UTF-8 runtime entry points.
    std::vector<std::string> values;
    for (int index = 0; index < __argc; ++index) {
        const int size = WideCharToMultiByte(CP_UTF8, 0, __wargv[index], -1, nullptr, 0, nullptr, nullptr);
        if (size < 1) { return 1; }
        std::string value(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, __wargv[index], -1, value.data(), size, nullptr, nullptr);
        value.pop_back();
        values.push_back(std::move(value));
    }
    std::vector<char *> argv;
    for (auto &value : values) { argv.push_back(value.data()); }
    const int result = RunApplication(static_cast<int>(argv.size()), argv.data(), arguments);
    std::printf("[application] exit=%d\n", result);
    return result;
}
