/** @file CBrotherAI.cpp
 * @brief Follow hysteresis and original imperfect aim/reaction intervals.
 */
#include "gun_bros/CBrotherAI.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kRadians = 3.14159265f / 180;
constexpr float kFollowStartDistance = 130;
constexpr float kFollowStopDistance = 100;
constexpr float kFollowSpeed = 100;
// Constructor :223568 stores squared 40000/48400: acquire 200, retain 220.
constexpr float kAcquireDistance = 200;
constexpr float kRetainDistance = 220;
}

void CBrotherAI::SetForce(float x, float y, int durationMs) {
    m_forceX = x;
    m_forceY = y;
    m_forceMs = durationMs;
}

int CBrotherAI::Random(int minimum, int maximum) {
    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(m_random);
}

void CBrotherAI::Reset(float startX, float startY) {
    x = startX;
    y = startY;
    previousX = x;
    previousY = y;
    facing = 0;
    vitals.Reset();
    m_target = 0;
    m_findDelay = 0;
    m_fireDelay = 0;
    m_aimError = 0;
    m_desiredAimError = 0;
    m_moving = false;
    m_targetCount = 0;
    m_forceMs = 0;
    m_weaponSwapRequested = false;
    m_random.seed(0xB6400);
}

void CBrotherAI::UpdateTarget(int deltaMs, IBrotherAIWorld &world, bool &shooting) {
    float targetX = 0;
    float targetY = 0;
    if (m_target != 0) {
        if (!world.GetBrotherTarget(m_target, targetX, targetY)) {
            m_target = 0;
            m_findDelay = Random(100, 200); // OnEnemyRemoved -> ClearTarget.
        } else if (std::hypot(targetX - x, targetY - y) > kRetainDistance) {
            m_target = 0;
            m_findDelay = Random(300, 600);
        }
    }
    if (m_target == 0) {
        m_findDelay = std::max(0, m_findDelay - deltaMs);
        if (m_findDelay == 0) {
            m_target = world.FindBrotherTarget(x, y, kAcquireDistance);
            if (m_target != 0) {
                world.GetBrotherTarget(m_target, targetX, targetY);
                m_fireDelay = Random(0, 500);
                m_aimError = Random(-2500, 2500) / 100.0f;
                m_desiredAimError = 0;
                ++m_targetCount;
            }
        }
    }
    m_fireDelay = std::max(0, m_fireDelay - deltaMs);
    if (m_target == 0 || m_fireDelay > 0) { return; }
    // Original aiming error converges at 20 degrees/second, then wanders +/-5.
    const float step = deltaMs * 0.02f;
    if (std::abs(m_desiredAimError - m_aimError) <= step) {
        m_aimError = m_desiredAimError;
        m_desiredAimError = Random(-500, 500) / 100.0f;
    } else if (m_aimError < m_desiredAimError) { m_aimError += step; }
    else { m_aimError -= step; }
    facing = std::atan2(targetY - y, targetX - x) / kRadians + 90 + m_aimError;
    shooting = true;
}

void CBrotherAI::Update(int deltaMs, CBrother &brother, IBrotherAIWorld &world,
    float playerX, float playerY, float speedMultiplier) {
    previousX = x;
    previousY = y;
    if (!vitals.dead && m_forceMs > 0) {
        const float seconds = std::min(deltaMs, m_forceMs) * 0.001f;
        x += m_forceX * seconds;
        y += m_forceY * seconds;
        m_forceMs = std::max(0, m_forceMs - deltaMs);
        world.ResolveBrotherForce(previousX, previousY, x, y);
    }
    if (vitals.dead || vitals.stunMs > 0) {
        m_moving = false;
        brother.SetInput(false, false);
        return;
    }
    // CBrotherAI::Update :139423 chooses an occasional weapon swap with the
    // original inclusive 0..10000 <= 3 roll. The host applies it after Update
    // returns, so replacing the gun cannot invalidate this CBrother reference.
    if (Random(0, 10000) <= 3) { m_weaponSwapRequested = true; }
    const float distance = std::hypot(playerX - x, playerY - y);
    if (distance < kFollowStopDistance || !brother.CanMove()) { m_moving = false; }
    else if (distance > kFollowStartDistance) { m_moving = true; }
    float moveX = 0;
    float moveY = 0;
    if (m_moving) {
        float waypointX = playerX;
        float waypointY = playerY;
        if (world.GetBrotherWaypoint(x, y, playerX, playerY, waypointX, waypointY)) {
            const float length = std::hypot(waypointX - x, waypointY - y);
            if (length > 0) {
                const float travel = std::min(length, kFollowSpeed * speedMultiplier * deltaMs * 0.001f);
                moveX = (waypointX - x) / length * travel;
                moveY = (waypointY - y) / length * travel;
                x += moveX;
                y += moveY;
            }
        }
    }
    bool shooting = false;
    if (brother.CanShoot()) { UpdateTarget(deltaMs, world, shooting); }
    if (!shooting && (moveX != 0 || moveY != 0)) { facing = std::atan2(moveY, moveX) / kRadians + 90; }
    brother.SetInput(moveX != 0 || moveY != 0, shooting);
}
