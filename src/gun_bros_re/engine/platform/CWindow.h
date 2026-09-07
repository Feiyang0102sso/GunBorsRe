/**
 * @file CWindow.h
 * @brief Window and OpenGL context, on SDL3.
 *
 * Fills the role platform/shared/cocoa/ played on iOS -- CCore_Cocoa plus
 * CRenderSurface_OGLES_Window_Cocoa -- without inheriting their shape. The
 * original had to juggle EAGL layers and view controllers; SDL3 collapses all
 * of that into a handful of calls, so this stays a thin wrapper rather than a
 * port.
 */

#ifndef GUN_BROS_RE_ENGINE_PLATFORM_CWINDOW_H
#define GUN_BROS_RE_ENGINE_PLATFORM_CWINDOW_H

#include <cstdint>
#include <string>
#include <vector>

struct SDL_Window;
typedef struct SDL_GLContextState *SDL_GLContext;

/**
 * The keys the harnesses care about.
 *
 * Deliberately not SDL's keycodes: callers above the platform layer should not
 * have to include SDL to read input. Extended as new keys are needed.
 */
enum class KeyCode {
    None,
    Left,
    Right,
    Up,
    Down,
    Home,
    T,
    P,
    G,
    K,
    M,
    N,
    Space,
    Period,
    W,
    A,
    S,
    D,
    B,
    C,
    E,
    F,
    Count,
};

// A large but desktop-friendly 4:3 target. CWindow also scales this down when
// the current display's usable area is smaller, so the title bar and all four
// edges remain reachable.
constexpr int kDefaultWindowWidth = 1600;
constexpr int kDefaultWindowHeight = 1200;

// GL 3.3 Core is the target: same mental model as the ES 2.0 the engine was
// written against, so the shaders port across with a change of keywords.
constexpr int kRequiredGLMajor = 3;
constexpr int kRequiredGLMinor = 3;

/**
 * An open window with a current GL context.
 *
 * Owns SDL initialisation as well, so constructing one is the whole of
 * platform start-up.
 */
class CWindow {
public:
    CWindow();
    ~CWindow();

    CWindow(const CWindow &) = delete;
    CWindow &operator=(const CWindow &) = delete;

    /**
     * Open the window, create the GL context, and resolve the GL entry points.
     * Returns false with a printed reason if any step fails.
     */
    bool Open(const std::string &title, int width, int height);

    /** Tear everything down. Safe to call when not open. */
    void Close();

    bool IsOpen() const { return m_window != nullptr; }

    /**
     * Drain the event queue.
     * @return false once the user has asked to quit.
     */
    bool PumpEvents();

    /** Present the back buffer. */
    void Present();

    /**
     * Read the drawn frame back and write it out as a PNG.
     *
     * The whole reason the PNG encoder exists: a milestone that saves its
     * first frame can be signed off without a human at the keyboard. Call it
     * before Present.
     */
    bool SaveFrame(const std::string &path) const;

    /** Current drawable size in pixels, which is what glViewport wants. */
    void GetDrawableSize(int &width, int &height) const;

    /** Milliseconds since platform start-up. */
    std::uint64_t GetTicksMs() const;

    /**
     * Mouse movement accumulated while the left button was held, in pixels,
     * and reset by the call. Lets a caller drag a view around without
     * tracking button state itself.
     */
    void TakeDragDelta(int &deltaX, int &deltaY);

    /** Wheel notches since the last call, positive away from the user. */
    float TakeWheelDelta();

    /**
     * Oldest key press not yet handled, removing it from the queue.
     * Returns KeyCode::None when nothing is queued, so callers can drain it
     * with a while loop.
     */
    KeyCode TakeKeyPress();

    /** Whether a key is currently held. Intended for continuous movement. */
    bool IsKeyDown(KeyCode key) const;

private:
    SDL_Window *m_window;
    SDL_GLContext m_context;
    bool m_sdlInitialised;
    bool m_quitRequested;

    // Input accumulators, drained by the Take* methods.
    int m_dragDeltaX;
    int m_dragDeltaY;
    float m_wheelDelta;
    std::vector<KeyCode> m_keyPresses;
    bool m_keyDown[static_cast<int>(KeyCode::Count)];
};

#endif  // GUN_BROS_RE_ENGINE_PLATFORM_CWINDOW_H
