/** Original: src/gunbros/targetingController.cpp :223153.
 * Windows graphics/resource storage is adapted; original data comes from BIG.
 */
/** @file CTargetingController.cpp
 * @brief CTargetingController::Update/ClearTarget, iOS :223185/:223439.
 */
#include "gun_bros_re/gameplay/enemy/CTargetingController.h"
#include "gun_bros_re/gameplay/brother/CBrotherAI.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kAcquireDistance = 200;
constexpr float kRetainDistance = 220;
constexpr float kRadians = 3.14159265f / 180;
}

int CTargetingController::Random(int minimum, int maximum) {
    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(m_random);
}

void CTargetingController::Reset() {
    m_target = 0;
    m_findDelay = 0;
    m_fireDelay = 0;
    m_aimError = 0;
    m_desiredAimError = 0;
    m_lastFacing = 0;
    m_random.seed(0xA170);
}

void CTargetingController::ClearTarget(float facing) {
    m_target = 0;
    m_findDelay = Random(100, 200);
    m_lastFacing = facing;
}

bool CTargetingController::Update(int deltaMs, float x, float y, float &facing, ZBrotherAIWorld &world) {
    float targetX = 0, targetY = 0;
    if (m_target != 0) {
        if (!world.GetBrotherTarget(m_target, targetX, targetY)) { ClearTarget(facing); }
        else if (std::hypot(targetX - x, targetY - y) > kRetainDistance) {
            m_target = 0;
            m_findDelay = Random(300, 600);
            m_lastFacing = facing;
        }
    }
    if (m_target == 0) {
        m_findDelay = std::max(0, m_findDelay - deltaMs);
        if (m_findDelay == 0) {
            m_target = world.FindBrotherTarget(x, y, kAcquireDistance);
            m_aimError = Random(-2500, 2500) / 100.0f;
            m_desiredAimError = 0;
            if (m_target != 0) {
                world.GetBrotherTarget(m_target, targetX, targetY);
                m_fireDelay = Random(0, 500);
            }
        }
    }
    m_fireDelay = std::max(0, m_fireDelay - deltaMs);
    if (m_fireDelay > 0 || (m_target == 0 && m_findDelay == 0)) { return false; }
    const float step = deltaMs * 0.02f;
    if (m_aimError > m_desiredAimError) {
        m_aimError -= step;
        if (m_aimError < m_desiredAimError) {
            m_aimError = m_desiredAimError;
            m_desiredAimError = Random(-500, 500) / 100.0f;
        }
    } else if (m_aimError < m_desiredAimError) {
        m_aimError += step;
        if (m_aimError > m_desiredAimError) {
            m_aimError = m_desiredAimError;
            m_desiredAimError = Random(-500, 500) / 100.0f;
        }
    }
    facing = m_lastFacing + m_aimError;
    if (m_target != 0) {
        facing = std::atan2(targetY - y, targetX - x) / kRadians + 90 + m_aimError;
    }
    // The original keeps firing at its last bearing during reacquisition delay.
    return true;
}
