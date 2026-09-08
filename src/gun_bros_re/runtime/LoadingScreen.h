/** Original CMenuSystem loading silhouette (core 0:124), hosted by SDL. */
#ifndef GUN_BROS_RE_LOADINGSCREEN_H
#define GUN_BROS_RE_LOADINGSCREEN_H
#include "runtime/MovieRenderer.h"
#include "runtime/PackTables.h"
#include "engine/platform/CWindow.h"

class LoadingScreen : public IPackLoadProgress {
public:
    LoadingScreen(CWindow &window, MovieRenderer &movies, PackTables &tables)
        : m_window(window), m_movies(movies), m_tables(tables) {
        m_start = window.GetTicksMs();
        Draw(0);
        m_tables.SetLoadProgress(this);
    }
    ~LoadingScreen() { Finish(); }
    void Finish() { m_tables.SetLoadProgress(nullptr); }
    bool Cancelled() const { return m_cancelled; }
    void OnResourceRead() override {
        const auto now = m_window.GetTicksMs();
        if (now - m_lastDraw < 40) { return; }
        if (!m_window.PumpEvents()) { m_cancelled = true; }
        Draw(static_cast<unsigned>(now - m_start));
    }
private:
    void Draw(unsigned elapsed) {
        int width = 0, height = 0;
        m_window.GetDrawableSize(width, height);
        glViewport(0, 0, width, height);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_DEPTH_TEST);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        m_movies.DrawSpriteFitted(0, 124, elapsed, 940, 674, 64, 74);
        m_window.Present();
        m_lastDraw = m_window.GetTicksMs();
    }
    CWindow &m_window;
    MovieRenderer &m_movies;
    PackTables &m_tables;
    std::uint64_t m_start = 0, m_lastDraw = 0;
    bool m_cancelled = false;
};
#endif
