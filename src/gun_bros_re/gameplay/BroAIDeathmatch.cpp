/** Test peer input policy. Shared CBrother/CGun/Flow still execute all actions.
 * This policy is a Windows replacement for player input, not original game AI.
 */
#include "gun_bros_re/gameplay/BroAIDeathmatch.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kRadians = 3.14159265f / 180;
constexpr float kSpeed = 220; // CPlayer movement speed, same as the local input host.
constexpr float kSearchRadius = 700;
constexpr float kPreferredRange = 230;
constexpr float kLookAhead = 100;
constexpr int kReactionMs = 180;
constexpr int kSwapMinimumMs = 3000, kSwapMaximumMs = 6000;
constexpr int kRoamMs = 2500;
constexpr int kSteeringMs = 180; // Host policy: retain an input briefly instead of frame-wise zigzagging.
constexpr float kRangeTolerance = 40;
}

void BroAIDeathmatch::Reset(float startX, float startY, float startFacing) {
    CBrotherAI::Reset(startX, startY, startFacing);
    m_target = 0;
    m_reactionMs = 0;
    m_moving = false;
    m_targetCount = 0;
    m_swapMs = kSwapMinimumMs;
    m_roamMs = 0;
    m_roamX = x;
    m_roamY = y;
    m_powerupMs = 0;
    m_shopMs = 0;
    m_lastShopAttempt = UINT32_MAX;
    m_steeringMs = 0;
    m_moveX = m_moveY = 0;
    m_rescuing = false;
    m_random.seed(0xB07);
}

void BroAIDeathmatch::AdvanceActions(unsigned deltaMs) {
    m_powerupMs += deltaMs;
    m_shopMs += deltaMs;
}
bool BroAIDeathmatch::TakePowerupRequest() {
    if (m_powerupMs < 9000) { return false; }
    m_powerupMs = 0;
    return true;
}
bool BroAIDeathmatch::TakeShopRequest() {
    if (m_shopMs < 45000) { return false; }
    m_shopMs = 0;
    return true;
}

unsigned BroAIDeathmatch::ShopSelection(unsigned elapsedMs) const { return elapsedMs / 850; }

bool BroAIDeathmatch::ShouldBuyShopItem(unsigned elapsedMs, unsigned ownedCount) {
    if (elapsedMs < 400) { m_lastShopAttempt = UINT32_MAX; return false; }
    const unsigned selection = ShopSelection(elapsedMs);
    if (elapsedMs % 850 < 400 || selection == m_lastShopAttempt) { return false; }
    m_lastShopAttempt = selection;
    return ownedCount == 0;
}

void BroAIDeathmatch::Update(int deltaMs, CBrother &brother, IBrotherAIWorld &world,
    float playerX, float playerY, float speedMultiplier) {
    if (deltaMs <= 0) { return; }
    UpdateForce(deltaMs, brother, world);
    m_moving = false;
    if (vitals.dead || vitals.stunMs > 0) { brother.SetInput(false, false); return; }
    m_swapMs -= deltaMs;
    if (m_swapMs <= 0 && m_shootingAllowed) {
        m_weaponSwapRequested = true;
        m_swapMs = std::uniform_int_distribution<int>(kSwapMinimumMs, kSwapMaximumMs)(m_random);
    }
    float targetX = 0, targetY = 0;
    if (m_target != 0 && (!world.GetBrotherTarget(m_target, targetX, targetY) ||
        std::hypot(targetX - x, targetY - y) > kSearchRadius)) { m_target = 0; }
    if (m_target == 0 && m_shootingAllowed) {
        m_target = world.FindBrotherTarget(x, y, kSearchRadius);
        if (m_target != 0) {
            world.GetBrotherTarget(m_target, targetX, targetY);
            m_reactionMs = kReactionMs;
            ++m_targetCount;
        }
    }
    m_reactionMs = std::max(0, m_reactionMs - deltaMs);
    m_roamMs -= deltaMs;
    if (m_roamMs <= 0) {
        const float angle = std::uniform_real_distribution<float>(0, 360)(m_random) * kRadians;
        m_roamX = x + std::cos(angle) * kPreferredRange;
        m_roamY = y + std::sin(angle) * kPreferredRange;
        m_roamMs = kRoamMs;
    }
    // A living player is never a movement destination. Approach only to revive.
    const bool rescue = world.IsPlayerDown();
    m_steeringMs -= deltaMs;
    if (rescue != m_rescuing) { m_steeringMs = 0; }
    m_rescuing = rescue;
    const float movementStep = kSpeed * speedMultiplier * deltaMs * 0.001f;
    if (!world.CanBrotherWalk(x, y, x + m_moveX * movementStep, y + m_moveY * movementStep)) { m_steeringMs = 0; }
    float goalX = m_roamX, goalY = m_roamY;
    if (m_target != 0) { goalX = targetX; goalY = targetY; }
    if (rescue) { goalX = playerX; goalY = playerY; }
    float waypointX = goalX, waypointY = goalY;
    if (rescue && world.GetBrotherWaypoint(x, y, goalX, goalY, waypointX, waypointY)) {
        goalX = waypointX; goalY = waypointY;
    }
    float moveX = m_moveX, moveY = m_moveY;
    if (m_steeringMs <= 0 || rescue) {
        const auto threats = world.GetBrotherThreats();
        float bestScore = std::numeric_limits<float>::max();
        moveX = 0; moveY = 0;
        // Score stationary plus sixteen walkable directions against every nearby
        // enemy. A close crowd outweighs the firing-range or rescue preference.
        for (unsigned candidate = 0; candidate <= 16; ++candidate) {
            float dx = 0, dy = 0;
            if (candidate != 0) {
                const float angle = (candidate - 1) * 22.5f * kRadians;
                dx = std::cos(angle); dy = std::sin(angle);
            }
            const float nextX = x + dx * kLookAhead, nextY = y + dy * kLookAhead;
            // Narrow passages can reject every 100-unit probe despite having a
            // valid short step. Require the actual step, then penalize blocked lookahead.
            const float step = kSpeed * speedMultiplier * deltaMs * 0.001f;
            if (!world.CanBrotherWalk(x, y, x + dx * step, y + dy * step)) { continue; }
            float preferred = 0;
            if (m_target != 0 && !rescue) { preferred = kPreferredRange; }
            if (rescue && goalX == playerX && goalY == playerY) { preferred = 70; }
            float score = std::abs(std::hypot(goalX - nextX, goalY - nextY) - preferred);
            // A safe firing band permits standing still. Exact-distance scoring
            // alternated left/right probes while pursuing an already stationary target.
            if (!rescue) { score = std::max(0.0f, score - kRangeTolerance); }
            if (rescue) {
                // A navigation portal must be crossed. Scoring a 100-unit probe
                // with a 70-unit stand-off made the peer stop before that portal.
                score = std::abs(std::hypot(goalX - x - dx * step, goalY - y - dy * step) - preferred);
            } else if (!world.CanBrotherWalk(x, y, nextX, nextY)) { score += 250; }
            for (const auto &threat : threats) {
                const float clearance = std::max(1.0f, std::hypot(threat.x - nextX, threat.y - nextY) - threat.radius);
                if (clearance < 180) { score += 120000 / (clearance * clearance); }
            }
            if (score < bestScore) { bestScore = score; moveX = dx; moveY = dy; }
        }
        m_moveX = moveX; m_moveY = moveY;
        m_steeringMs = kSteeringMs;
    }
    if (brother.CanMove() && (moveX != 0 || moveY != 0)) {
        const float step = kSpeed * speedMultiplier * deltaMs * 0.001f;
        const float oldX = x, oldY = y;
        x += moveX * step; y += moveY * step;
        world.ResolveBrotherForce(oldX, oldY, x, y);
        m_moving = x != oldX || y != oldY;
    }
    const bool shooting = m_shootingAllowed && brother.CanShoot() && m_target != 0 && m_reactionMs == 0;
    if (shooting) { facing = std::atan2(targetY - y, targetX - x) / kRadians + 90; }
    else if (m_moving) { facing = std::atan2(y - previousY, x - previousX) / kRadians + 90; }
    brother.SetInput(m_moving, shooting);
}
