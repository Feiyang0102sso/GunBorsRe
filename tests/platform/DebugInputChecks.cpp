/** @file DebugInputChecks.cpp
 * @brief Verify queued SDL modifiers and one-shot map hotkeys without physical input.
 */
#include "gun_bros_re/DebugKeys.h"
#include <SDL3/SDL.h>
#include <cstdio>

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
    CWindow window;
    if (!window.Open("Debug input checks", 640, 480)) { return 1; }
    window.SetEscapeCloses(false);
    if (!window.PumpEvents()) { return 1; }
    while (window.TakeKeyPress() != KeyCode::None) {}
    unsigned failures = 0;
    // Both Shift sides must survive release before the consumer drains the queue.
    const SDL_Keymod modifiers[] = {SDL_KMOD_LSHIFT, SDL_KMOD_RSHIFT, SDL_KMOD_NONE};
    for (SDL_Keymod modifier : modifiers) {
        if (!PushKey(SDL_EVENT_KEY_DOWN, SDLK_F3, modifier) ||
            !PushKey(SDL_EVENT_KEY_DOWN, SDLK_F3, modifier, true) ||
            !PushKey(SDL_EVENT_KEY_UP, SDLK_F3, SDL_KMOD_NONE) ||
            !window.PumpEvents()) { return 1; }
        const KeyCode key = window.TakeKeyPress();
        const bool expected = modifier != SDL_KMOD_NONE;
        if (key != GameDebugKeys::MapBrowser || GameDebugKeys::OpensMapBrowser(key, window) != expected ||
            window.IsKeyDown(KeyCode::F3) || window.TakeKeyPress() != KeyCode::None) { ++failures; }
    }
    const SDL_Keycode arrows[] = {SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT};
    const KeyCode actions[] = {GameDebugKeys::MapPrevious, GameDebugKeys::MapNext,
        GameDebugKeys::MapPreviousPage, GameDebugKeys::MapNextPage};
    for (unsigned index = 0; index < 4; ++index) {
        if (!PushKey(SDL_EVENT_KEY_DOWN, arrows[index], SDL_KMOD_NONE) ||
            !PushKey(SDL_EVENT_KEY_UP, arrows[index], SDL_KMOD_NONE) || !window.PumpEvents()) { return 1; }
        if (window.TakeKeyPress() != actions[index] || window.TakeKeyPress() != KeyCode::None) { ++failures; }
    }
    std::printf("[debug-input-check] modifier-release/repeat/plain-F3 failures=%u\n", failures);
    return failures != 0;
}
