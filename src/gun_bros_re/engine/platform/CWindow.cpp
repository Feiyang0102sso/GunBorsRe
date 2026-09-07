/**
 * @file CWindow.cpp
 * @brief Window and OpenGL context, on SDL3.
 */

#include "engine/platform/CWindow.h"

#include "engine/CPNG.h"
#include "engine/platform/GLLoader.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>

namespace {

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
        width = static_cast<int>(static_cast<float>(width) * scale);
        height = static_cast<int>(static_cast<float>(height) * scale);
    }
}

/** Map an SDL keycode onto the platform-independent enum. */
KeyCode TranslateKey(SDL_Keycode key) {
    switch (key) {
        case SDLK_LEFT:   return KeyCode::Left;
        case SDLK_RIGHT:  return KeyCode::Right;
        case SDLK_UP:     return KeyCode::Up;
        case SDLK_DOWN:   return KeyCode::Down;
        case SDLK_HOME:   return KeyCode::Home;
        case SDLK_T:      return KeyCode::T;
        case SDLK_P:      return KeyCode::P;
        case SDLK_G:      return KeyCode::G;
        case SDLK_K:      return KeyCode::K;
        case SDLK_M:      return KeyCode::M;
        case SDLK_N:      return KeyCode::N;
        case SDLK_SPACE:  return KeyCode::Space;
        case SDLK_PERIOD: return KeyCode::Period;
        case SDLK_W:      return KeyCode::W;
        case SDLK_A:      return KeyCode::A;
        case SDLK_S:      return KeyCode::S;
        case SDLK_D:      return KeyCode::D;
        case SDLK_C:      return KeyCode::C;
        default:          return KeyCode::None;
    }
}

}  // namespace

CWindow::CWindow()
    : m_window(nullptr),
      m_context(nullptr),
      m_sdlInitialised(false),
      m_quitRequested(false),
      m_dragDeltaX(0),
      m_dragDeltaY(0),
      m_wheelDelta(0.0f) {
    for (int i = 0; i < static_cast<int>(KeyCode::Count); ++i) {
        m_keyDown[i] = false;
    }
}

CWindow::~CWindow() {
    Close();
}

bool CWindow::Open(const std::string &title, int width, int height) {
    Close();
    m_quitRequested = false;
    m_keyPresses.clear();
    for (int i = 0; i < static_cast<int>(KeyCode::Count); ++i) {
        m_keyDown[i] = false;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
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

void CWindow::Close() {
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

bool CWindow::PumpEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            m_quitRequested = true;
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_ESCAPE) {
                m_quitRequested = true;
            } else {
                const KeyCode code = TranslateKey(event.key.key);
                if (code != KeyCode::None) {
                    const int index = static_cast<int>(code);
                    if (!m_keyDown[index]) {
                        m_keyPresses.push_back(code);
                    }
                    m_keyDown[index] = true;
                }
            }
        } else if (event.type == SDL_EVENT_KEY_UP) {
            const KeyCode code = TranslateKey(event.key.key);
            if (code != KeyCode::None) {
                m_keyDown[static_cast<int>(code)] = false;
            }
        } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
            // SDL reports the button state with the motion, so there is no
            // need to track presses and releases separately.
            if ((event.motion.state & SDL_BUTTON_LMASK) != 0) {
                m_dragDeltaX += static_cast<int>(event.motion.xrel);
                m_dragDeltaY += static_cast<int>(event.motion.yrel);
            }
        } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            m_wheelDelta += event.wheel.y;
        }
    }
    return !m_quitRequested;
}

void CWindow::TakeDragDelta(int &deltaX, int &deltaY) {
    deltaX = m_dragDeltaX;
    deltaY = m_dragDeltaY;
    m_dragDeltaX = 0;
    m_dragDeltaY = 0;
}

float CWindow::TakeWheelDelta() {
    const float delta = m_wheelDelta;
    m_wheelDelta = 0.0f;
    return delta;
}

KeyCode CWindow::TakeKeyPress() {
    if (m_keyPresses.empty()) {
        return KeyCode::None;
    }
    const KeyCode code = m_keyPresses.front();
    m_keyPresses.erase(m_keyPresses.begin());
    return code;
}

bool CWindow::IsKeyDown(KeyCode key) const {
    const int index = static_cast<int>(key);
    if (index <= static_cast<int>(KeyCode::None) ||
        index >= static_cast<int>(KeyCode::Count)) {
        return false;
    }
    return m_keyDown[index];
}

void CWindow::Present() {
    SDL_GL_SwapWindow(m_window);
}

bool CWindow::SaveFrame(const std::string &path) const {
    int width = 0;
    int height = 0;
    GetDrawableSize(width, height);

    PNGImage frame;
    frame.width = static_cast<std::uint32_t>(width);
    frame.height = static_cast<std::uint32_t>(height);
    frame.pixels.resize(static_cast<std::size_t>(width) * height * 4);

    // GL hands rows back bottom-up; PNG wants them top-down.
    std::vector<std::uint8_t> bottomUp(frame.pixels.size());
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bottomUp.data());
    if (!GLCheckErrors("glReadPixels")) {
        return false;
    }

    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
    for (int y = 0; y < height; ++y) {
        std::memcpy(&frame.pixels[static_cast<std::size_t>(y) * rowBytes],
                    &bottomUp[static_cast<std::size_t>(height - 1 - y) * rowBytes],
                    rowBytes);
    }

    return PNGEncode(frame, path);
}

void CWindow::GetDrawableSize(int &width, int &height) const {
    width = 0;
    height = 0;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
}

std::uint64_t CWindow::GetTicksMs() const {
    return SDL_GetTicks();
}
