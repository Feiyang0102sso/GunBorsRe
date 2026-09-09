/** @file CInputPadMeter.h
 * @brief Original meter value interpolation; drawing is a host adapter.
 * CInputPadMeter :130721..130857, native Health/ExperienceMeterConfiguration
 * at ARMv7 0x3ff150/0x3ff170. These are native colors, not a resource table.
 */
#ifndef GUN_BROS_RE_CINPUTPADMETER_H
#define GUN_BROS_RE_CINPUTPADMETER_H
#include <algorithm>
#include <cmath>
class CInputPadMeter {
public:
    void SnapValue(float value) { m_start = m_target = std::clamp(value, 0.0f, 1.0f); m_progress = 1; m_speed = 0; }
    void SetValue(float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        if (value == m_target) { return; }
        // SetValue uses linear interpolation for retargeting; Draw uses cosine.
        m_start = m_start * (1 - m_progress) + m_target * m_progress;
        m_speed = std::abs(value - m_start) * 50;
        m_target = value;
        m_progress = 0;
    }
    void ReFill() { m_start = 0; m_speed = m_target / 50; m_progress = 0; }
    void Update(unsigned deltaMs) {
        m_progress += m_speed * deltaMs * 0.001f;
        if (m_progress > 1) { m_progress = 1; }
    }
    float GetDrawValue() const {
        const float fraction = (1 - std::cos(m_progress * 3.14159265358979323846f)) * 0.5f;
        return m_start * (1 - fraction) + m_target * fraction;
    }
    unsigned GetHighlight() const { return static_cast<unsigned>((1 - m_progress) * 200); }
private:
    float m_start = 0, m_target = 0, m_speed = 0, m_progress = 1;
};
#endif
