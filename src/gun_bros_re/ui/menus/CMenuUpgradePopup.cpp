/** @file CMenuUpgradePopup.cpp
 * @brief The six original states, with authored Movie times and 1x star playback.
 */
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include <algorithm>
#include <cstdint>

namespace {
// Native algorithm constants from Update :393437/:393489, not layout data.
constexpr unsigned kBackdropAlpha = 225;
constexpr unsigned kFlashDurationMs = 250;
constexpr unsigned kMaxMasteryLevel = 3; // CGun::Template::GetMasteryLevel :127925.
unsigned AdvanceTo(unsigned time, unsigned delta, unsigned end) {
    return static_cast<unsigned>(std::min<std::uint64_t>(static_cast<std::uint64_t>(time) + delta, end));
}
}

bool CMenuUpgradePopup::StarsTarget(const CMovie &stars, const CGun::Template &gun, unsigned experience, unsigned &target) {
    // SetStarsPlaybackTime :393038 uses END - START - 1, after GetChapterEndMS.
    const unsigned level = gun.GetMasteryLevel(experience);
    if (level >= kMaxMasteryLevel) { target = stars.duration; return true; }
    unsigned lower = 0;
    if (level > 0) { lower = gun.GetMasteryThreshold(level - 1); }
    const unsigned upper = gun.GetMasteryThreshold(level);
    unsigned start = 0, end = 0;
    if (upper <= lower || !stars.GetChapterRange(level, start, end) || end <= start) { return false; }
    target = start + static_cast<unsigned>(static_cast<std::uint64_t>(experience - lower) * (end - start - 1) / (upper - lower));
    return true;
}

bool CMenuUpgradePopup::Bind(const CMovie &popup, const CMovie &stars, const CGun::Template &gun, unsigned experience) {
    m_bound = false;
    for (unsigned index = 0; index < m_chapters.size(); ++index) {
        Chapter &chapter = m_chapters[index];
        if (!popup.GetChapterRange(index, chapter.start, chapter.end) || chapter.end <= chapter.start) { return false; }
    }
    if (!StarsTarget(stars, gun, experience, m_target)) { return false; }
    m_time = m_chapters[0].start;
    m_starsTime = 0;
    m_flashTime = 0;
    m_displayExperience = experience;
    m_pendingExperience = experience;
    m_pendingLevel = gun.GetMasteryLevel(experience);
    m_state = State::Opening;
    m_bound = true;
    return true;
}

void CMenuUpgradePopup::Update(unsigned deltaMs) {
    if (!m_bound || m_state == State::Closed) { return; }
    // Original updates the main movie first, then the star movie only from state 1.
    unsigned chapterIndex = 1;
    if (m_state == State::Opening) { chapterIndex = 0; }
    if (m_state == State::Closing) { chapterIndex = 2; }
    const Chapter &chapter = m_chapters[chapterIndex];
    const std::uint64_t advancedTime = static_cast<std::uint64_t>(m_time) + deltaMs;
    const bool chapterFinished = advancedTime > chapter.end;
    m_time = AdvanceTo(m_time, deltaMs, chapter.end);
    if (m_state != State::Opening) { m_starsTime = AdvanceTo(m_starsTime, deltaMs, m_target); }
    switch (m_state) {
    case State::Opening:
        if (chapterFinished) { m_time = m_chapters[1].start; m_state = State::Ready; }
        break;
    case State::Ready:
        // CMovie::Update :109213-109224 loops only past END, preserving overshoot.
        if (chapterFinished) { m_time = chapter.start + static_cast<unsigned>((advancedTime - chapter.start) % (chapter.end - chapter.start)); }
        break;
    case State::Upgrading:
        if (m_starsTime == m_target) {
            m_displayExperience = m_pendingExperience;
            m_flashTime = 0;
            m_time = m_chapters[1].start;
            m_state = State::Flash;
        }
        break;
    case State::Flash:
        m_flashTime = AdvanceTo(m_flashTime, deltaMs, kFlashDurationMs);
        if (m_flashTime == kFlashDurationMs) {
            if (m_pendingLevel >= kMaxMasteryLevel) { Hide(); }
            else { m_time = m_chapters[1].start; m_state = State::Ready; }
        }
        break;
    case State::Closing:
        if (chapterFinished) { m_state = State::Closed; }
        break;
    case State::Closed:
        break;
    }
}

bool CMenuUpgradePopup::PerformUpgrade(const CMovie &stars, const CGun::Template &gun, unsigned experience) {
    if (m_state != State::Ready || !StarsTarget(stars, gun, experience, m_target)) { return false; }
    m_pendingExperience = experience;
    m_pendingLevel = gun.GetMasteryLevel(experience);
    m_time = m_chapters[1].start;
    m_state = State::Upgrading;
    return true;
}

void CMenuUpgradePopup::Hide() {
    if (!m_bound) { return; }
    m_time = m_chapters[2].start;
    m_state = State::Closing;
}

bool CMenuUpgradePopup::SelectGun(const CMovie &stars, const CGun::Template &gun, unsigned experience) {
    // RefreshMidMenuPopup :393539 resets only the stars, not the popup opening.
    if (m_state != State::Ready || !StarsTarget(stars, gun, experience, m_target)) { return false; }
    m_starsTime = 0;
    m_displayExperience = experience;
    m_pendingExperience = experience;
    m_pendingLevel = gun.GetMasteryLevel(experience);
    return true;
}

unsigned CMenuUpgradePopup::BackdropAlpha() const {
    if (m_state == State::Opening) {
        const Chapter &chapter = m_chapters[0];
        return kBackdropAlpha * (m_time - chapter.start) / (chapter.end - chapter.start);
    }
    if (m_state == State::Closing) {
        const Chapter &chapter = m_chapters[2];
        return kBackdropAlpha - kBackdropAlpha * (m_time - chapter.start) / (chapter.end - chapter.start);
    }
    if (m_state == State::Closed) { return 0; }
    return kBackdropAlpha;
}

unsigned CMenuUpgradePopup::FlashAlpha() const {
    if (m_state != State::Flash) { return 0; }
    return 255 - 255 * m_flashTime / kFlashDurationMs;
}
