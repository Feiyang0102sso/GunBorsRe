/** @file CInputPadNotices.cpp
 * @brief CInputPad::Base::Bind (:88320) binds meters to regions 0/1 and guns to 2/3.
 */
#define NOMINMAX
#include "gun_bros_re/ui/hud/CInputPad.h"
#include "gun_bros_re/ui/content/CMenuDataProvider.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
#include "gun_bros_re/host/ZHostSettings.h"
#include "gun_bros_re/gameplay/powerup/CPowerup.h"
#include "engine/platform/ZWindow.h"
#include "engine/resources/CResTOCManager.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "engine/graphics/ZPNG.h"
#include <algorithm>
#include <cstdio>
#include <sstream>

void CInputPad::ResetNotices() {
    m_notices.clear();
    m_matchMessages.clear();
    m_previousMatchScore[0] = 0; m_previousMatchScore[1] = 0;
    m_challengeHeld = false;
    m_challengeTime = 0;
    m_observedProgress = false;
    m_interstitialCompleted = false;
    m_liveWaveRemaining = 0;
    m_matchIntro = false; m_matchIntroCompleted = false;
    m_matchWrapUp = false; m_matchWrapUpTime = 0;
}

void CInputPad::BeginDeathmatch(unsigned killLimit) {
    ResetNotices();
    // OnDeathMatchStart :89958 queues the same sliding Movie as wave starts.
    const char *name = "IDS_HUD_DEATH_MATCH_START2";
    if (killLimit > 0) { name = "IDS_HUD_DEATH_MATCH_START1"; }
    QueueNotice("GLU_MOVIE_WAVE_CLEARED", NoticeNumber(name, killLimit));
    m_matchIntro = true;
}

bool CInputPad::TakeDeathmatchIntroCompletion() {
    const bool complete = m_matchIntroCompleted;
    m_matchIntroCompleted = false;
    return complete;
}

void CInputPad::AddMatchMessage(const std::string &text) {
    // MPMatchConsoleMessageAdd :87241 shares the five-entry queue for all events.
    if (m_matchMessages.size() == 5) { m_matchMessages.erase(m_matchMessages.begin()); }
    m_matchMessages.push_back({text});
}

void CInputPad::OnDeathmatchPowerup(const std::string &name) {
    // OnDeathMatchUsePowerUp :87783 formats the original POWERUP name.
    std::string text = m_resources.m_movies.NamedString("IDS_HUD_DEATH_MATCH_USE_POWERUP");
    const auto marker = text.find("%s");
    if (marker != std::string::npos) { text.replace(marker, 2, name); }
    AddMatchMessage(text);
    std::printf("[hud-match] powerup=%s text=%s\n", name.c_str(), text.c_str());
}

void CInputPad::AdvanceDeathmatchWrapUp(unsigned deltaMs) {
    // CGame::SetMissionWrapUp :75619 runs MISSION_END before ShowWrapUpMenu.
    if (!m_matchWrapUp) {
        m_matchWrapUp = true; m_matchWrapUpTime = 0;
        const auto *movie = m_resources.m_movies.GetMovie(m_resources.m_movies.Ordinal("GLU_MOVIE_MISSION_END"));
        if (movie != nullptr) { m_matchWrapUpDuration = movie->duration; }
        return;
    }
    m_matchWrapUpTime = std::min(m_matchWrapUpDuration, m_matchWrapUpTime + deltaMs);
}

bool CInputPad::IsDeathmatchWrapUpComplete() const {
    return m_matchWrapUp && m_matchWrapUpDuration > 0 && m_matchWrapUpTime >= m_matchWrapUpDuration;
}

bool CInputPad::DrawExperienceTexts(const std::vector<CLevel::ExperienceText> &texts, bool horde) {
    // CLevel::Bind :121860 selects the original STR. TextEffect::Draw ARM
    // 0x394D4..0x394F4 centers font 9 on both axes; it never scales with zoom.
    const char *name = "IDS_HUD_EXPERIENCE_UP";
    if (horde) { name = "IDS_HUD_POINTS_UP"; }
    for (const auto &effect : texts) {
        const std::string text = NoticeNumber(name, effect.amount);
        if (text.empty() || text.find('%') != std::string::npos) { return false; }
        const float x = effect.x - static_cast<int>(m_resources.m_movies.TextWidth(text, 9)) / 2;
        const float y = effect.y - static_cast<int>(m_resources.m_movies.TextHeight(9)) / 2;
        if (!m_resources.m_movies.Text(text, x, y, 9, 1, 0, effect.alpha)) { return false; }
    }
    return true;
}

void CInputPad::QueueNotice(const char *movie, const std::string &title, const std::string &footer, bool releaseLevel) {
    // SetUpOverlay :87596 uses a six-slot ring with one empty slot.
    if (m_notices.size() >= 5) { return; }
    m_notices.push_back({m_resources.m_movies.Ordinal(movie), 0, title, footer, releaseLevel});
}

bool CInputPad::HasInterstitial() const {
    for (const auto &notice : m_notices) { if (notice.releaseLevel) { return true; } }
    return false;
}

bool CInputPad::TakeInterstitialCompletion() {
    const bool completed = m_interstitialCompleted;
    m_interstitialCompleted = false;
    return completed;
}

void CInputPad::BeginLevel(unsigned wave, bool horde, bool boss) {
    ResetNotices();
    const char *name = "IDS_HUD_WAVE_START";
    if (horde) { name = "IDS_HUD_HORDE_START"; }
    std::string text = NoticeNumber(name, wave);
    if (boss) { text = m_resources.m_movies.NamedString("IDS_HUD_BOSS_WAVE_START"); }
    QueueNotice("GLU_MOVIE_WAVE_CLEARED", text, "", true);
    std::printf("[hud-overlay] begin wave=%u horde=%d boss=%d text=%s\n", wave, horde, boss, text.c_str());
}

void CInputPad::OnWaveClear(unsigned wave, bool perfect, unsigned rewardPercent, bool boss) {
    // OnWaveClear :89760 discards previous notices before its new sequence.
    m_notices.clear();
    m_challengeHeld = false;
    m_challengeTime = 0;
    m_interstitialCompleted = false;
    std::string text = NoticeNumber("IDS_HUD_WAVE_CLEAR", wave);
    // Original ARM 0x626A0..0x6271C loads the boss string but never copies it
    // to the zeroed output buffer. Preserve this observed original defect.
    if (boss) { text.clear(); }
    QueueNotice("GLU_MOVIE_WAVE_CLEARED", text, "", !perfect);
    if (perfect) {
        QueueNotice("GLU_MOVIE_PERFECT_WAVE", m_resources.m_movies.NamedString("IDS_HUD_WAVE_PERFECT"),
            NoticeNumber("IDS_HUD_WAVE_PERFECT_SUMMARY", rewardPercent), true);
    }
    if (HasChallenges()) {
        for (auto &notice : m_notices) { notice.releaseLevel = false; }
        QueueNotice("GLU_MOVIE_BRO_OPS_OVERLAY_INTERSITIAL", m_resources.m_movies.NamedString("IDS_HUD_CHALLENGE_UPDATE"), "", true);
    }
    std::printf("[hud-overlay] clear wave=%u perfect=%d reward=%u boss=%d\n", wave, perfect, rewardPercent, boss);
}

void CInputPad::ObserveProgress(const ZInputPadState &state) {
    if (state.deathmatch) {
        // MPMatchConsoleMessageAdd :87241 keeps five messages for 5000 ms.
        for (unsigned peer = 0; peer < 2; ++peer) {
            const char *name = "IDS_HUD_DEATH_MATCH_KILL";
            if (peer == 1) { name = "IDS_HUD_DEATH_MATCH_DEATH"; }
            for (unsigned score = m_previousMatchScore[peer] + 1; score <= state.matchScore[peer]; ++score) {
                AddMatchMessage(NoticeNumber(name, score));
            }
            m_previousMatchScore[peer] = state.matchScore[peer];
        }
    }
    // Native wave events come directly from the LEVEL consumer, not from
    // comparing two rendered snapshots. Reloading an account is no level-up.
    if (m_observedProgress && state.level > m_previousLevel) {
        QueueNotice("GLU_MOVIE_LEVEL_UP", NoticeNumber("IDS_HUD_LEVEL_REACHED", state.level));
        QueueNotice("GLU_MOVIE_LEVEL_UP", m_resources.m_movies.NamedString("IDS_HUD_HEALTH_UP"));
    }
    m_previousLevel = state.level;
    m_observedProgress = true;
}

void CInputPad::Advance(int deltaMs) {
    if (deltaMs > 0) { UpdatePowerupAnimation(static_cast<unsigned>(deltaMs)); }
    if (deltaMs > 0) { m_liveWaveRemaining -= std::min(m_liveWaveRemaining, static_cast<unsigned>(deltaMs)); }
    if (deltaMs > 0) {
        m_controlTime += deltaMs;
        if (!m_matchMessages.empty()) {
            auto &message = m_matchMessages.front();
            message.remainingMs -= std::min(message.remainingMs, static_cast<unsigned>(deltaMs));
            if (message.remainingMs == 0) { m_matchMessages.erase(m_matchMessages.begin()); }
        }
        if (HasChallenges()) {
            const auto *scroll = m_resources.m_movies.GetMovie(m_resources.m_movies.Ordinal("GLU_MOVIE_BRO_OPS_OVERLAY_SCROLL"));
            unsigned start = 0, end = 0;
            if (scroll && scroll->GetChapterRange(0, start, end)) {
                if (m_challengeHeld) { m_challengeTime = std::min(end - start, m_challengeTime + static_cast<unsigned>(deltaMs)); }
                else { m_challengeTime -= std::min(m_challengeTime, static_cast<unsigned>(deltaMs)); }
            }
        }
        for (auto &meter : m_meters) { meter.Update(deltaMs); }
    }
    if (deltaMs <= 0 || m_notices.empty()) { return; }
    Notice &notice = m_notices.front();
    notice.elapsed += static_cast<unsigned>(deltaMs);
    CMovie *movie = m_resources.m_movies.GetMovie(notice.movie);
    if (notice.movie == m_resources.m_movies.Ordinal("GLU_MOVIE_WAVE_WRAPUP") && m_liveWaveRemaining != 0) { return; }
    if (movie != nullptr && notice.elapsed >= movie->duration) {
        if (notice.releaseLevel) { m_interstitialCompleted = true; }
        if (m_matchIntro) { m_matchIntro = false; m_matchIntroCompleted = true; }
        m_notices.erase(m_notices.begin());
    }
}

void CInputPad::DrawNotice() {
    if (m_notices.empty()) { return; }
    const Notice &notice = m_notices.front();
    if (notice.movie == m_resources.m_movies.Ordinal("GLU_MOVIE_WAVE_WRAPUP")) {
        const auto *movie = m_resources.m_movies.GetMovie(notice.movie);
        if (movie == nullptr || movie->duration == 0) { return; }
        unsigned time = std::min(notice.elapsed, movie->duration - 1);
        if (m_liveWaveRemaining < 1000) { time = m_liveWaveRemaining; }
        DrawLiveWave(time);
        return;
    }
    {
        // OverlayDraw :86532 uses font 11 at its original size and centers in
        // the Movie region. Drawing as a callback preserves authored layering.
        class OverlayCallback : public ZMovieRegionCallback {
        public:
            OverlayCallback(CInputPad &owner, ZMovieRenderer &renderer, const Notice &current) : hud(owner), movies(renderer), notice(current) {}
            bool DrawMovieRegion(const ZMovieRegion &area) override {
                if (notice.movie == movies.Ordinal("GLU_MOVIE_BRO_OPS_OVERLAY_INTERSITIAL") && area.index == 1) {
                    return hud.DrawChallengeOverlay(area.x, area.y, notice.elapsed, area.alpha);
                }
                if (area.index > 1) { return true; }
                const std::string *text = &notice.title;
                if (area.index == 1) { text = &notice.footer; }
                if (text->empty()) { return true; }
                const float x = area.x + int(area.width) / 2 - int(movies.TextWidth(*text, 11)) / 2;
                const float y = area.y + int(area.height) / 2 - int(movies.TextHeight(11)) / 2;
                return movies.Text(*text, x, y, 11, 1, 0, area.alpha);
            }
            CInputPad &hud;
            ZMovieRenderer &movies;
            const Notice &notice;
        } callback(*this, m_resources.m_movies, notice);
        m_resources.m_movies.Draw(notice.movie, notice.elapsed, 512, 384, 1024, 768, 0, 1, &callback);
        return;
    }
}
