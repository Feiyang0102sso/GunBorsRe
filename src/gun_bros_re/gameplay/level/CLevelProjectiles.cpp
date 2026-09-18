#include "gun_bros_re/gameplay/map/CLevelProps.h"
/** @file CLevelProjectiles.cpp
 * @brief CLevel projectile tracing, damage and splash dispatch.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/ZCombatGeometry.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kRadians = 3.14159265f / 180;

bool Skipped(ZCombatId id, const std::vector<ZCombatId> &ids) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

using CombatGeometry::CircleFraction;
using CombatGeometry::EdgeFraction;
}

ZCombatTrace CLevel::Trace(const ZCombatHit &hit, float x, float y, float dx, float dy,
    float radius, const std::vector<ZCombatId> &skipTargets) {
    ZCombatTrace result;
    float nearest = 2;
    if (m_props != nullptr) {
        result = m_props->Trace(hit, x, y, dx, dy, radius, skipTargets);
        if (result.target != 0) { nearest = result.fraction; }
    }
    if (CanHitBrother(hit, kPlayerCombatId) && !m_vitals->dead && !Skipped(kPlayerCombatId, skipTargets)) {
        float moveX = m_actor.x - m_actor.previousX, moveY = m_actor.y - m_actor.previousY;
        if ((hit.flags & 0x100) != 0) { moveX = 0; moveY = 0; }
        const float fraction = CircleFraction(x, y, dx - moveX, dy - moveY,
            m_actor.x - moveX, m_actor.y - moveY, m_playerRadius + radius);
        if (fraction <= 1 && fraction < nearest) {
            nearest = fraction;
            result.target = kPlayerCombatId; result.fraction = nearest;
            result.normalX = x + dx * nearest - m_actor.x;
            result.normalY = y + dy * nearest - m_actor.y;
        }
    }
    if (CanHitBrother(hit, kBrotherCombatId) && m_brother != nullptr && !m_brother->vitals.dead && !Skipped(kBrotherCombatId, skipTargets)) {
        float moveX = m_brother->x - m_brother->previousX;
        float moveY = m_brother->y - m_brother->previousY;
        if ((hit.flags & 0x100) != 0) { moveX = 0; moveY = 0; }
        const float fraction = CircleFraction(x, y, dx - moveX, dy - moveY,
            m_brother->x - moveX, m_brother->y - moveY, m_playerRadius + radius);
        if (fraction <= 1 && fraction < nearest) {
            nearest = fraction;
            result = {kBrotherCombatId, fraction, -1, -1,
                x + dx * fraction - m_brother->x, y + dy * fraction - m_brother->y};
        }
    }
    for (auto &actor : m_objects.GetEnemies()) {
        CEnemy &enemy = *actor;
        CEnemy::CombatState &state = enemy.combat;
        if (!state.enabled || !enemy.CanReceiveProjectile(hit.ownerType, hit.owner) ||
            Skipped(state.id, skipTargets)) { continue; }
        const auto &edges = state.collision.GetEdges();
        const auto &vertices = state.collision.GetVertices();
        if (!edges.empty()) {
            const float cosine = std::cos(state.facing * kRadians), sine = std::sin(state.facing * kRadians);
            // Authored collision vertices already use world units; only runtime
            // scaling and actor rotation apply, not mesh normalisation or tilt.
            for (std::size_t e = 0; e < edges.size(); ++e) {
                const ZCollisionEdge &edge = edges[e];
                if (!edge.enabled || edge.firstVertex >= vertices.size() || edge.secondVertex >= vertices.size()) { continue; }
                const ZCollisionPoint &a = vertices[edge.firstVertex], &b = vertices[edge.secondVertex];
                ZCollisionPoint first(state.x + (a.x * cosine - a.y * sine) * state.scaleFactor,
                    state.y + (a.x * sine + a.y * cosine) * state.scaleFactor);
                ZCollisionPoint second(state.x + (b.x * cosine - b.y * sine) * state.scaleFactor,
                    state.y + (b.x * sine + b.y * cosine) * state.scaleFactor);
                const float fraction = EdgeFraction(x, y, dx, dy, first, second, radius);
                if (fraction < nearest) {
                    nearest = fraction;
                    // The script sees the authored edge group, not its array index.
                    result = {state.id, fraction, 0, edge.group, first.y - second.y, second.x - first.x};
                }
            }
            continue;
        }
        for (std::uint32_t p = 0; p < enemy.GetPartCount(); ++p) {
            const CEnemy::Part &part = enemy.GetPart(p);
            if (part.radius <= 0 || !part.visible) { continue; }
            float cx = 0, cy = 0, partRadius = 0;
            EnemyCircle(*actor, p, cx, cy, partRadius);
            float moveX = state.x - state.previousX, moveY = state.y - state.previousY;
            if ((hit.flags & 0x100) != 0) { moveX = 0; moveY = 0; }
            const float fraction = CircleFraction(x, y, dx - moveX, dy - moveY,
                cx - moveX, cy - moveY, radius + partRadius);
            if (fraction < nearest) {
                nearest = fraction;
                result = {state.id, fraction, static_cast<int>(p), -1,
                    x + dx * fraction - cx + moveX * (1 - fraction),
                    y + dy * fraction - cy + moveY * (1 - fraction)};
            }
        }
    }
    return result;
}


bool CLevel::Suicide() {
    m_playerModel->SetLevelContext(GetScriptLevel());
    return m_playerModel->StartDeath();
}

ZHitResult CLevel::ApplyHit(ZCombatId target, const ZCombatHit &hit) {
    if (target == kBrotherCombatId && m_brotherModel != nullptr) {
        if (!CanHitBrother(hit, target)) { return ZHitResult::Ignored; }
        m_brotherModel->SetLevelContext(GetScriptLevel());
        const float reduction = m_brotherModel->GetArmorMultiplier(0) - 1;
        float damage = hit.damage;
        if (IsDeathmatch() && hit.applyArmorAttack && hit.owner == kPlayerCombatId) { damage *= m_playerModel->GetArmorMultiplier(1); }
        if (hit.splash && hit.percentDamage) { damage *= m_brother->vitals.maximum * 0.01f; }
        const ZHitResult result = m_brotherModel->ReceiveDamage(std::max(0.0f, damage * (1 - reduction)) /
            CFriendPowerManager::Multiplier(m_brotherModel->friendCount, 1));
        if (IsDeathmatch() && result != ZHitResult::Ignored) { m_matchStreaks[1] = 0; }
        if (result == ZHitResult::Killed) { RecordMatchDeath(1, hit.owner == kPlayerCombatId ? 0 : -1); }
        return result;
    }
    if (target == kPlayerCombatId) {
        if (!CanHitBrother(hit, target) || m_playerModel->weapon == nullptr) { return ZHitResult::Ignored; }
        m_playerModel->SetLevelContext(GetScriptLevel());
        // CBrother::Damage (:136667): add slot percentages, then reduce the
        // incoming amount. Defence does not increase the player's max health.
        const float reduction = m_playerModel->GetArmorMultiplier(0) - 1.0f;
        // CBrother::OnSplashDamage :135359 interprets native 23 as a percent
        // of maximum health before the ordinary armor / frenzy reductions.
        float damage = hit.damage;
        if (IsDeathmatch() && hit.applyArmorAttack && hit.owner == kBrotherCombatId) { damage *= m_brotherModel->GetArmorMultiplier(1); }
        if (hit.splash && hit.percentDamage) { damage *= m_vitals->maximum * 0.01f; }
        damage = std::max(0.0f, damage * (1.0f - reduction)) / CFriendPowerManager::Multiplier(m_playerModel->friendCount, 1);
        const unsigned hitsBefore = m_vitals->hits;
        const ZHitResult result = m_playerModel->ReceiveDamage(damage);
        if (IsDeathmatch() && result != ZHitResult::Ignored) { m_matchStreaks[0] = 0; }
        if (result == ZHitResult::Killed) { RecordMatchDeath(0, hit.owner == kBrotherCombatId ? 1 : -1); }
        // OnPlayerDamaged :115914 resets the streak on accepted damage only.
        if (m_vitals->hits != hitsBefore) { ResetKillStreak(); }
        return result;
    }
    ZCombatHit adjusted = hit;
    // CPlayer::GetDamage includes the active roster's native BRO BUFF.
    if (hit.owner == kPlayerCombatId) { adjusted.damage *= CFriendPowerManager::Multiplier(m_playerModel->friendCount, 0); }
    if (hit.owner == kBrotherCombatId && m_brotherModel != nullptr) { adjusted.damage *= CFriendPowerManager::Multiplier(m_brotherModel->friendCount, 0); }
    if (hit.applyArmorAttack && hit.owner == kPlayerCombatId) {
        adjusted.damage *= m_playerModel->GetArmorMultiplier(1);
    }
    if (hit.applyArmorAttack && hit.owner == kBrotherCombatId && m_brotherModel != nullptr) {
        adjusted.damage *= m_brotherModel->GetArmorMultiplier(1);
    }
    CEnemy *actor = Find(target);
    if (actor == nullptr) {
        if (m_props != nullptr) { return m_props->ApplyHit(target, adjusted); }
        return ZHitResult::Ignored;
    }
    const ZHitResult result = actor->ReceiveHit(adjusted);
    return result;
}

float CLevel::GetDamageMultiplier(ZCombatId owner, float fallback) const {
    for (const auto &actor : m_objects.GetEnemies()) {
        if (actor->combat.id == owner) {
            return GetEnemyMultiplier(actor->combat.templateRef, 0);
        }
    }
    return fallback;
}

float CLevel::GetProjectilePowerupMultiplier(ZCombatId owner) const {
    if (owner == kPlayerCombatId && m_playerModel->weapon) { return m_playerModel->GetProjectilePowerupMultiplier(); }
    if (owner == kBrotherCombatId && m_brotherModel != nullptr && m_brotherModel->weapon) {
        return m_brotherModel->GetProjectilePowerupMultiplier();
    }
    return 1;
}

bool CLevel::FindTarget(const ZCombatHit &hit, float radius, float &x, float &y) {
    if (IsDeathmatch()) {
        const ZCombatId owner = ParticipantOwner(hit.owner);
        if (owner == kPlayerCombatId && m_brother != nullptr && !IsMatchSpawnPending(1) && !m_brother->vitals.dead && std::hypot(hit.x - m_brother->x, hit.y - m_brother->y) <= radius) {
            x = m_brother->x; y = m_brother->y; return true;
        }
        if (owner == kBrotherCombatId && !IsMatchSpawnPending(0) && !m_vitals->dead && std::hypot(hit.x - m_actor.x, hit.y - m_actor.y) <= radius) { x = m_actor.x; y = m_actor.y; return true; }
        if (owner == kPlayerCombatId || owner == kBrotherCombatId) { return false; }
    }
    bool found = false;
    if (hit.ownerType == 1 && !m_vitals->dead && std::hypot(hit.x - m_actor.x, hit.y - m_actor.y) < radius) {
        x = m_actor.x; y = m_actor.y; return true;
    }
    for (auto &actor : m_objects.GetEnemies()) {
        CEnemy &enemy = *actor;
        if (!enemy.combat.targetable || !enemy.CanReceiveProjectile(hit.ownerType, hit.owner)) { continue; }
        const float distance = std::hypot(enemy.combat.x - hit.x, enemy.combat.y - hit.y);
        if (distance < radius) { radius = distance; x = enemy.combat.x; y = enemy.combat.y; found = true; }
    }
    return found;
}

void CLevel::Splash(const ZCombatHit &hit, float radius, float coneDegrees, float force, int forceMs) {
    if (m_props != nullptr) { m_props->Splash(hit, radius); }
    std::vector<ZCombatId> targets;
    if (CanHitBrother(hit, kPlayerCombatId) && !m_vitals->dead) { targets.push_back(kPlayerCombatId); }
    if (CanHitBrother(hit, kBrotherCombatId) && m_brother != nullptr && !m_brother->vitals.dead) { targets.push_back(kBrotherCombatId); }
    for (const auto &actor : m_objects.GetEnemies()) {
        if (actor->CanReceiveProjectile(hit.ownerType, hit.owner)) { targets.push_back(actor->combat.id); }
    }
    for (ZCombatId id : targets) {
        float x = m_actor.x, y = m_actor.y;
        if (id == kBrotherCombatId) { x = m_brother->x; y = m_brother->y; }
        CEnemy *actor = Find(id);
        if (actor != nullptr) { x = actor->combat.x; y = actor->combat.y; }
        const float dx = x - hit.x, dy = y - hit.y;
        const float distance = std::hypot(dx, dy);
        // CLevel includes the target's collision radius in the blast test.
        // Testing only its centre drops explosions at the surface of big units.
        float targetRadius = m_playerRadius;
        if (actor != nullptr) {
            targetRadius = actor->GetPart(0).radius * actor->combat.scaleFactor;
        }
        if (distance > radius + targetRadius) { continue; }
        const float angle = std::atan2(dy, dx) / kRadians;
        const float difference = std::remainder(angle - hit.direction, 360.0f);
        if (coneDegrees < 360 && std::abs(difference) > coneDegrees * 0.5f) { continue; }
        ZCombatHit splash = hit;
        splash.part = 0;
        // OnSplashDamage still goes through class 6 event 2. The separate
        // splash flag tells the script which shield/part rules to apply.
        splash.splash = true;
        splash.part = -1;
        ApplyHit(id, splash);
        if (force > 0 && forceMs > 0 && distance > 0 && actor == nullptr) {
            // CBrother::OnSplashDamage :135359 uses SetForce over time.
            // CEnemy::OnSplashDamage :67945 does not apply positional force.
            // In particular, a full-size map must never clamp to Arena bounds.
            ApplyBrotherForce(id, dx / distance * force, dy / distance * force, forceMs);
        }
    }
}

void CLevel::SplashBrothers(float x, float y, float radius, float damage, float force, int forceMs) {
    // CProp::FireSplashDamageKnockBack :123456 uses actor centres, without
    // the collision-radius expansion of CLevel's general splash dispatcher.
    ZCombatHit hit;
    hit.ownerType = 1;
    hit.x = x; hit.y = y; hit.damage = damage;
    hit.splash = true; hit.applyArmorAttack = false;
    for (unsigned peer = 0; peer < 2; ++peer) {
        if (peer == 1 && m_brother == nullptr) { continue; }
        ZCombatId target = kPlayerCombatId;
        float dx = m_actor.x - x, dy = m_actor.y - y;
        if (peer == 1) { target = kBrotherCombatId; dx = m_brother->x - x; dy = m_brother->y - y; }
        const float distance = std::hypot(dx, dy);
        if (distance > radius) { continue; }
        ApplyHit(target, hit);
        if (force > 0 && forceMs > 0 && distance > 0) {
            ApplyBrotherForce(target, dx / distance * force, dy / distance * force, forceMs);
        }
    }
}

void CLevel::ApplyBrotherForce(ZCombatId target, float x, float y, int durationMs) {
    if (target == kBrotherCombatId && m_brotherModel != nullptr) {
        if (m_brotherModel->BeginKnockback(durationMs)) {
            m_brother->SetForce(x, y, durationMs);
        }
    } else if (target == kPlayerCombatId) {
        if (m_playerModel->weapon != nullptr && m_playerModel->BeginKnockback(durationMs)) {
            m_actor.forceX = x;
            m_actor.forceY = y;
            m_actor.forceMs = durationMs;
        }
    }
}

void CLevel::SpawnFromProjectile(const GameObjectRef &resource, const ZCombatHit &hit) {
    ZCombatId summoner = ParticipantOwner(hit.owner);
    if (summoner != kPlayerCombatId && summoner != kBrotherCombatId) { summoner = 0; }
    m_objects.QueueEnemy(resource, hit.x, hit.y, hit.spawnObjectId, hit.forceSpawn, summoner);
}

void CLevel::FinishSpawns() {
    for (CEnemy *actor : m_objects.FinishEnemySpawns()) {
        SelectTarget(*actor);
    }
}
