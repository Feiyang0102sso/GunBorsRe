/** Original: src/gunbros/stunController.cpp :394408.
 * Windows graphics/resource storage is adapted; original data comes from BIG.
 */
/** @file CStunController.cpp
 * @brief Keep original phase calculation: it uses remaining time / interval.
 */
#include "gun_bros_re/gameplay/enemy/CStunController.h"

void CStunController::SetStunned(int durationMs, int intervalMs, int amplitude) {
    m_remainingMs = durationMs;
    m_intervalMs = intervalMs;
    m_amplitude = amplitude;
    m_offset = 0;
}

void CStunController::ClearStunned() {
    m_remainingMs = 0;
    m_intervalMs = 0;
    m_amplitude = 0;
    m_offset = 0;
}

bool CStunController::Update(int deltaMs) {
    if (m_remainingMs <= 0) { return false; }
    const int previous = m_remainingMs;
    m_remainingMs -= deltaMs;
    if (m_remainingMs <= 0) { m_offset = 0; return true; }
    if (m_amplitude != 0 && m_intervalMs > 0 && previous / m_intervalMs != m_remainingMs / m_intervalMs) {
        if (m_offset < 0) { m_offset = m_amplitude; }
        else { m_offset = -m_amplitude; }
    }
    return false;
}
