/** Original CGun::Fire :128152 and CBullet::UpdateSeeking :61446.
 * Stable IDs replace original target pointers; target policy remains in CLevel.
 */
#include "gun_bros_re/gameplay/weapon/CBullet.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kRadians = 3.14159265f / 180.0f;
float NormalizeAngle(float angle) {
    angle = std::fmod(angle, 360.0f);
    if (angle < 0) { angle += 360.0f; }
    return angle;
}
Collision::Hit SeekingQuery(const CBullet &bullet) {
    Collision::Hit hit;
    hit.owner = bullet.owner;
    hit.ownerType = bullet.ownerType;
    hit.x = bullet.x;
    hit.y = bullet.y;
    hit.direction = bullet.direction;
    return hit;
}
}

void CBullet::InitializeSeeking(World *world) {
    if (world == nullptr || seekingTurnRate <= 0 || m_sourceGun == nullptr) { return; }
    const float angle = m_sourceGun->GetSeekAngle();
    if (angle > 0) { seekingTarget = world->FindSeekTarget(SeekingQuery(*this), angle); }
}

void CBullet::UpdateSeeking(World *world, int deltaMs) {
    if (world == nullptr || seekingTurnRate <= 0 || deltaMs <= 0 || speed <= 0) { return; }
    float targetX = 0, targetY = 0, targetZ = 0, targetAngle = 0;
    if (seekingTarget != Collision::NoObject &&
        !world->Anchor(seekingTarget, -1, -1, targetX, targetY, targetZ, targetAngle)) {
        seekingTarget = Collision::NoObject;
    }
    const bool reacquire = seekingTarget == Collision::NoObject && (flags & 0x2000) != 0;
    if (ownerType == 1 && !reacquire) {
        seekingTarget = world->FindSeekTarget(SeekingQuery(*this), 0);
    } else if (reacquire && ownerType != 1 && m_sourceGun != nullptr) {
        seekingTarget = world->FindSeekTarget(SeekingQuery(*this), m_sourceGun->GetSeekAngle());
    }
    if (seekingTarget == Collision::NoObject ||
        !world->Anchor(seekingTarget, -1, -1, targetX, targetY, targetZ, targetAngle)) { return; }
    const float dx = targetX - x;
    const float dy = targetY - y;
    if (dx == 0 && dy == 0) { return; }
    const float current = NormalizeAngle(direction + 90);
    const float desired = NormalizeAngle(std::atan2(dy, dx) / kRadians + 90);
    float difference = desired - current;
    if (difference > 180) { difference -= 360; }
    if (difference < -180) { difference += 360; }
    const float maximumTurn = seekingTurnRate * deltaMs * 0.001f;
    float next = desired;
    if (std::abs(difference) > maximumTurn) {
        next = current + std::clamp(difference, -maximumTurn, maximumTurn);
    }
    // :61780 refreshes the velocity/ray only for a change larger than one degree.
    if (std::abs(next - current) > 1) { direction = next - 90; }
}
