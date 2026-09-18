/** @file CLevelEnemies.cpp
 * @brief Enemy allocation, targeting, navigation and native side-effect dispatch.
 * Original: src/gunbros/level.cpp UpdateNormal :121150, enemy.cpp native actions
 * :71692, GetRotationOffset :71429. These are CLevel methods, not a new class.
 * Host adaptation: local Bot target selection and graphics matrix arguments.
 */
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/debug/PerformanceProbe.h"
#include "gun_bros_re/gameplay/ZCombatGeometry.h"
#include "engine/core/ZMatrix4d.h"
#include "engine/graphics/CMeshCamera.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace {
constexpr float kRadians = 3.14159265f / 180;
}
void CLevel::EnemyMatrix(const CEnemy &actor, float *matrix) const {
    float identity[16];
    Matrix4dIdentity(identity);
    const CEnemy::CombatState &state = actor.combat;
    const float scale = actor.GetWorldScale(actor.data->gameScale, m_cameraScale) * state.scaleFactor;
    actor.BuildGameMatrix(identity, state.x, state.y, scale, state.facing, matrix);
}

void CLevel::PartMatrix(const CEnemy &actor, int index, float *matrix) const {
    float base[16];
    EnemyMatrix(actor, base);
    const CEnemy::Part &part = actor.GetPart(index);
    if (!part.followsFacing) {
        float identity[16];
        Matrix4dIdentity(identity);
        const CEnemy::CombatState &state = actor.combat;
        actor.BuildGameMatrix(identity, state.x, state.y,
            actor.GetWorldScale(actor.data->gameScale, m_cameraScale) * state.scaleFactor, 0, base);
    }
    ZMeshPart placement;
    placement.extraAngleDegrees = part.extraAngleDegrees;
    placement.extraAxisX = part.extraAxisX;
    placement.extraAxisY = part.extraAxisY;
    placement.extraAxisZ = part.extraAxisZ;
    if (part.boneIndex >= 0) {
        actor.GetPart(0).controller.GetAnimation().GetNodeAt(part.boneIndex, placement.attachment);
    }
    MeshCameraBuildPartMatrix(placement, base, matrix);
}

void CLevel::SelectTarget(CEnemy &actor) {
    CEnemy &enemy = actor;
    if (IsDeathmatch() && enemy.combat.summoner != 0) {
        if (enemy.combat.summoner == kPlayerCombatId && m_brother != nullptr) {
            enemy.SetTarget(kBrotherCombatId, m_brother->x, m_brother->y, !IsMatchSpawnPending(1) && !m_brother->vitals.dead);
        } else { enemy.SetTarget(kPlayerCombatId, m_actor.x, m_actor.y, !IsMatchSpawnPending(0) && !m_vitals->dead); }
        return;
    }
    if (enemy.combat.targetType != 2) {
        // Local peer has no network target packet: choose the nearest living
        // brother on this host, including while the human player is down.
        if (m_localLive && m_brother != nullptr && !m_brother->vitals.dead &&
            (m_vitals->dead || std::hypot(m_brother->x - enemy.combat.x, m_brother->y - enemy.combat.y) <
                std::hypot(m_actor.x - enemy.combat.x, m_actor.y - enemy.combat.y))) {
            enemy.SetTarget(kBrotherCombatId, m_brother->x, m_brother->y, true);
            return;
        }
        enemy.SetTarget(kPlayerCombatId, m_actor.x, m_actor.y, !m_vitals->dead);
        return;
    }
    CEnemy *nearest = nullptr;
    float distance = 100000;
    for (auto &other : m_objects.GetEnemies()) {
        const CEnemy &target = *other;
        if (!target.combat.enabled || !target.combat.targetable ||
            !target.CanReceiveProjectile(0, enemy.combat.id)) { continue; }
        const float current = std::hypot(target.combat.x - enemy.combat.x, target.combat.y - enemy.combat.y);
        if (current < distance) { distance = current; nearest = other.get(); }
    }
    if (nearest != nullptr) {
        const CEnemy::CombatState &target = nearest->combat;
        enemy.SetTarget(target.id, target.x, target.y, true);
    } else { enemy.SetTarget(0, enemy.combat.x, enemy.combat.y, false); }
}

void CLevel::EnemyCircle(const CEnemy &actor, int part, float &x, float &y, float &radius) const {
    const CEnemy &enemy = actor;
    const CEnemy::CombatState &state = enemy.combat;
    x = state.x;
    y = state.y;
    enemy.GetCollisionCircle(actor.data->gameScale, part, x, y, radius);
}

void CLevel::UpdateNavigation(CEnemy &actor) {
    // Historical host heuristic: Skip centres only when the actual collision sweep has a clear corridor.
    // Replaced by the original CMeshPathFinder shared-edge destination below.
    PerformanceProbe::Scope timing(PerformanceProbe::counters.navigationMs);
    CEnemy::CombatState &state = actor.combat;
    state.hasNavigationTarget = false;
    if (m_map == nullptr || state.behaviour != 0 || state.dead || !state.targetAlive) { return; }
    ILayerPath *path = m_map->GetPathLayer(m_pathLayer);
    if (path == nullptr) { return; }
    const auto *mesh = dynamic_cast<const CLayerPathMesh *>(path);
    if (mesh != nullptr) {
        actor.UpdateNavigation(*mesh, m_flock.GetDistanceMap(state.targetId));
        return;
    }
    // Legacy link-map target pursuit retains graph-node routing. Authored
    // behaviour 2 routes are owned by CEnemy's CLinkPathFinder, not this query.
    const int start = path->FindNode(state.x, state.y);
    const int destination = path->FindNode(state.targetX, state.targetY);
    const int next = path->FindNext(start, destination);
    if (next < 0 || next == destination) { return; }
    state.hasNavigationTarget = true;
    path->GetConnectionPoint(start, next, state.navigationX, state.navigationY);
}

bool CLevel::PreloadEnemies(const RequirementList &requirements, const CScript &levelScript) {
    return m_objects.PreloadEnemies(requirements, levelScript);
}

CEnemy *CLevel::Spawn(std::size_t entry, float x, float y) {
    CEnemy *actor = m_objects.SpawnEnemy(entry, x, y);
    if (actor != nullptr) { SelectTarget(*actor); }
    return actor;
}

CEnemy *CLevel::SpawnNearby(std::size_t entry) {
    CEnemy *actor = m_objects.GetNearbyEnemy(entry, m_actor.x, m_actor.y);
    if (actor != nullptr) { SelectTarget(*actor); }
    return actor;
}

CEnemy *CLevel::Find(ZCombatId id) {
    return m_objects.FindEnemy(id);
}

const CEnemy *CLevel::Find(ZCombatId id) const {
    for (const auto &actor : m_objects.GetEnemies()) {
        if (actor->combat.id == id) { return actor.get(); }
    }
    return nullptr;
}

std::size_t CLevel::AliveCount() const {
    return m_objects.GetAliveEnemyCount();
}

void CLevel::Actions(CEnemy &actor) {
    CEnemy &enemy = actor;
    const CEnemy::CombatState &state = enemy.combat;
    for (const CEnemy::Action &action : enemy.TakeActions()) {
        float x = state.x, y = state.y, z = 0, direction = state.facing - 90;
        // Death effects still use the final pose, even though the actor is no
        // longer a valid continuous beam/effect anchor.
        if (!state.dead) { Anchor(state.id, action.part, action.node, x, y, z, direction); }
        int ownerType = 1;
        if (state.targetType == 2) { ownerType = 0; }
        if (action.kind == CEnemy::Action::Kind::LevelEvent) {
            QueueLevelEvent(static_cast<std::uint8_t>(action.slot));
        } else if (action.kind == CEnemy::Action::Kind::Teleported) {
            QueueEnemyTeleport(actor.objectId, state.templateRef);
        } else if (action.kind == CEnemy::Action::Kind::Shake) {
            if (m_map != nullptr) { m_map->GetCamera().Shake(action.durationMs); }
        } else if (action.kind == CEnemy::Action::Kind::TurretActive) {
            // CEnemy native 71 :72744 selects the local player when offline.
            CBrother *owner = m_playerModel;
            if (state.summoner == kBrotherCombatId && m_brotherModel != nullptr) { owner = m_brotherModel; }
            owner->SetTurretIsActive(action.slot != 0);
            std::printf("[turret] actor=%llu active=%d\n", static_cast<unsigned long long>(state.id), action.slot != 0);
        } else if (action.kind == CEnemy::Action::Kind::SpawnPickup) {
            QueuePickupSpawn(action.resource, x, y);
        } else if (action.kind == CEnemy::Action::Kind::Bullet) {
            if (action.slot != 1) { direction = action.direction - 90; }
            SpawnProjectile(action.resource, x, y, z, direction,
                action.speed, state.id, ownerType, action.part, action.node);
        } else if (action.kind == CEnemy::Action::Kind::Stun) {
            if (ownerType == 1 && std::hypot(m_actor.x - x, m_actor.y - y) < action.radius) {
                m_playerModel->Stun(action.durationMs);
            }
            if (ownerType == 1 && m_brother != nullptr && std::hypot(m_brother->x - x, m_brother->y - y) < action.radius) {
                m_brotherModel->Stun(action.durationMs);
            }
        } else if (action.kind == CEnemy::Action::Kind::CollisionResolved) {
            // Record assistance only after the enemy Flow accepts the collision.
            if (m_localLive && (action.result == ZHitResult::Hit || action.result == ZHitResult::Killed) &&
                !action.resource.IsNull() && action.slot >= 0 && action.slot < 2) {
                if (action.owner == kPlayerCombatId) { actor.assistMask[0] |= 1u << action.slot; }
                if (action.owner == kBrotherCombatId) { actor.assistMask[1] |= 1u << action.slot; }
            }
            ResolveHit(action.projectile, action.result);
        } else if (action.kind == CEnemy::Action::Kind::RemoveBullet) {
            RemoveOldestProjectile(state.id);
        } else if (action.kind == CEnemy::Action::Kind::Broadcast) {
            for (auto &other : m_objects.GetEnemies()) {
                if (other.get() != &actor && !other->combat.dead) {
                    other->TriggerEvent(static_cast<std::uint8_t>(action.slot));
                }
            }
        } else if (action.kind == CEnemy::Action::Kind::SpawnEnemy) {
            // Native 54 spawns from the LEVEL table, with no player summoner.
            m_objects.QueueEnemy(action.resource, action.x, action.y, action.slot, false, 0);
        } else if (action.kind == CEnemy::Action::Kind::Splash) {
            ZCombatHit hit;
            hit.owner = state.id;
            hit.ownerType = ownerType;
            hit.x = x; hit.y = y; hit.direction = direction;
            hit.damage = action.damage * GetDamageMultiplier(state.id);
            Splash(hit, action.radius, 360, action.force, action.durationMs);
        } else {
            ZGunCue cue;
            cue.resource = action.resource;
            cue.effectGroup = action.effectGroup;
            cue.effectScale = action.effectScale;
            cue.alignEffect = action.alignEffect;
            cue.linkedEnemyEffect = action.kind == CEnemy::Action::Kind::LinkedEffect;
            cue.kind = ZGunCue::Kind::Effect;
            if (action.kind == CEnemy::Action::Kind::Sound) { cue.kind = ZGunCue::Kind::Sound; }
            if (action.kind == CEnemy::Action::Kind::LoopSound) { cue.kind = ZGunCue::Kind::LoopSound; }
            if (action.kind == CEnemy::Action::Kind::StopSound) { cue.kind = ZGunCue::Kind::StopSound; }
            if (action.kind == CEnemy::Action::Kind::LinkedEffect) { cue.kind = ZGunCue::Kind::Trail; }
            if (action.kind == CEnemy::Action::Kind::StopEffect) { cue.kind = ZGunCue::Kind::StopTrail; }
            if (action.kind == CEnemy::Action::Kind::Shake || action.kind == CEnemy::Action::Kind::Reward) { continue; }
            Emit(cue, x, y, z, direction, state.id, action.slot, action.part, action.node);
        }
    }
}
