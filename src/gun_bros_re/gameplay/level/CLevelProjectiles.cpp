#include "gun_bros_re/gameplay/map/CLevelProps.h"
/** @file CLevelProjectiles.cpp
 * @brief CLevel projectile tracing, damage and splash dispatch.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/collision/Collision.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kRadians = 3.14159265f / 180;

bool Skipped(Collision::ObjectId id, const std::vector<Collision::ObjectId> &ids) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

using Collision::CircleFraction;
using Collision::EdgeFraction;
}

Collision::Trace CLevel::Trace(const Collision::Hit &hit, float x, float y, float dx, float dy,
    float radius, const std::vector<Collision::ObjectId> &skipTargets) {
    Collision::Trace result;
    float nearest = 2;
    if (m_props != nullptr) {
        result = m_props->Trace(hit, x, y, dx, dy, radius, skipTargets);
        if (result.target != 0) { nearest = result.fraction; }
    }
    if (CanHitBrother(hit, Collision::Player) && !m_vitals->dead && !Skipped(Collision::Player, skipTargets)) {
        float moveX = m_actor.x - m_actor.previousX, moveY = m_actor.y - m_actor.previousY;
        if ((hit.flags & 0x100) != 0) { moveX = 0; moveY = 0; }
        const float fraction = CircleFraction(x, y, dx - moveX, dy - moveY,
            m_actor.x - moveX, m_actor.y - moveY, m_playerRadius + radius);
        if (fraction <= 1 && fraction < nearest) {
            nearest = fraction;
            result.target = Collision::Player; result.fraction = nearest;
            result.normalX = x + dx * nearest - m_actor.x;
            result.normalY = y + dy * nearest - m_actor.y;
        }
    }
    if (CanHitBrother(hit, Collision::Brother) && m_brother != nullptr && !m_brother->vitals.dead && !Skipped(Collision::Brother, skipTargets)) {
        float moveX = m_brother->x - m_brother->previousX;
        float moveY = m_brother->y - m_brother->previousY;
        if ((hit.flags & 0x100) != 0) { moveX = 0; moveY = 0; }
        const float fraction = CircleFraction(x, y, dx - moveX, dy - moveY,
            m_brother->x - moveX, m_brother->y - moveY, m_playerRadius + radius);
        if (fraction <= 1 && fraction < nearest) {
            nearest = fraction;
            result = {Collision::Brother, fraction, -1, -1,
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

Collision::HitResult CLevel::ApplyHit(Collision::ObjectId target, const Collision::Hit &hit) {
    if (target == Collision::Brother && m_brotherModel != nullptr) {
        if (!CanHitBrother(hit, target)) { return Collision::HitResult::Ignored; }
        m_brotherModel->SetLevelContext(GetScriptLevel());
        const float reduction = m_brotherModel->GetArmorMultiplier(0) - 1;
        float damage = hit.damage;
        if (IsDeathmatch() && hit.applyArmorAttack && hit.owner == Collision::Player) { damage *= m_playerModel->GetArmorMultiplier(1); }
        if (hit.splash && hit.percentDamage) { damage *= m_brother->vitals.maximum * 0.01f; }
        const Collision::HitResult result = m_brotherModel->ReceiveDamage(std::max(0.0f, damage * (1 - reduction)) /
            CFriendPowerManager::Multiplier(m_brotherModel->friendCount, 1));
        if (result == Collision::HitResult::Hit && !hit.splash && hit.knockbackSpeed != 0) {
            ApplyBrotherForce(target, std::cos(hit.direction * kRadians) * hit.knockbackSpeed,
                std::sin(hit.direction * kRadians) * hit.knockbackSpeed, hit.knockbackDurationMs);
        }
        if (IsDeathmatch() && result != Collision::HitResult::Ignored) { m_matchStreaks[1] = 0; }
        if (result == Collision::HitResult::Killed) { RecordMatchDeath(1, hit.owner == Collision::Player ? 0 : -1); }
        return result;
    }
    if (target == Collision::Player) {
        if (!CanHitBrother(hit, target) || m_playerModel->weapon == nullptr) { return Collision::HitResult::Ignored; }
        m_playerModel->SetLevelContext(GetScriptLevel());
        // CBrother::Damage (:136667): add slot percentages, then reduce the
        // incoming amount. Defence does not increase the player's max health.
        const float reduction = m_playerModel->GetArmorMultiplier(0) - 1.0f;
        // CBrother::OnSplashDamage :135359 interprets native 23 as a percent
        // of maximum health before the ordinary armor / frenzy reductions.
        float damage = hit.damage;
        if (IsDeathmatch() && hit.applyArmorAttack && hit.owner == Collision::Brother) { damage *= m_brotherModel->GetArmorMultiplier(1); }
        if (hit.splash && hit.percentDamage) { damage *= m_vitals->maximum * 0.01f; }
        damage = std::max(0.0f, damage * (1.0f - reduction)) / CFriendPowerManager::Multiplier(m_playerModel->friendCount, 1);
        const unsigned hitsBefore = m_vitals->hits;
        const Collision::HitResult result = m_playerModel->ReceiveDamage(damage);
        // CBrother::HandleCollision :137829..137847, after accepted damage.
        // BeginKnockback retains the shield/death/already-forced guards.
        if (result == Collision::HitResult::Hit && !hit.splash && hit.knockbackSpeed != 0) {
            ApplyBrotherForce(target, std::cos(hit.direction * kRadians) * hit.knockbackSpeed,
                std::sin(hit.direction * kRadians) * hit.knockbackSpeed, hit.knockbackDurationMs);
        }
        if (IsDeathmatch() && result != Collision::HitResult::Ignored) { m_matchStreaks[0] = 0; }
        if (result == Collision::HitResult::Killed) { RecordMatchDeath(0, hit.owner == Collision::Brother ? 1 : -1); }
        // OnPlayerDamaged :115914 resets the streak on accepted damage only.
        if (m_vitals->hits != hitsBefore) { ResetKillStreak(); }
        return result;
    }
    Collision::Hit adjusted = hit;
    // CPlayer::GetDamage includes the active roster's native BRO BUFF.
    if (hit.owner == Collision::Player) { adjusted.damage *= CFriendPowerManager::Multiplier(m_playerModel->friendCount, 0); }
    if (hit.owner == Collision::Brother && m_brotherModel != nullptr) { adjusted.damage *= CFriendPowerManager::Multiplier(m_brotherModel->friendCount, 0); }
    if (hit.applyArmorAttack && hit.owner == Collision::Player) {
        adjusted.damage *= m_playerModel->GetArmorMultiplier(1);
    }
    if (hit.applyArmorAttack && hit.owner == Collision::Brother && m_brotherModel != nullptr) {
        adjusted.damage *= m_brotherModel->GetArmorMultiplier(1);
    }
    CEnemy *actor = Find(target);
    if (actor == nullptr) {
        if (m_props != nullptr) { return m_props->ApplyHit(target, adjusted); }
        return Collision::HitResult::Ignored;
    }
    const Collision::HitResult result = actor->ReceiveHit(adjusted);
    return result;
}

float CLevel::GetDamageMultiplier(Collision::ObjectId owner, float fallback) const {
    for (const auto &actor : m_objects.GetEnemies()) {
        if (actor->combat.id == owner) {
            return GetEnemyMultiplier(actor->combat.templateRef, 0);
        }
    }
    return fallback;
}

float CLevel::GetProjectilePowerupMultiplier(Collision::ObjectId owner) const {
    if (owner == Collision::Player && m_playerModel->weapon) { return m_playerModel->GetProjectilePowerupMultiplier(); }
    if (owner == Collision::Brother && m_brotherModel != nullptr && m_brotherModel->weapon) {
        return m_brotherModel->GetProjectilePowerupMultiplier();
    }
    return 1;
}

Collision::ObjectId CLevel::FindSeekTarget(const Collision::Hit &hit, float angle) {
    // CBrother::FindSeekTarget :136545: PvP targets the opposing living peer.
    if (IsDeathmatch()) {
        const Collision::ObjectId owner = ParticipantOwner(hit.owner);
        if (owner == Collision::Player && m_brother != nullptr && !IsMatchSpawnPending(1) && !m_brother->vitals.dead) { return Collision::Brother; }
        if (owner == Collision::Brother && !IsMatchSpawnPending(0) && !m_vitals->dead) { return Collision::Player; }
        if (owner == Collision::Player || owner == Collision::Brother) { return Collision::NoObject; }
    }
    // CBullet::UpdateSeeking :61604 chooses the nearer living brother.
    if (hit.ownerType == 1) {
        Collision::ObjectId result = Collision::NoObject;
        float nearest = std::numeric_limits<float>::max();
        if (!m_vitals->dead) {
            nearest = std::hypot(hit.x - m_actor.x, hit.y - m_actor.y);
            result = Collision::Player;
        }
        if (m_brother != nullptr && !m_brother->vitals.dead &&
            std::hypot(hit.x - m_brother->x, hit.y - m_brother->y) < nearest) { result = Collision::Brother; }
        return result;
    }
    Collision::ObjectId result = Collision::NoObject;
    float nearest = std::numeric_limits<float>::max();
    const float directionX = std::cos(hit.direction * kRadians);
    const float directionY = std::sin(hit.direction * kRadians);
    for (const auto &actor : m_objects.GetEnemies()) {
        const CEnemy &enemy = *actor;
        if (!enemy.combat.targetable || !enemy.CanReceiveProjectile(hit.ownerType, hit.owner)) { continue; }
        const float dx = enemy.combat.x - hit.x;
        const float dy = enemy.combat.y - hit.y;
        const float distance = std::hypot(dx, dy);
        if (distance <= 0 || distance >= nearest) { continue; }
        const float cosine = std::clamp((dx * directionX + dy * directionY) / distance, -1.0f, 1.0f);
        // The original compares the angle strictly; it has no radius cutoff.
        if (std::acos(cosine) / kRadians >= angle) { continue; }
        nearest = distance;
        result = enemy.combat.id;
    }
    return result;
}

void CLevel::Splash(const Collision::Hit &hit, float radius, float coneDegrees, float force, int forceMs) {
    if (m_props != nullptr) { m_props->Splash(hit, radius); }
    std::vector<Collision::ObjectId> targets;
    if (CanHitBrother(hit, Collision::Player) && !m_vitals->dead) { targets.push_back(Collision::Player); }
    if (CanHitBrother(hit, Collision::Brother) && m_brother != nullptr && !m_brother->vitals.dead) { targets.push_back(Collision::Brother); }
    for (const auto &actor : m_objects.GetEnemies()) {
        if (actor->CanReceiveProjectile(hit.ownerType, hit.owner)) { targets.push_back(actor->combat.id); }
    }
    for (Collision::ObjectId id : targets) {
        float x = m_actor.x, y = m_actor.y;
        if (id == Collision::Brother) { x = m_brother->x; y = m_brother->y; }
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
        Collision::Hit splash = hit;
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
    Collision::Hit hit;
    hit.ownerType = 1;
    hit.x = x; hit.y = y; hit.damage = damage;
    hit.splash = true; hit.applyArmorAttack = false;
    for (unsigned peer = 0; peer < 2; ++peer) {
        if (peer == 1 && m_brother == nullptr) { continue; }
        Collision::ObjectId target = Collision::Player;
        float dx = m_actor.x - x, dy = m_actor.y - y;
        if (peer == 1) { target = Collision::Brother; dx = m_brother->x - x; dy = m_brother->y - y; }
        const float distance = std::hypot(dx, dy);
        if (distance > radius) { continue; }
        ApplyHit(target, hit);
        if (force > 0 && forceMs > 0 && distance > 0) {
            ApplyBrotherForce(target, dx / distance * force, dy / distance * force, forceMs);
        }
    }
}

void CLevel::ApplyBrotherForce(Collision::ObjectId target, float x, float y, int durationMs) {
    if (target == Collision::Brother && m_brotherModel != nullptr) {
        if (m_brotherModel->BeginKnockback(durationMs)) {
            m_brother->SetForce(x, y, durationMs);
        }
    } else if (target == Collision::Player) {
        if (m_playerModel->weapon != nullptr && m_playerModel->BeginKnockback(durationMs)) {
            m_actor.forceX = x;
            m_actor.forceY = y;
            m_actor.forceMs = durationMs;
        }
    }
}

void CLevel::SpawnFromProjectile(const GameObjectRef &resource, const Collision::Hit &hit) {
    Collision::ObjectId summoner = ParticipantOwner(hit.owner);
    if (summoner != Collision::Player && summoner != Collision::Brother) { summoner = 0; }
    m_objects.QueueEnemy(resource, hit.x, hit.y, hit.spawnObjectId, hit.forceSpawn, summoner);
}

void CLevel::FinishSpawns() {
    for (CEnemy *actor : m_objects.FinishEnemySpawns()) {
        SelectTarget(*actor);
    }
}
