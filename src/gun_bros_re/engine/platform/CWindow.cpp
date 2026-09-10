/**
 * @file CWindow.cpp
 * @brief Window and OpenGL context, on SDL3.
 */

#include "engine/platform/CWindow.h"
#define NOMINMAX
#include <Windows.h>
#include "engine/platform/ResearchLauncher.h"
#include "engine/CAudioPlayer.h"

#include "engine/CPNG.h"
#include "engine/platform/GLLoader.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr int kUsableDisplayPercent = 90;
// Windows system-menu commands occupy the low range below 0xF000.
constexpr UINT kResearchToolsCommand = 0x1FE0;

bool SDLCALL HandleWindowsMenu(void *, MSG *message) {
    if (message->message != WM_SYSCOMMAND || (message->wParam & 0xFFF0) != kResearchToolsCommand) { return true; }
    const wchar_t *arguments = L"--research";
    if (CAudioPlayer::IsMuted()) { arguments = L"--research --mute"; }
    LaunchResearchTools(arguments, true);
    return false;
}

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
        case SDLK_ESCAPE: return KeyCode::Escape;
        case SDLK_RETURN: return KeyCode::Enter;
        case SDLK_LEFT:   return KeyCode::Left;
        case SDLK_RIGHT:  return KeyCode::Right;
        case SDLK_UP:     return KeyCode::Up;
        case SDLK_DOWN:   return KeyCode::Down;
        case SDLK_HOME:   return KeyCode::Home;
        case SDLK_T:      return KeyCode::T;
        case SDLK_P:      return KeyCode::P;
        case SDLK_G:      return KeyCode::G;
        case SDLK_X:      return KeyCode::X;
        case SDLK_R:      return KeyCode::R;
        case SDLK_K:      return KeyCode::K;
        case SDLK_M:      return KeyCode::M;
        case SDLK_N:      return KeyCode::N;
        case SDLK_SPACE:  return KeyCode::Space;
        case SDLK_PERIOD: return KeyCode::Period;
        case SDLK_W:      return KeyCode::W;
        case SDLK_A:      return KeyCode::A;
        case SDLK_S:      return KeyCode::S;
        case SDLK_D:      return KeyCode::D;
        case SDLK_B:      return KeyCode::B;
        case SDLK_C:      return KeyCode::C;
        case SDLK_E:      return KeyCode::E;
        case SDLK_F:      return KeyCode::F;
        case SDLK_Q:      return KeyCode::Q;
        case SDLK_1:      return KeyCode::Digit1;
        case SDLK_2:      return KeyCode::Digit2;
        case SDLK_3:      return KeyCode::Digit3;
        case SDLK_4:      return KeyCode::Digit4;
        case SDLK_5:      return KeyCode::Digit5;
        case SDLK_6:      return KeyCode::Digit6;
        case SDLK_7:      return KeyCode::Digit7;
        case SDLK_8:      return KeyCode::Digit8;
        case SDLK_9:      return KeyCode::Digit9;
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
    for (int i = 0; i < static_cast<int>(KeyCode::Count); ++i) {
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
    // Host tool access lives in the window chrome, outside original BIG menus.
    // Right-click the title bar / Alt+Space opens the permanent research entry.
    const auto properties = SDL_GetWindowProperties(m_window);
    const HWND handle = static_cast<HWND>(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (handle != nullptr) {
        const HMENU menu = GetSystemMenu(handle, FALSE);
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kResearchToolsCommand, L"Research tools...");
        SDL_SetWindowsMessageHook(HandleWindowsMenu, nullptr);
    }

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

unsigned CWindow::GetSurfaceId() const { return SDL_GetWindowID(m_window); }

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
            if (m_cheatsEnabled && !event.key.repeat && event.key.key >= SDLK_A && event.key.key <= SDLK_Z) {
                const auto now = SDL_GetTicks();
                if (now - m_cheatKeyTime > 2500) { m_cheatPrefix.clear(); }
                m_cheatKeyTime = now;
                const char letter = static_cast<char>(event.key.key);
                // Desktop Boss shortcut is six letters. A lone S still reaches
                // movement; only an established ST prefix consumes its suffix.
                if (m_cheatPrefix.size() >= 2 && m_cheatPrefix.compare(0, 2, "st") == 0) {
                    m_cheatPrefix += letter;
                    if (m_cheatPrefix == "stboss") {
                        m_cheatCodes.push_back(m_cheatPrefix);
                        m_cheatPrefix.clear();
                        continue;
                    }
                    if (std::string("stboss").compare(0, m_cheatPrefix.size(), m_cheatPrefix) == 0) { continue; }
                    m_cheatPrefix.clear();
                }
                if (m_cheatPrefix == "s" && letter == 't') { m_cheatPrefix = "st"; continue; }
                if (m_cheatPrefix == "ch") {
                    if (std::strchr("mtdchiw", letter) != nullptr) {
                        m_cheatCodes.push_back(m_cheatPrefix + letter);
                        m_cheatPrefix.clear();
                        continue; // A completed cheat must not also trigger a weapon hotkey.
                    }
                    m_cheatPrefix.clear();
                }
                if (m_cheatPrefix == "c" && letter == 'h') { m_cheatPrefix = "ch"; continue; }
                if (letter == 'c') { m_cheatPrefix = "c"; continue; }
                m_cheatPrefix.clear();
                if (letter == 's') { m_cheatPrefix = "s"; }
            }
            if (event.key.key == SDLK_ESCAPE && m_escapeCloses) {
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
            for (int i = 0; i < static_cast<int>(KeyCode::Count); ++i) {
                m_keyDown[i] = false;
            }
            m_keyPresses.clear();
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

bool CWindow::SetVSync(bool enabled) {
    int interval = 0;
    if (enabled) { interval = 1; }
    return SDL_GL_SetSwapInterval(interval);
}

std::string CWindow::TakeCheatCode() {
    if (m_cheatCodes.empty()) { return {}; }
    const std::string code = m_cheatCodes.front();
    m_cheatCodes.erase(m_cheatCodes.begin());
    return code;
}

bool CWindow::GetMousePosition(float &x, float &y) const {
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

bool CWindow::IsLeftMouseDown() const {
    return SDL_GetKeyboardFocus() == m_window &&
        (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK) != 0;
}

void CWindow::SetTitle(const std::string &title) {
    SDL_SetWindowTitle(m_window, title.c_str());
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

void CWindow::GetPosition(int &x, int &y) const {
    SDL_GetWindowPosition(m_window, &x, &y);
}

std::uint64_t CWindow::GetTicksMs() const {
    return SDL_GetTicks();
}
