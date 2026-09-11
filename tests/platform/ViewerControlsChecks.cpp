/** Exercise real SDL routing: remapped keys, dock ownership, resize and GL state. */
#include "gun_bros_viewer/ViewerControls.h"
#include "tests/Capture.h"
#include "tests/TestOutput.h"
#include <SDL3/SDL.h>
#include <cstdio>

namespace {
unsigned failures = 0;
void Check(bool condition, const char *name) {
    if (!condition) { ++failures; std::printf("[viewer-controls-check] FAILED: %s\n", name); }
}
bool PushKey(SDL_EventType type, SDL_Keycode key) {
    SDL_Event event{};
    event.type = type;
    event.key.key = key;
    return SDL_PushEvent(&event);
}
bool PushButton(SDL_EventType type, float x, float y, Uint8 button = SDL_BUTTON_LEFT) {
    SDL_Event event{};
    event.type = type;
    event.button.x = x;
    event.button.y = y;
    event.button.button = button;
    return SDL_PushEvent(&event);
}
bool PushMotion(float x, float y, float dx, float dy) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = x;
    event.motion.y = y;
    event.motion.xrel = dx;
    event.motion.yrel = dy;
    return SDL_PushEvent(&event);
}
bool PushWheel(float x, float y, float delta) {
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.mouse_x = x;
    event.wheel.mouse_y = y;
    event.wheel.y = delta;
    return SDL_PushEvent(&event);
}
bool Draw(CWindow &window, ViewerControls &controls, const char *name) {
    int width = 0, height = 0;
    controls.GetDrawableSize(width, height);
    glViewport(0, 0, width, height);
    glClearColor(0.08f, 0.1f, 0.13f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    if (!controls.Draw()) { return false; }
    GLint viewport[4], source = 0, destination = 0;
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_BLEND_SRC, &source);
    glGetIntegerv(GL_BLEND_DST, &destination);
    Check(viewport[2] == width && viewport[3] == height && glIsEnabled(GL_DEPTH_TEST) &&
        glIsEnabled(GL_BLEND) && source == GL_ONE && destination == GL_ONE, "scene GL state restored");
    return Capture::SaveFrame(window, TestOutput::Path(name));
}
}

int CheckViewerControls() {
    failures = 0;
    CWindow window;
    if (!window.Open("Viewer controls checks", 800, 600) || !window.PumpEvents()) { return 1; }
    while (window.TakeKeyPress() != KeyCode::None) {}
    {
        // Changing the definition must affect both dispatch and the rendered help.
        std::vector<ViewerBinding> bindings(arena::All, arena::All + arena::Bindings.count);
        for (auto &binding : bindings) {
            if (binding.action == ViewerAction::Category3) { binding.key = KeyCode::Q; }
            if (binding.action == ViewerAction::MoveUp) { binding.key = KeyCode::T; }
            if (binding.action == ViewerAction::Back) { binding.key = KeyCode::B; }
        }
        ViewerControls controls(window, {L"Remapped arena", bindings.data(), bindings.size()});
        if (!controls.Init() || !controls.PumpEvents()) { return 1; }
        int width = 0, height = 0;
        controls.GetDrawableSize(width, height);
        Check(width == 400 && height == 600, "expanded panel reserves scene width");
        if (!Draw(window, controls, "controls-expanded.png")) { return 1; }
        Check(PushKey(SDL_EVENT_KEY_DOWN, SDLK_Q) && PushKey(SDL_EVENT_KEY_UP, SDLK_Q) &&
            controls.PumpEvents(), "queue remapped category");
        Check(controls.WeaponSelectionKey(controls.TakeKeyPress()) == KeyCode::Digit3 &&
            controls.WeaponSelectionKey(KeyCode::Digit3) == KeyCode::None, "category remap reaches shared catalogue");
        Check(PushKey(SDL_EVENT_KEY_DOWN, SDLK_T) && controls.PumpEvents() &&
            controls.IsDown(ViewerAction::MoveUp), "remapped held movement");
        Check(PushKey(SDL_EVENT_KEY_UP, SDLK_T) && controls.PumpEvents() &&
            !controls.IsDown(ViewerAction::MoveUp), "held movement released");
        Check(PushButton(SDL_EVENT_MOUSE_BUTTON_DOWN, 600, 200) &&
            PushMotion(200, 200, -400, 0) && controls.PumpEvents() &&
            !controls.IsDown(ViewerAction::Fire), "panel-origin click cannot fire after entering scene");
        Check(PushButton(SDL_EVENT_MOUSE_BUTTON_UP, 200, 200) &&
            PushButton(SDL_EVENT_MOUSE_BUTTON_DOWN, 200, 200) && controls.PumpEvents() &&
            controls.IsDown(ViewerAction::Fire), "scene-origin click fires");
        float x = 0, y = 0;
        Check(controls.GetMousePosition(x, y) && x == 200 && y == 200, "aim uses scene coordinates");
        Check(PushMotion(600, 200, 400, 0) && controls.PumpEvents() &&
            !controls.IsDown(ViewerAction::Fire) && !controls.GetMousePosition(x, y), "panel blocks fire and aim");
        Check(PushButton(SDL_EVENT_MOUSE_BUTTON_UP, 600, 200) &&
            PushWheel(600, 200, -100) && controls.PumpEvents() && controls.TakeWheelDelta() == 0,
            "panel consumes wheel");
        if (!Draw(window, controls, "controls-scrolled.png")) { return 1; }
        Check(PushButton(SDL_EVENT_MOUSE_BUTTON_DOWN, 770, 24) &&
            PushButton(SDL_EVENT_MOUSE_BUTTON_UP, 770, 24) && controls.PumpEvents(), "collapse click");
        controls.GetDrawableSize(width, height);
        Check(width == 756, "collapse releases viewport width");
        if (!Draw(window, controls, "controls-collapsed.png")) { return 1; }
        Check(PushButton(SDL_EVENT_MOUSE_BUTTON_DOWN, 780, 24) &&
            PushButton(SDL_EVENT_MOUSE_BUTTON_UP, 780, 24) && controls.PumpEvents(), "expand click");
        controls.GetDrawableSize(width, height);
        Check(width == 400, "expand restores viewport width");
        SDL_Window *surface = SDL_GetWindowFromID(window.GetSurfaceId());
        Check(SDL_SetWindowSize(surface, 700, 400) && controls.PumpEvents(), "resize window");
        controls.GetDrawableSize(width, height);
        Check(width == 350 && height == 400, "resize updates scene projection dimensions");
        if (!Draw(window, controls, "controls-resized.png")) { return 1; }
        Check(PushKey(SDL_EVENT_KEY_DOWN, SDLK_ESCAPE) && PushKey(SDL_EVENT_KEY_UP, SDLK_ESCAPE) &&
            controls.PumpEvents(), "old Back key no longer exits");
        Check(PushKey(SDL_EVENT_KEY_DOWN, SDLK_B) && !controls.PumpEvents(), "remapped Back exits scene");
    }
    {
        std::vector<ViewerBinding> bindings(meshview::All, meshview::All + meshview::Bindings.count);
        for (auto &binding : bindings) {
            if (binding.action == ViewerAction::Drag) { binding.input = ViewerInput::RightDrag; }
        }
        ViewerControls controls(window, {L"Remapped mesh", bindings.data(), bindings.size()});
        if (!controls.Init() || !controls.PumpEvents()) { return 1; }
        int dx = 0, dy = 0;
        Check(PushButton(SDL_EVENT_MOUSE_BUTTON_DOWN, 100, 200) &&
            PushMotion(120, 210, 20, 10) && controls.PumpEvents(), "old mouse drag");
        controls.TakeDragDelta(dx, dy);
        Check(dx == 0 && dy == 0, "old drag button disabled");
        Check(PushButton(SDL_EVENT_MOUSE_BUTTON_UP, 120, 210) &&
            PushButton(SDL_EVENT_MOUSE_BUTTON_DOWN, 120, 210, SDL_BUTTON_RIGHT) &&
            PushMotion(140, 220, 20, 10) && controls.PumpEvents(), "remapped mouse drag");
        controls.TakeDragDelta(dx, dy);
        Check(dx == 20 && dy == 10, "right drag drives scene");
        Check(PushMotion(600, 220, 460, 0) && controls.PumpEvents(), "drag onto panel");
        controls.TakeDragDelta(dx, dy);
        Check(dx == 0 && dy == 0, "panel blocks drag deltas");
        Check(PushWheel(600, 220, -2) && controls.PumpEvents() && controls.TakeWheelDelta() == 0,
            "panel wheel does not zoom mesh");
        Check(PushWheel(100, 220, 2) && controls.PumpEvents() && controls.TakeWheelDelta() == 2,
            "scene wheel still zooms mesh");
        SDL_Event lost{};
        lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        Check(SDL_PushEvent(&lost) && controls.PumpEvents(), "focus loss");
        Check(PushMotion(160, 220, 20, 0) && controls.PumpEvents(), "motion after focus loss");
        controls.TakeDragDelta(dx, dy);
        Check(dx == 0 && dy == 0, "focus loss releases drag capture");
    }
    std::printf("[viewer-controls-check] remap/dock/mouse/resize/GL failures=%u\n", failures);
    return failures != 0;
}
