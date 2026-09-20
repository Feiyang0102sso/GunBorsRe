/**
 * @file ZWindow.cpp
 * @brief Window and OpenGL context, on SDL3.
 */

#include "engine/platform/ZWindow.h"
#include "engine/platform/ZWindowOverlay.h"
#define NOMINMAX
#include <Windows.h>
#include "engine/platform/ZAudioPlayer.h"

#include "engine/graphics/ZPNG.h"
#include "engine/platform/ZGLLoader.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>
#include <algorithm>

namespace {
ZWindow::CommandMatcher commandMatcher = nullptr;
std::uint64_t commandTimeoutMs = 0;

constexpr int kUsableDisplayPercent = 90;
/** Keep the requested 4:3 window comfortably inside the current desktop. */
void FitWindowToUsableDisplay(int &width, int &height) {
    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    if (display == 0) {
        return;
    }

    SDL_Rect usableBounds;
    if (!SDL_GetDisplayUsableBounds(display, &usableBounds)) {
        return;
    }

    const int maximumWidth = usableBounds.w * kUsableDisplayPercent / 100;
    const int maximumHeight = usableBounds.h * kUsableDisplayPercent / 100;
    float scale = 1.0f;

    if (width > maximumWidth) {
        scale = static_cast<float>(maximumWidth) / static_cast<float>(width);
    }
    if (height > maximumHeight) {
        const float heightScale =
            static_cast<float>(maximumHeight) / static_cast<float>(height);
        if (heightScale < scale) {
            scale = heightScale;
        }
    }

    if (scale < 1.0f) {
        width = std::max(1, static_cast<int>(static_cast<float>(width) * scale));
        height = std::max(1, static_cast<int>(static_cast<float>(height) * scale));
    }
}

/** Map an SDL keycode onto the platform-independent enum. */
ZKeyCode TranslateKey(SDL_Keycode key) {
    switch (key) {
        case SDLK_ESCAPE: return ZKeyCode::Escape;
        case SDLK_RETURN: return ZKeyCode::Enter;
        case SDLK_LEFT:   return ZKeyCode::Left;
        case SDLK_RIGHT:  return ZKeyCode::Right;
        case SDLK_UP:     return ZKeyCode::Up;
        case SDLK_DOWN:   return ZKeyCode::Down;
        case SDLK_HOME:   return ZKeyCode::Home;
        case SDLK_F3:     return ZKeyCode::F3;
        case SDLK_T:      return ZKeyCode::T;
        case SDLK_P:      return ZKeyCode::P;
        case SDLK_G:      return ZKeyCode::G;
        case SDLK_X:      return ZKeyCode::X;
        case SDLK_R:      return ZKeyCode::R;
        case SDLK_K:      return ZKeyCode::K;
        case SDLK_M:      return ZKeyCode::M;
        case SDLK_N:      return ZKeyCode::N;
        case SDLK_SPACE:  return ZKeyCode::Space;
        case SDLK_PERIOD: return ZKeyCode::Period;
        case SDLK_W:      return ZKeyCode::W;
        case SDLK_A:      return ZKeyCode::A;
        case SDLK_S:      return ZKeyCode::S;
        case SDLK_D:      return ZKeyCode::D;
        case SDLK_B:      return ZKeyCode::B;
        case SDLK_C:      return ZKeyCode::C;
        case SDLK_I:      return ZKeyCode::I;
        case SDLK_E:      return ZKeyCode::E;
        case SDLK_F:      return ZKeyCode::F;
        case SDLK_Q:      return ZKeyCode::Q;
        case SDLK_1:      return ZKeyCode::Digit1;
        case SDLK_2:      return ZKeyCode::Digit2;
        case SDLK_3:      return ZKeyCode::Digit3;
        case SDLK_4:      return ZKeyCode::Digit4;
        case SDLK_5:      return ZKeyCode::Digit5;
        case SDLK_6:      return ZKeyCode::Digit6;
        case SDLK_7:      return ZKeyCode::Digit7;
        case SDLK_8:      return ZKeyCode::Digit8;
        case SDLK_9:      return ZKeyCode::Digit9;
        default:          return ZKeyCode::None;
    }
}

}  // namespace

ZWindow::ZWindow()
    : m_window(nullptr),
      m_context(nullptr),
      m_sdlInitialised(false),
      m_quitRequested(false),
      m_dragDeltaX(0),
      m_dragDeltaY(0),
      m_wheelDelta(0.0f) {
    for (int i = 0; i < static_cast<int>(ZKeyCode::Count); ++i) {
        m_keyDown[i] = false;
    }
}

ZWindow::~ZWindow() {
    Close();
}

bool ZWindow::Open(const std::string &title, int width, int height) {
    if (IsOpen()) {
        // Scene boundaries retain the HWND, GL objects, focus and user sizing.
        // Only transient input belongs to the previous scene.
        m_keyPresses.clear();
        m_cheatCodes.clear();
        m_cheatPrefix.clear();
        m_dragDeltaX = 0;
        m_dragDeltaY = 0;
        m_wheelDelta = 0;
        m_rightDrag = false;
        m_escapeCloses = true;
        m_cheatsEnabled = false;
        for (bool &down : m_keyDown) { down = false; }
        return true;
    }
    Close();
    m_quitRequested = false;
    m_keyPresses.clear();
    for (int i = 0; i < static_cast<int>(ZKeyCode::Count); ++i) {
        m_keyDown[i] = false;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        std::printf("[window] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    m_sdlInitialised = true;

    FitWindowToUsableDisplay(width, height);

    // Ask for a core profile before creating the window -- these are hints for
    // the context that SDL_GL_CreateContext will build.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, kRequiredGLMajor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, kRequiredGLMinor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    m_window = SDL_CreateWindow(title.c_str(), width, height,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (m_window == nullptr) {
        std::printf("[window] SDL_CreateWindow failed: %s\n", SDL_GetError());
        Close();
        return false;
    }
    SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);
    ++m_surfaceGeneration;

    m_context = SDL_GL_CreateContext(m_window);
    if (m_context == nullptr) {
        std::printf("[window] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        Close();
        return false;
    }

    if (!GLLoaderInitialize()) {
        std::printf("[window] the GL context is missing entry points we need\n");
        Close();
        return false;
    }

    // vsync. Not fatal if the driver refuses.
    if (!SDL_GL_SetSwapInterval(1)) {
        std::printf("[window] vsync unavailable: %s\n", SDL_GetError());
    }

    const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    std::printf("[window] %dx%d, GL %s on %s\n", width, height,
                (version != nullptr) ? version : "?",
                (renderer != nullptr) ? renderer : "?");

    return true;
}

unsigned ZWindow::GetSurfaceId() const { return SDL_GetWindowID(m_window); }

void ZWindow::Close() {
    m_presentationOverlay.reset();
    if (m_context != nullptr) {
        SDL_GL_DestroyContext(m_context);
        m_context = nullptr;
    }
    if (m_window != nullptr) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    if (m_sdlInitialised) {
        SDL_Quit();
        m_sdlInitialised = false;
    }
}

bool ZWindow::PumpEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (m_eventFilter != nullptr && !m_eventFilter(m_eventContext, event)) {
            continue;
        }
        if (event.type == SDL_EVENT_QUIT) {
            m_quitRequested = true;
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            // Modified shortcuts must not become prefixes of typed cheat commands.
            const bool modified = (event.key.mod & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0;
            if (modified) { m_cheatPrefix.clear(); }
            if (!modified && m_cheatsEnabled && commandMatcher != nullptr && event.key.key >= SDLK_A && event.key.key <= SDLK_Z) {
                const auto now = SDL_GetTicks();
                if (now - m_cheatKeyTime > commandTimeoutMs) { m_cheatPrefix.clear(); }
                m_cheatKeyTime = now;
                if (commandMatcher(m_cheatPrefix, m_cheatCodes, static_cast<char>(event.key.key), event.key.repeat)) { continue; }
            }
            if (event.key.key == SDLK_ESCAPE && m_escapeCloses) {
                m_quitRequested = true;
            } else {
                const ZKeyCode code = TranslateKey(event.key.key);
                if (code != ZKeyCode::None) {
                    const int index = static_cast<int>(code);
                    if (!m_keyDown[index]) {
                        m_keyPresses.push_back({code, (event.key.mod & SDL_KMOD_SHIFT) != 0});
                    }
                    m_keyDown[index] = true;
                }
            }
        } else if (event.type == SDL_EVENT_KEY_UP) {
            const ZKeyCode code = TranslateKey(event.key.key);
            if (code != ZKeyCode::None) {
                m_keyDown[static_cast<int>(code)] = false;
            }
        } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
            // SDL reports the button state with the motion, so there is no
            // need to track presses and releases separately.
            SDL_MouseButtonFlags dragButton = SDL_BUTTON_LMASK;
            if (m_rightDrag) { dragButton = SDL_BUTTON_RMASK; }
            if ((event.motion.state & dragButton) != 0) {
                m_dragDeltaX += static_cast<int>(event.motion.xrel);
                m_dragDeltaY += static_cast<int>(event.motion.yrel);
            }
        } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            m_wheelDelta += event.wheel.y;
        } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            m_cheatPrefix.clear();
            for (int i = 0; i < static_cast<int>(ZKeyCode::Count); ++i) {
                m_keyDown[i] = false;
            }
            m_keyPresses.clear();
        }
    }
    return !m_quitRequested;
}

void ZWindow::TakeDragDelta(int &deltaX, int &deltaY) {
    deltaX = m_dragDeltaX;
    deltaY = m_dragDeltaY;
    m_dragDeltaX = 0;
    m_dragDeltaY = 0;
}

float ZWindow::TakeWheelDelta() {
    const float delta = m_wheelDelta;
    m_wheelDelta = 0.0f;
    return delta;
}

ZKeyCode ZWindow::TakeKeyPress() {
    if (m_keyPresses.empty()) {
        return ZKeyCode::None;
    }
    const ZKeyCode code = m_keyPresses.front().code;
    m_lastKeyShift = m_keyPresses.front().shift;
    m_keyPresses.erase(m_keyPresses.begin());
    return code;
}

bool ZWindow::IsKeyDown(ZKeyCode key) const {
    const int index = static_cast<int>(key);
    if (index <= static_cast<int>(ZKeyCode::None) ||
        index >= static_cast<int>(ZKeyCode::Count)) {
        return false;
    }
    return m_keyDown[index];
}

void ZWindow::Present() {
    if (m_presentationOverlay) { m_presentationOverlay->Tick(GetTicksMs()); }
    DrawPresentationOverlay();
    SDL_GL_SwapWindow(m_window);
}

void ZWindow::SetPresentationOverlay(std::unique_ptr<ZWindowOverlay> overlay) {
    m_presentationOverlay = std::move(overlay);
}

void ZWindow::DrawPresentationOverlay() const {
    if (!m_presentationOverlay) { return; }
    int width = 0, height = 0;
    GetDrawableSize(width, height);
    m_presentationOverlay->Draw(width, height);
}

bool ZWindow::SetVSync(bool enabled) {
    int interval = 0;
    if (enabled) { interval = 1; }
    return SDL_GL_SetSwapInterval(interval);
}

void ZWindow::SetCommandMatcher(CommandMatcher matcher, std::uint64_t timeoutMs) {
    commandMatcher = matcher;
    commandTimeoutMs = timeoutMs;
}
std::string ZWindow::TakeCheatCode() {
    if (m_cheatCodes.empty()) { return {}; }
    const std::string code = m_cheatCodes.front();
    m_cheatCodes.erase(m_cheatCodes.begin());
    return code;
}

bool ZWindow::GetMousePosition(float &x, float &y) const {
    if (SDL_GetMouseFocus() != m_window) { return false; }
    SDL_GetMouseState(&x, &y);
    int width = 0, height = 0, drawableWidth = 0, drawableHeight = 0;
    SDL_GetWindowSize(m_window, &width, &height);
    GetDrawableSize(drawableWidth, drawableHeight);
    if (width <= 0 || height <= 0) { return false; }
    x *= static_cast<float>(drawableWidth) / width;
    y *= static_cast<float>(drawableHeight) / height;
    return true;
}

bool ZWindow::IsLeftMouseDown() const {
    return SDL_GetKeyboardFocus() == m_window &&
        (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;
}

void ZWindow::SetTitle(const std::string &title) {
    SDL_SetWindowTitle(m_window, title.c_str());
}

void ZWindow::GetDrawableSize(int &width, int &height) const {
    width = 0;
    height = 0;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
}

void ZWindow::GetPosition(int &x, int &y) const {
    SDL_GetWindowPosition(m_window, &x, &y);
}

std::uint64_t ZWindow::GetTicksMs() const {
    return SDL_GetTicks();
}
