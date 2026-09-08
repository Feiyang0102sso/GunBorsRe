/** @file CCamera.cpp
 * @brief CCamera::GetScale/SetScale and the scale part of UpdatePlayerCamera.
 */
#include "gun_bros/CCamera.h"
#include <cmath>

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kScaleProgressPerMillisecond = 0.001f;
}

void CCamera::SnapScale(float scale) {
    m_previousScale = scale;
    m_targetScale = scale;
    m_scaleProgress = 1;
}

void CCamera::SetScale(float scale) {
    // Original :64923 starts a retarget from the current interpolated scale.
    m_previousScale = GetScale();
    m_targetScale = scale;
    m_scaleProgress = 0;
}

float CCamera::GetScale() const {
    if (m_scaleProgress == 1) { return m_targetScale; }
    const float fraction = (1 - std::cos(kPi * m_scaleProgress)) * 0.5f;
    return m_previousScale * (1 - fraction) + m_targetScale * fraction;
}

void CCamera::Update(int deltaMs) {
    if (deltaMs <= 0) { return; }
    // Original :65305 uses 0.001 per millisecond and clamps at one.
    m_scaleProgress += deltaMs * kScaleProgressPerMillisecond;
    if (m_scaleProgress > 1) { m_scaleProgress = 1; }
}
