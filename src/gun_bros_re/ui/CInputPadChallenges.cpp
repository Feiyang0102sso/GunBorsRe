/** CInputPad challenge overlay; all geometry and timing comes from BIG Movies. */
#define NOMINMAX
#include "gun_bros_re/ui/CInputPad.h"
#include "gun_bros_re/ZHostSettings.h"
#include <algorithm>

bool CInputPad::HasChallenges() const {
    return GameHostSettings().isConnected && m_challenges && !m_challenges->current.empty();
}

namespace {
class ZChallengeCard : public ZMovieRegionCallback {
public:
    ZChallengeCard(ZMovieRenderer &renderer, const CChallengeManager::Challenge &value) : movies(renderer), challenge(value) {}
    bool DrawMovieRegion(const ZMovieRegion &area) override {
        // CMenuChallengeOption::Init binds name/progress/description to 1/2/3.
        if (area.index == 1) { return movies.Text(challenge.name, area.x, area.y, 0, 1, 0, area.alpha); }
        if (area.index == 3) { return movies.Text(challenge.description, area.x, area.y, 1, 1, 0, area.alpha); }
        if (area.index != 2) { return true; }
        for (const char *name : {"GLU_MOVIE_BRO_OP_METER_BLUE", "GLU_MOVIE_BRO_OP_METER"}) {
            const unsigned id = movies.Ordinal(name);
            const auto *movie = movies.GetMovie(id);
            if (!movie || !movies.Draw(id, movie->duration * std::min(100u, challenge.progress) / 100,
                area.x, area.y, 1024, 768, 0, area.alpha)) { return false; }
        }
        const std::string text = std::to_string(challenge.achieved) + "/" + std::to_string(challenge.target);
        return movies.Text(text, area.x + area.width - movies.TextWidth(text, 1), area.y, 1, 1, 0, area.alpha);
    }
private:
    ZMovieRenderer &movies;
    const CChallengeManager::Challenge &challenge;
};

class ZChallengeList : public ZMovieRegionCallback {
public:
    ZChallengeList(ZMovieRenderer &renderer, const CChallengeManager &manager, unsigned time, unsigned &drawn) :
        movies(renderer), challenges(manager), elapsed(time), rows(drawn) {}
    bool DrawMovieRegion(const ZMovieRegion &area) override {
        if (area.index == 0 || area.index > challenges.current.size()) { return true; }
        const auto &challenge = challenges.current[area.index - 1];
        const auto id = movies.Ordinal("GLU_MOVIE_BRO_OPS_OVERLAY_BOX");
        const auto *movie = movies.GetMovie(id);
        unsigned chapter = 0, start = 0, end = 0;
        if (challenge.applicable) { chapter = 1; }
        if (challenge.progress == 100) { chapter = 2; }
        if (!movie || !movie->GetChapterRange(chapter, start, end)) { return false; }
        ZChallengeCard callback(movies, challenge);
        ++rows;
        return movies.Draw(id, start + std::min(elapsed, end - start), area.x, area.y, 1024, 768, 0, area.alpha, &callback);
    }
private:
    ZMovieRenderer &movies;
    const CChallengeManager &challenges;
    unsigned elapsed;
    unsigned &rows;
};
}

bool CInputPad::DrawChallengeOverlay(float x, float y, unsigned elapsed, float alpha) {
    if (!HasChallenges()) { return true; }
    const auto id = m_resources.m_movies.Ordinal("GLU_MOVIE_BRO_OPS_OVERLAY_SCROLL");
    const auto *movie = m_resources.m_movies.GetMovie(id);
    unsigned start = 0, end = 0;
    if (!movie || !movie->GetChapterRange(0, start, end)) { return false; }
    ZChallengeList callback(m_resources.m_movies, *m_challenges, elapsed, m_challengeRows);
    return m_resources.m_movies.Draw(id, start + std::min(elapsed, end - start), x, y, 1024, 768, 0, alpha, &callback);
}
