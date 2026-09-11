/** @file CPlayerProgress.cpp
 * @brief CPlayerProgress::Template::Init (:193209) and level-table indexing.
 */
#include "gun_bros_re/data/CPlayerProgress.h"
#include <algorithm>
#include <cmath>

bool CPlayerProgress::Template::Init(CArrayInputStream &stream) {
    experience.resize(stream.ReadUInt16());
    for (std::uint32_t &value : experience) { value = stream.ReadUInt32(); }
    health.resize(stream.ReadUInt16());
    // Original storage truncates the 32-bit wire value into a signed short.
    for (std::int16_t &value : health) { value = static_cast<std::int16_t>(stream.ReadUInt32()); }
    value20Raw = stream.ReadInt32();
    value20 = std::ceil(value20Raw * (100.0f / 65536.0f)) / 100.0f;
    value24 = stream.ReadInt32();
    return !stream.Overran() && experience.size() >= health.size() && health.size() >= 2;
}

unsigned CPlayerProgress::Template::GetMaximumLevel() const {
    return static_cast<unsigned>(health.size() - 1);
}

std::uint64_t CPlayerProgress::Template::GetExperienceForLevel(unsigned level) const {
    std::uint64_t total = 0;
    for (unsigned index = 0; index < level && index < experience.size(); ++index) {
        total += experience[index];
    }
    return total;
}

void CPlayerProgress::Bind(const Template &data) {
    m_template = &data;
    SetExperience(0);
}

void CPlayerProgress::SetExperience(std::uint64_t experience) {
    m_experience = experience;
    m_level = 1;
    std::uint64_t threshold = m_template->GetExperienceForLevel(1);
    while (m_level < m_template->GetMaximumLevel()) {
        threshold += m_template->experience[m_level];
        if (m_experience < threshold) { break; }
        ++m_level;
    }
}

bool CPlayerProgress::AddExperience(std::uint32_t amount) {
    if (IsMaximumLevel()) { return false; }
    const unsigned previous = m_level;
    SetExperience(m_experience + amount);
    return m_level != previous;
}

std::uint64_t CPlayerProgress::GetExperienceInLevel() const {
    const std::uint64_t base = m_template->GetExperienceForLevel(m_level);
    if (m_experience < base) { return 0; }
    return m_experience - base;
}

std::uint32_t CPlayerProgress::GetExperienceDelta() const {
    return m_template->experience[m_level];
}

float CPlayerProgress::GetHealth() const { return static_cast<float>(m_template->health[m_level]); }
bool CPlayerProgress::IsMaximumLevel() const { return m_level == m_template->GetMaximumLevel(); }
