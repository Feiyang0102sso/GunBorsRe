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
#include <memory>
class IWindowOverlay;

struct SDL_Window;
union SDL_Event;
typedef struct SDL_GLContextState *SDL_GLContext;

/**
 * The keys the harnesses care about.
 *
 * Deliberately not SDL's keycodes: callers above the platform layer should not
 * have to include SDL to read input. Extended as new keys are needed.
 */
enum class KeyCode {
    None,
    Escape,
    Enter,
    Left,
    Right,
    Up,
    Down,
    Home,
    F3,
    T,
    P,
    G,
    X,
    R,
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
    Q,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    I,
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

    unsigned GetSurfaceId() const;
    unsigned GetSurfaceGeneration() const { return m_surfaceGeneration; }
    void GetPosition(int &x, int &y) const;

    bool IsOpen() const { return m_window != nullptr; }

    /**
     * Drain the event queue.
     * @return false once the user has asked to quit.
     */
    bool PumpEvents();
    /** A host may consume input before it enters the scene accumulators. */
    using EventFilter = bool (*)(void *, const SDL_Event &);
    void SetEventFilter(EventFilter filter, void *context) {
        m_eventFilter = filter;
        m_eventContext = context;
    }
    /** Game menus consume Escape as Back; research viewers retain Escape to quit. */
    void SetEscapeCloses(bool enabled) { m_escapeCloses = enabled; }

    /** Present the back buffer. */
    void Present();
    /** Transfers a caller-provided presentation policy to this surface across scene changes. */
    void SetPresentationOverlay(std::unique_ptr<IWindowOverlay> overlay);
    bool HasPresentationOverlay() const { return m_presentationOverlay != nullptr; }
    void DrawPresentationOverlay() const;
    /** Research benchmarks can separate draw cost from refresh-rate waiting. */
    bool SetVSync(bool enabled);

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
    /** Weapon previews reserve left click for firing and rotate with right drag. */
    void SetRightDrag(bool enabled) { m_rightDrag = enabled; }

    /** Wheel notches since the last call, positive away from the user. */
    float TakeWheelDelta();

    /**
     * Oldest key press not yet handled, removing it from the queue.
     * Returns KeyCode::None when nothing is queued, so callers can drain it
     * with a while loop.
     */
    KeyCode TakeKeyPress();
    /** Modifiers captured with the last dequeued key, even if released meanwhile. */
    bool WasShiftPressed() const { return m_lastKeyShift; }

    /** Whether a key is currently held. Intended for continuous movement. */
    bool IsKeyDown(KeyCode key) const;
    /** Mouse position in drawable pixels, including HiDPI scaling. */
    bool GetMousePosition(float &x, float &y) const;
    bool IsLeftMouseDown() const;
    void SetTitle(const std::string &title);
#if GB_ENABLE_CHEATS
    using CommandMatcher = bool (*)(std::string &, std::vector<std::string> &, char, bool);
    static void SetCommandMatcher(CommandMatcher matcher);
    void EnableCheats(bool enabled) { m_cheatsEnabled = enabled; }
    std::string TakeCheatCode();
#endif

private:
    std::unique_ptr<IWindowOverlay> m_presentationOverlay;
    SDL_Window *m_window;
    unsigned m_surfaceGeneration = 0;
    SDL_GLContext m_context;
    bool m_sdlInitialised;
    bool m_quitRequested;
    bool m_escapeCloses = true;
    EventFilter m_eventFilter = nullptr;
    void *m_eventContext = nullptr;

    // Input accumulators, drained by the Take* methods.
    int m_dragDeltaX;
    int m_dragDeltaY;
    bool m_rightDrag = false;
    float m_wheelDelta;
    struct KeyPress { KeyCode code; bool shift; };
    std::vector<KeyPress> m_keyPresses;
    bool m_lastKeyShift = false;
#if GB_ENABLE_CHEATS
    bool m_cheatsEnabled = false;
    std::string m_cheatPrefix;
    std::vector<std::string> m_cheatCodes;
    std::uint64_t m_cheatKeyTime = 0;
#endif
    bool m_keyDown[static_cast<int>(KeyCode::Count)];
};

#endif  // GUN_BROS_RE_ENGINE_PLATFORM_CWINDOW_H
