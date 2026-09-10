/** CMenuSystem::Transition1Callback/Transition2Callback :96250..96279.
 * GPU surfaces adapt the two menu draws to this host; BIG WIPE owns clips,
 * ordering, sweep artwork and duration. There are no hand-authored keyframes.
 */
#ifndef GUN_BROS_RE_MENUWIPE_H
#define GUN_BROS_RE_MENUWIPE_H
#include "runtime/MovieRenderer.h"
class MenuWipe : public IMovieRegionCallback {
public:
    bool Remember() { return m_old.CaptureFramebuffer(); }
    bool Begin(MovieRenderer &movies) {
        if (!m_old.IsValid()) { return true; }
        m_movies = &movies;
        m_ordinal = movies.Ordinal("GLU_MOVIE_WIPE");
        const auto *movie = movies.GetMovie(m_ordinal);
        if (!movie || movie->duration == 0) { return false; }
        m_duration = movie->duration;
        m_time = 0;
        m_active = true;
        return true;
    }
    bool IsActive() const { return m_active; }
    unsigned Time() const { return m_time; }
    unsigned Duration() const { return m_duration; }
    void Update(unsigned delta) {
        if (!m_active) { return; }
        m_time = std::min(m_duration, m_time + delta);
        if (m_time == m_duration) { m_active = false; }
    }
    bool Draw() {
        if (!m_active) { return true; }
        if (!m_new.CaptureFramebuffer()) { return false; }
        return m_movies->Draw(m_ordinal, m_time, 512, 384, 1024, 768, 0, 1, this);
    }
private:
    bool DrawMovieRegion(const MovieRegion &region) override {
        if (region.index > 1) { return true; }
        GLint viewport[4], previous[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_SCISSOR_BOX, previous);
        const GLboolean clipped = glIsEnabled(GL_SCISSOR_TEST);
        glEnable(GL_SCISSOR_TEST);
        glScissor(int(region.x * viewport[2] / 1024), int((768 - region.y - region.height) * viewport[3] / 768),
            std::max(0, int(region.width * viewport[2] / 1024)), std::max(0, int(region.height * viewport[3] / 768)));
        const CTexture *surface = &m_old;
        if (region.index == 1) { surface = &m_new; }
        // Framebuffer textures have a bottom-left origin. Flip V only;
        // a negative fitted height would also reverse X through its scale.
        m_movies->Image(*surface, 0, 0, 1024, 768, true);
        glScissor(previous[0], previous[1], previous[2], previous[3]);
        if (!clipped) { glDisable(GL_SCISSOR_TEST); }
        return true;
    }
    MovieRenderer *m_movies = nullptr;
    CTexture m_old, m_new;
    unsigned m_ordinal = 0, m_duration = 0, m_time = 0;
    bool m_active = false;
};
#endif
