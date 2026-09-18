#include "gun_bros_re/gameplay/map/CMapInternal.h"
#include "TestOutput.h"
using namespace MapDetail;

namespace MapDetail {

// Exercise the real SDL event queue and CWindow recognizer, including held S.
bool PushBossCheckKey(ZWindow &window, char letter, bool repeat , bool checkMovement ) {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = static_cast<SDL_Keycode>(letter);
    event.key.down = true;
    event.key.repeat = repeat;
    if (!SDL_PushEvent(&event) || !window.PumpEvents()) { return false; }
    if (checkMovement && !window.IsKeyDown(ZKeyCode::S)) { return false; }
    event.type = SDL_EVENT_KEY_UP;
    event.key.down = false;
    event.key.repeat = false;
    return SDL_PushEvent(&event) && window.PumpEvents();
}
}
