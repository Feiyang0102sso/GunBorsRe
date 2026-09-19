/** @file CEnemyCombat.cpp
 * Original: src/gunbros/enemy.cpp SetBehaviour :69707, Update :67732,
 * Damage :71552, ResolveFunctionLocally :71692. One CEnemy, split implementation.
 */
/** @file CEnemyCombat.cpp
 * @brief Enemy script behaviour, timers and damage in world coordinates.
 * Reference: CEnemy::Spawn :73239, Update :67732, resolver :71692.
 */
#include "gun_bros_re/gameplay/enemy/CEnemy.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kRadians = 3.14159265f / 180.0f;
}

void CEnemy::ConfigureTemplate(float radius, bool targetable,
    const GameObjectRef &bullet, const CCollisionData &collision) {
    m_parts[0].radius = radius;
    combat.targetable = targetable;
    combat.bullet = bullet;
    combat.collision = collision;
}

bool CEnemy::CanReceiveProjectile(int ownerType, Collision::ObjectId owner) const {
    if (combat.dead || combat.removed || combat.health <= 0 || owner == combat.id) {
        return false;
    }
    // Both circle and complex collision use these exact comparisons (:72862).
    const int allegiance = combat.variables[16];
    if (ownerType == 1 && allegiance == 0) {
        return false;
    }
    if ((ownerType == 0 || ownerType == 2) && allegiance == 1) {
        return false;
    }
    return true;
}

Collision::HitResult CEnemy::ReceiveHit(const Collision::Hit &hit) {
    if (!CanReceiveProjectile(hit.ownerType, hit.owner)) {
        return Collision::HitResult::Ignored;
    }
    combat.pendingHit = hit;
    combat.collisionPending = true;
    combat.collisionResult = Collision::HitResult::Pending;
    combat.variables[2] = static_cast<std::int16_t>(hit.part);
    combat.variables[3] = 256;
    combat.variables[4] = 0;
    if (hit.splash) { combat.variables[4] = 1; }
    combat.variables[5] = static_cast<std::int16_t>(NormalizeAngle(
        std::atan2(hit.x - combat.x, combat.y - hit.y) / kRadians));
    combat.variables[7] = static_cast<std::int16_t>(hit.edge);
    combat.variables[9] = static_cast<std::int16_t>(NormalizeAngle(
        combat.variables[5] - combat.facing));
    combat.variables[19] = 0;
    // Scripts can reject frontal hits or modify the damage multiplier before
    // native 13 commits the collision. A direct health subtraction skips this.
    // HandleCollision :71526-71534 returns before the hit event for a grenade
    // (IsGrenade :60370, collision flag bit 4). Its later OnSplashDamage :67945
    // still triggers the event. Otherwise each fuse contact can strip armor.
    if (!hit.splash && (hit.flags & (1u << 4)) != 0) {
        return combat.collisionResult;
    }
    TriggerEvent(2);
    return combat.collisionResult;
}

void CEnemy::ResolvePendingHit(bool apply) {
    if (!combat.collisionPending) {
        return;
    }
    combat.collisionPending = false;
    if (apply) {
        const int part = combat.variables[2];
        if (part >= 0 && static_cast<std::uint32_t>(part) < m_partCount) {
            m_parts[part].hitFlash = 1;
        }
        Damage(combat.pendingHit.damage * static_cast<float>(combat.variables[3]) / 256.0f);
        combat.collisionResult = Collision::HitResult::Hit;
        if (combat.dead) {
            combat.collisionResult = Collision::HitResult::Killed;
        }
    } else {
        combat.collisionResult = Collision::HitResult::Ignored;
    }
    CEnemy::Action action;
    action.kind = CEnemy::Action::Kind::CollisionResolved;
    action.projectile = combat.pendingHit.projectile;
    action.owner = combat.pendingHit.owner;
    action.resource = combat.pendingHit.weapon;
    action.slot = static_cast<int>(combat.pendingHit.weaponSlot);
    action.result = combat.collisionResult;
    combat.actions.push_back(action);
}

void CEnemy::Damage(float amount) {
    if (combat.dead || combat.removed || combat.health <= 0 || amount <= 0) {
        return;
    }
    combat.lastDamage = std::min(combat.health, amount);
    combat.totalDamage += combat.lastDamage;
    combat.health = std::max(0.0f, combat.health - amount);
    combat.hitFlash = 1;
    combat.healthBarFlashMs = 1000; // CEnemy::Damage :71563.
    ++combat.hitCount;
    if (combat.health == 0) {
        combat.dead = true;
        ++combat.deathCount;
        combat.autoFireInterval = 0;
        // Export 1 selects the death sequence and its eventual removal call.
        m_interpreter.CallExportFunction(1);
    }
}

bool CEnemy::CanCollideWithPlayer() const {
    // Enabled/removed represent membership in the original level's enemy list.
    // Targeting type, visible parts and complex bullet edges do not filter bodies.
    return combat.enabled && !combat.removed && combat.health != 0 && combat.variables[16] != 1;
}
