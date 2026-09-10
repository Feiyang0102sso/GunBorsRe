/** SDL binding of CMenuInviteFriends / CMenuIncentives, not a replacement layout.
 * ui_movie.bt; original Init/Bind/Draw/Update at :248330..248861 and
 * :292917..293260. All geometry, animation and captions resolve from BIG.
 */
#ifndef GUN_BROS_RE_ORIGINALPROMOTIONPOPUP_H
#define GUN_BROS_RE_ORIGINALPROMOTIONPOPUP_H
#include "runtime/OriginalTextLayout.h"

class OriginalPromotionPopup : public IMovieRegionCallback {
public:
    bool Activate(MovieRenderer &renderer, unsigned action) {
        m_movies = &renderer;
        m_action = action;
        const char *name = "GLU_MOVIE_ADD_FRIENDS_POPUP_SMALL";
        if (action == 130) { name = "GLU_MOVIE_INCENTIVES_POPUP"; }
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
    const std::vector<std::pair<MovieRegion, unsigned>> &Hits() const { return m_hits; }
    bool Draw(MovieRenderer &renderer) {
        m_movies = &renderer;
        if (!m_active) { return true; }
        m_hits.clear();
        if (!m_movies->Draw(m_ordinal, m_time, 512, 384, 1024, 768, 0, 1, this)) { return false; }
        if (m_time < m_visibleStart || m_time > m_idleEnd) { return true; }
        unsigned iconsRegion = 6, closeRegion = 7;
        unsigned left = 6, right = 5, leftAction = 126, rightAction = 127;
        if (m_action == 130) {
            iconsRegion = 3; closeRegion = 4;
            left = 16; right = 17; leftAction = 121; rightAction = 120;
        }
        MovieRegion icons, close;
        if (!m_movies->Region(m_ordinal, iconsRegion, m_time, icons) ||
            !m_movies->Region(m_ordinal, closeRegion, m_time, close)) { return false; }
        // Both original classes divide their authored icon strip into equal cells.
        for (unsigned cell = 0; cell < 2; ++cell) {
            unsigned animation = left, action = leftAction;
            if (cell == 1) { animation = right; action = rightAction; }
            const float x = icons.x + icons.width * (cell * 2 + 1) / 4;
            const float y = icons.y + icons.height / 2;
            MovieRegion bounds;
            if (!m_movies->SpriteBounds(29, animation, bounds) ||
                !m_movies->DrawSprite(29, animation, m_time, x, y)) { return false; }
            bounds.x += x; bounds.y += y;
            m_hits.push_back({bounds, action});
        }
        if (!m_movies->DrawSpriteFitted(0, 99, m_time, close.x, close.y, close.width, close.height)) { return false; }
        m_hits.push_back({close, 45});
        return true;
    }
private:
    bool DrawMovieRegion(const MovieRegion &region) override {
        // Callback bindings are native code, not resource layout values.
        static constexpr const char *inviteTitles[] = {
            "IDS_POPUP_INVITE_FRIENDS_INVITE_MORE", "IDS_POPUP_INVITE_FRIENDS_FRIENDS",
            "IDS_POPUP_INVITE_FRIENDS_GET_MORE", "IDS_POPUP_INVITE_FRIENDS_MONEY",
            "IDS_POPUP_INVITE_FRIENDS_EQUALS"};
        const char *name = nullptr;
        bool heading = false;
        if (m_action == 125) {
            if (region.index < 5) { name = inviteTitles[region.index]; heading = true; }
            if (region.index == 5) { name = "IDS_POPUP_INVITE_FRIENDS_BODY"; }
        } else {
            if (region.index == 0) { name = "IDS_POPUP_INCENTIVES_TITLE"; heading = true; }
            if (region.index == 1) { name = "IDS_POPUP_INCENTIVES_ADCOLONY"; }
            if (region.index == 2) { name = "IDS_POPUP_INCENTIVES_TAPJOY"; }
        }
        if (!name) { return true; }
        const std::string text = m_movies->NamedString(name);
        if (text.empty()) { return false; }
        if (heading) {
            float y = region.y;
            if (m_action == 125) { y += region.height / 2 - m_movies->TextHeight(6) / 2; }
            return m_movies->Text(text, region.x + region.width / 2 - m_movies->TextWidth(text, 6) / 2,
                y, 6, 1, 0, region.alpha);
        }
        const auto lines = FormatStoreText(*m_movies, text, region.width, {0, 0, 0, 0, 0});
        float height = 0;
        for (const auto &line : lines) { height += line.height; }
        float y = region.y + (region.height - height) / 2;
        for (const auto &line : lines) {
            for (const auto &run : line.runs) {
                m_movies->Text(run.text, region.x + (region.width - line.width) / 2 + run.x,
                    y, run.font, 1, 0, region.alpha);
            }
            y += line.height;
        }
        return true;
    }
    MovieRenderer *m_movies = nullptr;
    unsigned m_action = 0, m_ordinal = 0, m_time = 0, m_duration = 0;
    unsigned m_visibleStart = 0, m_visibleEnd = 0, m_idleStart = 0, m_idleEnd = 0;
    bool m_active = false, m_closing = false;
    std::vector<std::pair<MovieRegion, unsigned>> m_hits;
};
#endif
