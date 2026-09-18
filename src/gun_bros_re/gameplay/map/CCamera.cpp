/** @file CCamera.cpp
 * @brief CCamera::GetScale/SetScale and the scale part of UpdatePlayerCamera.
 */
#include "gun_bros_re/gameplay/map/CCamera.h"
#include <cmath>
#include <algorithm>

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
    m_startX = m_x;
    m_startY = m_y;
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
    if (m_shakeMs > 0) {
        const int previousBucket = m_shakeMs / 40;
        m_shakeMs = std::max(0, m_shakeMs - deltaMs);
        if (m_shakeMs == 0) { m_shakeX = 0; m_shakeY = 0; }
        else if (previousBucket != m_shakeMs / 40) {
            m_shakeX = RandomShake(m_shakeX);
            m_shakeY = RandomShake(m_shakeY);
        }
    }
}

void CCamera::Reset(float scale) {
    *this = CCamera();
    SnapScale(scale);
}

void CCamera::SetCameraMode(int mode) {
    // Original modes: 0 follows the player, 1 holds, 2 follows a fixed target.
    m_mode = mode;
    if (mode == 0 || mode == 2) { SetScale(m_targetScale); }
}

void CCamera::SetTarget(float x, float y) {
    m_targetX = x;
    m_targetY = y;
    m_targetPlayer = false;
}

void CCamera::SetTargetToPlayer() {
    SetCameraMode(2);
    m_targetPlayer = true;
}

float CCamera::RandomShake(float previous) {
    m_random = m_random * 1664525u + 1013904223u;
    float value = static_cast<float>(1 + (m_random >> 16) % 3);
    if (previous >= 0) { value = -value; }
    return value;
}

void CCamera::Shake(int durationMs) {
    // CCamera::Shake :64584: extend an active shake; otherwise begin with a
    // negative 1..3 offset, then alternate its sign every 40 ms.
    if (durationMs <= 0) { return; }
    if (m_shakeMs == 0) { m_shakeX = RandomShake(0); m_shakeY = RandomShake(0); }
    m_shakeMs = std::max(m_shakeMs, durationMs);
}

void CCamera::UpdatePosition(float playerX, float playerY, float left, float top, float width, float height,
    float viewWidth, float viewHeight) {
    if (m_targetPlayer) { SetTarget(playerX, playerY); }
    if (m_mode != 0 && m_mode != 2 && m_hasPosition) { return; }
    float x = playerX, y = playerY;
    if (m_mode == 2) { x = m_targetX; y = m_targetY; }
    auto clampCenter = [&](float &cx, float &cy) {
        if (width > 0) {
            if (width <= viewWidth) { cx = left + width * 0.5f; }
            else { cx = std::clamp(cx, left + viewWidth * 0.5f, left + width - viewWidth * 0.5f); }
        }
        if (height > 0) {
            if (height <= viewHeight) { cy = top + height * 0.5f; }
            else { cy = std::clamp(cy, top + viewHeight * 0.5f, top + height - viewHeight * 0.5f); }
        }
    };
    clampCenter(x, y);
    x += m_shakeX;
    y += m_shakeY;
    clampCenter(x, y);
    if (!m_hasPosition) { m_startX = x; m_startY = y; m_hasPosition = true; }
    const float fraction = (1 - std::cos(kPi * m_scaleProgress)) * 0.5f;
    m_x = m_startX * (1 - fraction) + x * fraction;
    m_y = m_startY * (1 - fraction) + y * fraction;
}
