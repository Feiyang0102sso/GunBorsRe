/** @file SurvivalHud.cpp
 * @brief CInputPad::Base::Bind (:88320) binds meters to regions 0/1 and guns to 2/3.
 */
#define NOMINMAX
#include "gun_bros_re/ui/SurvivalHud.h"
#include "gun_bros_re/ui/OriginalMenuData.h"
#include "gun_bros_re/ui/OriginalTextLayout.h"
#include "gun_bros_re/HostSettings.h"
#include "gun_bros_re/data/PowerupCatalog.h"
#include "engine/platform/CWindow.h"
#include "engine/resources/CResTOCManager.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "engine/graphics/CPNG.h"
#include <algorithm>
#include <cstdio>
#include <sstream>

void SurvivalHud::ResetNotices() {
    m_notices.clear();
    m_observedProgress = false;
    m_interstitialCompleted = false;
}

bool SurvivalHud::DrawExperienceTexts(const std::vector<CombatScene::ExperienceText> &texts, bool horde) {
    // CLevel::Bind :121860 selects the original STR. TextEffect::Draw ARM
    // 0x394D4..0x394F4 centers font 9 on both axes; it never scales with zoom.
    const char *name = "IDS_HUD_EXPERIENCE_UP";
    if (horde) { name = "IDS_HUD_POINTS_UP"; }
    for (const auto &effect : texts) {
        const std::string text = OriginalNoticeNumber(name, effect.amount);
        if (text.empty() || text.find('%') != std::string::npos) { return false; }
        const float x = effect.x - static_cast<int>(m_movies.TextWidth(text, 9)) / 2;
        const float y = effect.y - static_cast<int>(m_movies.TextHeight(9)) / 2;
        if (!m_movies.Text(text, x, y, 9, 1, 0, effect.alpha)) { return false; }
    }
    return true;
}

void SurvivalHud::QueueOriginalNotice(const char *movie, const std::string &title, const std::string &footer, bool releaseLevel) {
    // SetUpOverlay :87596 uses a six-slot ring with one empty slot.
    if (m_notices.size() >= 5) { return; }
    m_notices.push_back({m_movies.Ordinal(movie), 0, title, footer, releaseLevel});
}

bool SurvivalHud::HasInterstitial() const {
    for (const auto &notice : m_notices) { if (notice.releaseLevel) { return true; } }
    return false;
}

bool SurvivalHud::TakeInterstitialCompletion() {
    const bool completed = m_interstitialCompleted;
    m_interstitialCompleted = false;
    return completed;
}

void SurvivalHud::BeginOriginalLevel(unsigned wave, bool horde, bool boss) {
    ResetNotices();
    const char *name = "IDS_HUD_WAVE_START";
    if (horde) { name = "IDS_HUD_HORDE_START"; }
    std::string text = OriginalNoticeNumber(name, wave);
    if (boss) { text = m_movies.NamedString("IDS_HUD_BOSS_WAVE_START"); }
    QueueOriginalNotice("GLU_MOVIE_WAVE_CLEARED", text, "", true);
    std::printf("[hud-overlay] begin wave=%u horde=%d boss=%d text=%s\n", wave, horde, boss, text.c_str());
}

void SurvivalHud::OnOriginalWaveClear(unsigned wave, bool perfect, unsigned rewardPercent, bool boss) {
    // OnWaveClear :89760 discards previous notices before its new sequence.
    m_notices.clear();
    m_interstitialCompleted = false;
    std::string text = OriginalNoticeNumber("IDS_HUD_WAVE_CLEAR", wave);
    // Original ARM 0x626A0..0x6271C loads the boss string but never copies it
    // to the zeroed output buffer. Preserve this observed original defect.
    if (boss) { text.clear(); }
    QueueOriginalNotice("GLU_MOVIE_WAVE_CLEARED", text, "", !perfect);
    if (perfect) {
        QueueOriginalNotice("GLU_MOVIE_PERFECT_WAVE", m_movies.NamedString("IDS_HUD_WAVE_PERFECT"),
            OriginalNoticeNumber("IDS_HUD_WAVE_PERFECT_SUMMARY", rewardPercent), true);
    }
    std::printf("[hud-overlay] clear wave=%u perfect=%d reward=%u boss=%d\n", wave, perfect, rewardPercent, boss);
}

void SurvivalHud::ObserveProgress(const SurvivalHudState &state) {
    // Native wave events come directly from the LEVEL consumer, not from
    // comparing two rendered snapshots. Reloading an account is no level-up.
    if (m_observedProgress && state.level > m_previousLevel) {
        QueueOriginalNotice("GLU_MOVIE_LEVEL_UP", OriginalNoticeNumber("IDS_HUD_LEVEL_REACHED", state.level));
        QueueOriginalNotice("GLU_MOVIE_LEVEL_UP", m_movies.NamedString("IDS_HUD_HEALTH_UP"));
    }
    m_previousLevel = state.level;
    m_observedProgress = true;
}

void SurvivalHud::Advance(int deltaMs) {
    if (deltaMs > 0) {
        m_controlTime += deltaMs;
        for (auto &meter : m_meters) { meter.Update(deltaMs); }
    }
    if (deltaMs <= 0 || m_notices.empty()) { return; }
    Notice &notice = m_notices.front();
    notice.elapsed += static_cast<unsigned>(deltaMs);
    CMovie *movie = m_movies.GetMovie(notice.movie);
    if (movie != nullptr && notice.elapsed >= movie->duration) {
        if (notice.releaseLevel) { m_interstitialCompleted = true; }
        m_notices.erase(m_notices.begin());
    }
}

void SurvivalHud::DrawNotice() {
    if (m_notices.empty()) { return; }
    const Notice &notice = m_notices.front();
    {
        // OverlayDraw :86532 uses font 11 at its original size and centers in
        // the Movie region. Drawing as a callback preserves authored layering.
        class OverlayCallback : public IMovieRegionCallback {
        public:
            OverlayCallback(MovieRenderer &renderer, const Notice &current) : movies(renderer), notice(current) {}
            bool DrawMovieRegion(const MovieRegion &area) override {
                if (area.index > 1) { return true; }
                const std::string *text = &notice.title;
                if (area.index == 1) { text = &notice.footer; }
                if (text->empty()) { return true; }
                const float x = area.x + int(area.width) / 2 - int(movies.TextWidth(*text, 11)) / 2;
                const float y = area.y + int(area.height) / 2 - int(movies.TextHeight(11)) / 2;
                return movies.Text(*text, x, y, 11, 1, 0, area.alpha);
            }
            MovieRenderer &movies;
            const Notice &notice;
        } callback(m_movies, notice);
        m_movies.Draw(notice.movie, notice.elapsed, 512, 384, 1024, 768, 0, 1, &callback);
        return;
    }
}