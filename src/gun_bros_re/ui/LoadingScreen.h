/** Original CMenuSystem loading silhouette (core 0:124), hosted by SDL. */
#ifndef GUN_BROS_RE_LOADINGSCREEN_H
#define GUN_BROS_RE_LOADINGSCREEN_H
#include "engine/glu/movie/MovieRenderer.h"
#include "gun_bros_re/ui/OriginalLoadingSplash.h"
#include "gun_bros_re/data/PackTables.h"
#include "gun_bros_re/StartupSequence.h"
#include "engine/platform/CWindow.h"
#include "gun_bros_re/gameplay/CBGM.h"
#include <thread>
#include <chrono>

class LoadingScreen : public IPackLoadProgress {
public:
    LoadingScreen(CWindow &window, MovieRenderer &movies, PackTables &tables, const CProfileManager *profile = nullptr, bool enteringGame = false, bool startup = false, CBGM *music = nullptr)
        : m_window(window), m_tables(tables), m_movies(movies), m_startup(startup), m_music(music) {
        m_start = window.GetTicksMs();
        if (startup) {
            m_valid = LoadStartupSplash(m_title);
            if (!m_valid) { return; }
            Draw(0);
            m_tables.SetLoadProgress(this);
            std::printf("[startup-loading] launch image and original indicator; no CG\n");
            return;
        }
        // CMenuSystem::GetSplashScreenIndex :96132 cycles sequentially through normal tips.
        static unsigned nextIndex = 0;
        m_valid = m_splash.Init(movies, nextIndex, profile);
        if (!m_valid) { std::printf("[loading-splash] binding failed\n"); return; }
        nextIndex = (nextIndex + 1) % m_splash.Count();
        m_start = window.GetTicksMs();
        m_animateExit = kSplashReturnAnimateExit;
        if (enteringGame) { m_animateExit = kSplashEnterAnimateExit; }
        // CMenuSplash executes its loading action only after chapter 0 ends.
        Animate(0, m_splash.IdleStart());
        m_tables.SetLoadProgress(this);
    }
    ~LoadingScreen() { Finish(); }
    void Finish() {
        if (m_finished) { return; }
        m_finished = true;
        m_tables.SetLoadProgress(nullptr);
        if (m_valid && !m_cancelled && m_animateExit) {
            Animate(m_splash.ExitStart(), m_splash.Duration());
        }
    }
    bool IsValid() const { return m_valid; }
    bool Cancelled() const { return m_cancelled; }
    // Research captures use the production draw path before swapping buffers.
    bool CaptureFrame(const std::string &path, unsigned elapsed) {
        return Draw(elapsed, false, path);
    }
    void OnResourceRead() override {
        const auto now = m_window.GetTicksMs();
        if (now - m_lastDraw < 40) { return; }
        if (!m_window.PumpEvents()) { m_cancelled = true; }
        Draw(static_cast<unsigned>(now - m_start));
    }
private:
    void Animate(unsigned first, unsigned last) {
        const auto start = m_window.GetTicksMs();
        unsigned frame = first;
        while (m_valid && !m_cancelled) {
            if (!m_window.PumpEvents()) { m_cancelled = true; break; }
            frame = first + static_cast<unsigned>(m_window.GetTicksMs() - start);
            Draw(std::min(frame, last), true);
            if (frame >= last) { break; }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    bool Draw(unsigned elapsed, bool playChapter = false, const std::string &capturePath = {}) {
        // Loading still services the existing track; it never selects a new one.
        if (m_music != nullptr) { m_music->Update(); }
        int width = 0, height = 0;
        m_window.GetDrawableSize(width, height);
        glViewport(0, 0, width, height);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (m_startup) {
            m_movies.Image(m_title, 0, 0, 1024, 768);
            MovieRegion bounds;
            if (!m_movies.SpriteBounds(0, 124, bounds) || !m_movies.DrawSprite(0, 124, elapsed,
                1024 - bounds.width, 768 - bounds.height + std::floor(bounds.height / 4))) { m_valid = false; }
        } else if (!m_splash.Draw(elapsed, playChapter)) { m_valid = false; }
        bool captured = true;
        if (!capturePath.empty()) { captured = GB_SAVE_FRAME(m_window, capturePath); }
        m_window.Present();
        m_lastDraw = m_window.GetTicksMs();
        return m_valid && captured;
    }
    CWindow &m_window;
    PackTables &m_tables;
    MovieRenderer &m_movies;
    CTexture m_title;
    bool m_startup = false;
    OriginalLoadingSplash m_splash;
    std::uint64_t m_start = 0, m_lastDraw = 0;
    bool m_cancelled = false;
    bool m_valid = false;
    bool m_finished = false, m_animateExit = false;
    CBGM *m_music = nullptr;
};
#endif
