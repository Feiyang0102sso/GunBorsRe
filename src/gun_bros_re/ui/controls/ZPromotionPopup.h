/** SDL binding of CMenuInviteFriends / CMenuIncentives, not a replacement layout.
 * ui_movie.bt; original Init/Bind/Draw/Update at :248330..248861 and
 * :292917..293260. All geometry, animation and captions resolve from BIG.
 */
#ifndef GUN_BROS_RE_ZPROMOTIONPOPUP_H
#define GUN_BROS_RE_ZPROMOTIONPOPUP_H
#include "gun_bros_re/ui/menus/CMenuInviteFriends.h"
#include "gun_bros_re/ui/menus/CMenuIncentives.h"

class ZPromotionPopup : public ZMovieRegionCallback {
public:
// Draw authored regions with the native menu's animation/action bindings.
static bool DrawControls(ZMovieRenderer &movies, unsigned ordinal, unsigned time,
    unsigned iconsRegion, unsigned closeRegion, const std::array<unsigned, 2> &animations,
    const std::array<unsigned, 2> &actions, std::vector<std::pair<ZMovieRegion, unsigned>> &hits) {
    ZMovieRegion icons, close;
    if (!movies.Region(ordinal, iconsRegion, time, icons) ||
        !movies.Region(ordinal, closeRegion, time, close)) { return false; }
    // Both original classes divide their authored icon strip into equal cells.
    for (unsigned cell = 0; cell < 2; ++cell) {
        unsigned animation = animations[0], action = actions[0];
        if (cell == 1) { animation = animations[1]; action = actions[1]; }
        const float x = icons.x + icons.width * (cell * 2 + 1) / 4;
        const float y = icons.y + icons.height / 2;
        ZMovieRegion bounds;
        if (!movies.SpriteBounds(29, animation, bounds) ||
            !movies.DrawSprite(29, animation, time, x, y)) { return false; }
        bounds.x += x; bounds.y += y;
        hits.push_back({bounds, action});
    }
    if (!movies.DrawSpriteFitted(0, 99, time, close.x, close.y, close.width, close.height)) { return false; }
    hits.push_back({close, 45});
    return true;
}

    bool Activate(ZMovieRenderer &renderer, unsigned action) {
        m_movies = &renderer;
        m_action = action;
        const char *name = CMenuInviteFriends::MovieName();
        if (action == 130) { name = CMenuIncentives::MovieName(); }
        m_ordinal = renderer.Ordinal(name);
        const CMovie *movie = renderer.GetMovie(m_ordinal);
        if (!movie || !movie->GetChapterRange(1, m_visibleStart, m_visibleEnd) ||
            !movie->GetChapterRange(2, m_idleStart, m_idleEnd)) { return false; }
        m_duration = movie->duration;
        m_time = 0;
        m_active = true;
        m_closing = false;
        m_hits.clear();
        return true;
    }
    bool IsActive() const { return m_active; }
    bool IsReady() const { return m_active && !m_closing && m_time >= m_idleStart; }
    unsigned Action() const { return m_action; }
    unsigned Time() const { return m_time; }
    unsigned Ordinal() const { return m_ordinal; }
    void Dismiss() {
        if (!IsReady()) { return; }
        // Dismiss plays from chapter 2 through the end, without its idle loop.
        m_closing = true;
        m_time = m_idleStart;
    }
    void Update(unsigned delta) {
        if (!m_active) { return; }
        m_time += delta;
        if (m_closing) {
            if (m_time >= m_duration) { m_time = m_duration; m_active = false; }
        } else if (m_time > m_idleEnd) {
            m_time = m_idleStart + (m_time - m_idleStart) % (m_idleEnd - m_idleStart + 1);
        }
    }
    /** Windows shows the original FB/Game Center choices as reference endpoints.
     * A click returns the original action; no success, invite or reward is faked.
     */
    unsigned Click(float x, float y) {
        if (!IsReady()) { return 0; }
        for (const auto &hit : m_hits) {
            if (!hit.first.Contains(x, y)) { continue; }
            if (hit.second == 45) { Dismiss(); }
            return hit.second;
        }
        return 0;
    }
    const std::vector<std::pair<ZMovieRegion, unsigned>> &Hits() const { return m_hits; }
    bool Draw(ZMovieRenderer &renderer) {
        m_movies = &renderer;
        if (!m_active) { return true; }
        m_hits.clear();
        if (!m_movies->Draw(m_ordinal, m_time, 512, 384, 1024, 768, 0, 1, this)) { return false; }
        if (m_time < m_visibleStart || m_time > m_idleEnd) { return true; }
        if (m_action == 125) { return CMenuInviteFriends::DrawControls(renderer, m_ordinal, m_time, m_hits); }
        return CMenuIncentives::DrawControls(renderer, m_ordinal, m_time, m_hits);
    }

private:
    bool DrawMovieRegion(const ZMovieRegion &region) override {
        if (m_action == 125) { return CMenuInviteFriends::DrawRegion(*m_movies, region); }
        return CMenuIncentives::DrawRegion(*m_movies, region);
    }
    ZMovieRenderer *m_movies = nullptr;
    unsigned m_action = 0, m_ordinal = 0, m_time = 0, m_duration = 0;
    unsigned m_visibleStart = 0, m_visibleEnd = 0, m_idleStart = 0, m_idleEnd = 0;
    bool m_active = false, m_closing = false;
    std::vector<std::pair<ZMovieRegion, unsigned>> m_hits;
};
#endif
