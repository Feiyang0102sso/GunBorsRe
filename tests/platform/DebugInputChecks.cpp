#include "gun_bros_re/debug/FrameRateOverlay.h"
/** @file DebugInputChecks.cpp
 * @brief Verify queued SDL modifiers and one-shot map hotkeys without physical input.
 */
#include "gun_bros_re/debug/DebugKeys.h"
#include "gun_bros_re/debug/SurvivalDebug.h"
#include "gun_bros_re/host/ZHostSettings.h"
#include "engine/platform/ZGLLoader.h"
#include "gun_bros_re/debug/Capture.h"
#include "TestOutput.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <fstream>
#include <vector>

namespace {
bool PushKey(Uint32 type, SDL_Keycode key, SDL_Keymod modifiers, bool repeat = false) {
    SDL_Event event{};
    event.type = type;
    event.key.key = key;
    event.key.mod = modifiers;
    event.key.repeat = repeat;
    return SDL_PushEvent(&event);
}
}

int CheckDebugInput() {
    ZWindow window;
    if (!window.Open("Debug input checks", 640, 480)) { return 1; }
    window.SetEscapeCloses(false);
    if (!window.PumpEvents()) { return 1; }
    while (window.TakeKeyPress() != ZKeyCode::None) {}
    unsigned failures = 0;
    const bool previousDebugMode = GameHostSettings().debugMode;
    GameHostSettings().debugMode = true;
    // Use SDL's real modifier-aware translation, with the retail cheat recognizer enabled.
    GameCheats::Bind();
    window.EnableCheats(true);
    for (SDL_Scancode scan : {SDL_SCANCODE_C, SDL_SCANCODE_I}) {
        const SDL_Keycode shifted = SDL_GetKeyFromScancode(scan, SDL_KMOD_LSHIFT, true);
        const SDL_Keycode released = SDL_GetKeyFromScancode(scan, SDL_KMOD_NONE, true);
        PushKey(SDL_EVENT_KEY_DOWN, shifted, SDL_KMOD_LSHIFT);
        PushKey(SDL_EVENT_KEY_UP, released, SDL_KMOD_NONE);
        window.PumpEvents();
        const ZKeyCode key = window.TakeKeyPress();
        bool toggled = GameDebugKeys::TogglesCollision(key, window);
        if (scan == SDL_SCANCODE_I) { toggled = GameDebugKeys::TogglesInfo(key, window); }
        std::printf("[shift-repro] scan=%u shifted=%u key=%u toggled=%d\n", scan, shifted, key, toggled);
        if (!toggled || window.IsKeyDown(key)) { ++failures; }
        bool collisions = false;
        const bool previousInfo = GameHostSettings().drawDebugInfo;
        if (!HandleDebugKey(key, window, collisions)) { ++failures; }
        if (scan == SDL_SCANCODE_C && !collisions) { ++failures; }
        if (scan == SDL_SCANCODE_I && GameHostSettings().drawDebugInfo == previousInfo) { ++failures; }
        GameHostSettings().drawDebugInfo = previousInfo;
    }
    // Unmodified original cheat sequences must still reach their command consumer.
    for (SDL_Keycode key : {SDLK_C, SDLK_H, SDLK_M}) {
        PushKey(SDL_EVENT_KEY_DOWN, key, SDL_KMOD_NONE);
        PushKey(SDL_EVENT_KEY_UP, key, SDL_KMOD_NONE);
    }
    window.PumpEvents();
    if (window.TakeCheatCode() != GameCheats::Money || window.TakeKeyPress() != ZKeyCode::None) { ++failures; }
    // Every configured sequence completes once, with repeated key-downs ignored.
    for (const char *command : GameCheats::Commands) {
        for (const char *letter = command; *letter != '\0'; ++letter) {
            PushKey(SDL_EVENT_KEY_DOWN, *letter, SDL_KMOD_NONE);
            PushKey(SDL_EVENT_KEY_DOWN, *letter, SDL_KMOD_NONE, true);
            PushKey(SDL_EVENT_KEY_UP, *letter, SDL_KMOD_NONE);
        }
        window.PumpEvents();
        if (window.TakeCheatCode() != command || !window.TakeCheatCode().empty()) { ++failures; }
        while (window.TakeKeyPress() != ZKeyCode::None) {}
    }
    // Removed commands must no longer produce actions.
    for (const char *command : {"chh", "chi", "chunlock"}) {
        for (const char *letter = command; *letter != '\0'; ++letter) {
            PushKey(SDL_EVENT_KEY_DOWN, *letter, SDL_KMOD_NONE);
            PushKey(SDL_EVENT_KEY_UP, *letter, SDL_KMOD_NONE);
        }
        window.PumpEvents();
        if (!window.TakeCheatCode().empty()) { ++failures; }
        while (window.TakeKeyPress() != ZKeyCode::None) {}
    }
    // Both Shift sides must survive release before the consumer drains the queue.
    const SDL_Keymod modifiers[] = {SDL_KMOD_LSHIFT, SDL_KMOD_RSHIFT, SDL_KMOD_NONE};
    for (SDL_Keymod modifier : modifiers) {
        if (!PushKey(SDL_EVENT_KEY_DOWN, SDLK_T, modifier) ||
            !PushKey(SDL_EVENT_KEY_DOWN, SDLK_T, modifier, true) ||
            !PushKey(SDL_EVENT_KEY_UP, SDLK_T, SDL_KMOD_NONE) || !window.PumpEvents()) { return 1; }
        const ZKeyCode key = window.TakeKeyPress();
        if (key != ZKeyCode::T || GameDebugKeys::StartsTutorial(key, window) != (modifier != SDL_KMOD_NONE) ||
            window.TakeKeyPress() != ZKeyCode::None || !window.TakeCheatCode().empty()) { ++failures; }
    }
    for (SDL_Keymod modifier : modifiers) {
        if (!PushKey(SDL_EVENT_KEY_DOWN, SDLK_M, modifier) ||
            !PushKey(SDL_EVENT_KEY_DOWN, SDLK_M, modifier, true) ||
            !PushKey(SDL_EVENT_KEY_UP, SDLK_M, SDL_KMOD_NONE) ||
            !window.PumpEvents()) { return 1; }
        const ZKeyCode key = window.TakeKeyPress();
        const bool expected = modifier != SDL_KMOD_NONE;
        if (key != GameDebugKeys::MapBrowser || GameDebugKeys::OpensMapBrowser(key, window) != expected ||
            window.IsKeyDown(ZKeyCode::M) || window.TakeKeyPress() != ZKeyCode::None) { ++failures; }
    }
    // The configuration is the only availability gate for all debug shortcuts.
    const auto debugConfigPath = TestOutput::Path("debug-key-gate.cfg");
    for (unsigned enabled = 0; enabled < 2; ++enabled) {
        { std::ofstream config(debugConfigPath); config << "DebugMode=" << enabled << '\n'; }
        ZHostSettings settings;
        if (!settings.Load(debugConfigPath)) { return 1; }
        GameHostSettings().debugMode = settings.debugMode;
        for (SDL_Keycode letter : {SDLK_C, SDLK_I, SDLK_M, SDLK_T, SDLK_F3}) {
            PushKey(SDL_EVENT_KEY_DOWN, letter, SDL_KMOD_LSHIFT);
            PushKey(SDL_EVENT_KEY_UP, letter, SDL_KMOD_NONE);
            if (!window.PumpEvents()) { return 1; }
            const ZKeyCode key = window.TakeKeyPress();
            bool active = false;
            if (letter == SDLK_C) { active = GameDebugKeys::TogglesCollision(key, window); }
            if (letter == SDLK_I) { active = GameDebugKeys::TogglesInfo(key, window); }
            if (letter == SDLK_M || letter == SDLK_F3) { active = GameDebugKeys::OpensMapBrowser(key, window); }
            if (letter == SDLK_T) { active = GameDebugKeys::StartsTutorial(key, window); }
            const bool expected = enabled == 1 && letter != SDLK_F3;
            if (active != expected) { ++failures; }
            std::printf("[debug-key-gate] enabled=%u key=%u active=%d expected=%d\n", enabled, letter, active, expected);
        }
    }
    GameHostSettings().debugMode = true;
    const SDL_Keycode arrows[] = {SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT};
    const ZKeyCode actions[] = {GameDebugKeys::MapPrevious, GameDebugKeys::MapNext,
        GameDebugKeys::MapPreviousPage, GameDebugKeys::MapNextPage};
    for (unsigned index = 0; index < 4; ++index) {
        if (!PushKey(SDL_EVENT_KEY_DOWN, arrows[index], SDL_KMOD_NONE) ||
            !PushKey(SDL_EVENT_KEY_UP, arrows[index], SDL_KMOD_NONE) || !window.PumpEvents()) { return 1; }
        if (window.TakeKeyPress() != actions[index] || window.TakeKeyPress() != ZKeyCode::None) { ++failures; }
    }
    for (SDL_Keycode hotkey : {SDLK_C, SDLK_I}) {
        for (SDL_Keymod modifier : modifiers) {
            if (!PushKey(SDL_EVENT_KEY_DOWN, hotkey, modifier) ||
                !PushKey(SDL_EVENT_KEY_DOWN, hotkey, modifier, true) ||
                !PushKey(SDL_EVENT_KEY_UP, hotkey, SDL_KMOD_NONE) || !window.PumpEvents()) { return 1; }
            const ZKeyCode key = window.TakeKeyPress();
            bool toggled = GameDebugKeys::TogglesCollision(key, window);
            if (hotkey == SDLK_I) { toggled = GameDebugKeys::TogglesInfo(key, window); }
            if (toggled != (modifier != SDL_KMOD_NONE) || window.TakeKeyPress() != ZKeyCode::None) { ++failures; }
        }
    }
    // Missing DrawFPS preserves the enabled default; all four flag combinations are independent.
    const std::string configPath = TestOutput::Path("overlay.cfg");
    { std::ofstream config(configPath); config << "DebugMode=0\n"; }
    ZHostSettings defaults;
    if (!defaults.Load(configPath) || !defaults.drawFPS || !defaults.drawDebugInfo) { ++failures; }
    if (defaults.dmBotLevel != 1) { ++failures; }
    for (int level : {-1, 0, 1, 2, 3, 4}) {
        { std::ofstream config(configPath); config << "DMBotLevel = " << level << " # DM only\n"; }
        ZHostSettings settings;
        const bool valid = level >= 1 && level <= 3;
        if (settings.Load(configPath) != valid || (valid && settings.dmBotLevel != level)) { ++failures; }
    }
    for (unsigned debug = 0; debug < 2; ++debug) {
        for (unsigned fps = 0; fps < 2; ++fps) {
            { std::ofstream config(configPath); config << "DebugMode=" << debug << "\nDrawFPS=" << fps << "\n"; }
            ZHostSettings settings;
            if (!settings.Load(configPath) || settings.drawFPS != (fps != 0) || settings.debugMode != (debug != 0)) { ++failures; }
            if (!SetDebugFPS(window, settings.drawFPS)) { return 1; }
            glDisable(GL_SCISSOR_TEST);
            glClearColor(0.25f, 0.125f, 0.0625f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            // The overlay must ignore, then restore, a scene's restricted viewport and scissor.
            glViewport(3, 5, 120, 90);
            glScissor(0, 0, 1, 1);
            glEnable(GL_SCISSOR_TEST);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            window.DrawPresentationOverlay();
            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);
            if (viewport[0] != 3 || viewport[1] != 5 || viewport[2] != 120 || viewport[3] != 90 ||
                !glIsEnabled(GL_SCISSOR_TEST) || !glIsEnabled(GL_DEPTH_TEST) ||
                !glIsEnabled(GL_BLEND) || !glIsEnabled(GL_CULL_FACE)) { ++failures; }
            int width = 0, height = 0;
            window.GetDrawableSize(width, height);
            std::vector<unsigned char> pixels(width * height * 4);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            unsigned fontPixels = 0;
            unsigned blackPixels = 0;
            for (std::size_t index = 0; index < pixels.size(); index += 4) {
                if (pixels[index + 2] > 180 && pixels[index + 1] > 80) { ++fontPixels; }
                if (pixels[index] < 10 && pixels[index + 1] < 10 && pixels[index + 2] < 10) { ++blackPixels; }
            }
            if ((fontPixels > 0) != settings.drawFPS || blackPixels != 0) { ++failures; }
            if (!Capture::SaveFrame(window, TestOutput::Path("fps-" + std::to_string(fps) + "-debug-" + std::to_string(debug) + ".png"))) { ++failures; }
        }
    }
    std::printf("[debug-input-check] modifiers/repeat/shift-C-I/FPS-flags/GL-state failures=%u\n", failures);
    GameHostSettings().debugMode = previousDebugMode;
    return failures != 0;
}
