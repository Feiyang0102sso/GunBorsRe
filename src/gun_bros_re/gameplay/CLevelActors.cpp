/** @file CLevelActors.cpp
 * @brief CLevel actor transforms, targeting and brother integration.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/CLevel.h"
#include "engine/core/ZMatrix4d.h"
#include "engine/graphics/CMeshCamera.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include <cmath>
#include <cstdio>

namespace {
constexpr float kRadians = 3.14159265f / 180;

void Transform(const float *matrix, float localX, float localY, float localZ,
    float &x, float &y, float &z) {
    x = matrix[0] * localX + matrix[1] * localY + matrix[2] * localZ + matrix[3];
    y = matrix[4] * localX + matrix[5] * localY + matrix[6] * localZ + matrix[7];
    z = matrix[8] * localX + matrix[9] * localY + matrix[10] * localZ + matrix[11];
}
}

void CLevel::PlayerMatrix(float *matrix) const {
    float identity[16];
    Matrix4dIdentity(identity);
    const float scale = PlayerModelWorldScale(*m_playerModel, m_playerGameScale, m_cameraScale);
    BuildPlayerGameMatrix(identity, m_actor.x, m_actor.y, scale, m_actor.facing, matrix);
}

void CLevel::SetBrother(ZPlayerModel *model, CBrotherAI *brother) {
    m_brotherModel = model;
    m_brother = brother;
}

void CLevel::SetBrotherWeapons(const CScript &script, const CGun::Template &pistol, const CGun::Template &rifle) {
    m_brotherScript = &script;
    m_brotherWeapons[0] = &pistol;
    m_brotherWeapons[1] = &rifle;
    m_brotherWeaponSlot = 0;
}

bool CLevel::RequestBrotherWeaponSwap() {
    if (IsDeathmatch()) { return RequestMatchWeaponSwap(1); }
    if (m_brother == nullptr || m_brotherModel == nullptr || m_brotherScript == nullptr || m_brother->vitals.dead) { return false; }
    return m_brotherModel->weapon->brother.OnSwapGun();
}

bool CLevel::SwapBrotherWeapon() {
    if (IsDeathmatch()) { return FinishMatchWeaponSwap(1); }
    if (m_brotherScript == nullptr || m_brotherModel == nullptr || m_brother->vitals.dead) { return true; }
    const unsigned next = 1 - m_brotherWeaponSlot;
    m_effects->RetireOwner(kBrotherCombatId);
    // CBrother native 3 changes the gun while the same body/script continues
    // the swap sequence. Reuse the two stable banks used by the local player.
    if (m_brotherModel->uiOtherWeapon == nullptr) {
        if (!PreparePlayerUIWeapon(*m_tables, *m_brotherWeapons[1], "AI brother swap", *m_brotherModel) ||
            !CreatePlayerBuffers(*m_brotherModel, *m_program)) { return false; }
    }
    SelectPlayerUIWeapon(*m_brotherModel, next == 0);
    m_brotherWeaponSlot = next;
    m_brotherModel->gunSlot = next;
    m_brotherModel->gunResource = m_gunConfigurations[1][next];
    std::printf("[brother] weapon-slot=%u\n", next);
    return true;
}

void CLevel::ResetBrotherPosition(float x, float y, float facingDegrees) {
    if (m_brother == nullptr) { return; }
    // The host used to reset both actors onto the same point. Pick an open
    // nearby position through the actual collision path, including on restart.
    constexpr float directions[][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1},
        {-0.7071f, -0.7071f}, {0.7071f, -0.7071f}, {-0.7071f, 0.7071f}, {0.7071f, 0.7071f}};
    const float distance = m_playerRadius * 4;
    for (const auto &direction : directions) {
        const float targetX = x + direction[0] * distance;
        const float targetY = y + direction[1] * distance;
        if (!CanWalkTo(x, y, targetX, targetY)) { continue; }
        m_brother->Reset(targetX, targetY, facingDegrees);
        return;
    }
    // Extremely tight authored spawn areas still have a deterministic fallback.
    m_brother->Reset(x, y, facingDegrees);
}

void CLevel::BrotherMatrix(float *matrix) const {
    float identity[16];
    Matrix4dIdentity(identity);
    const float scale = PlayerModelWorldScale(*m_brotherModel, m_playerGameScale, m_cameraScale);
    BuildPlayerGameMatrix(identity, m_brother->x, m_brother->y, scale, m_brother->facing, matrix);
}

ZCombatId CLevel::FindBrotherTarget(float x, float y, float radius) {
    if (IsDeathmatch()) {
        if (!IsMatchSpawnPending(0) && !m_vitals->dead && std::hypot(x - m_actor.x, y - m_actor.y) <= radius && HasLineOfFire(x, y, m_actor.x, m_actor.y)) { return kPlayerCombatId; }
        return 0;
    }
    ZCombatId nearest = 0;
    for (const auto &actor : m_objects.GetEnemies()) {
        float targetX = 0;
        float targetY = 0;
        const ZCombatId id = actor->model.enemy.combat.id;
        if (!GetBrotherTarget(id, targetX, targetY)) { continue; }
        const float distance = std::hypot(targetX - x, targetY - y);
        if (distance < radius) { radius = distance; nearest = id; }
    }
    return nearest;
}

bool CLevel::GetBrotherTarget(ZCombatId id, float &x, float &y) {
    if (IsDeathmatch() && id == kPlayerCombatId && !IsMatchSpawnPending(0) && !m_vitals->dead) { x = m_actor.x; y = m_actor.y; return true; }
    ZCombatEnemy *actor = Find(id);
    if (actor == nullptr) { return false; }
    const CEnemy &enemy = actor->model.enemy;
    if (!enemy.combat.enabled || !enemy.combat.targetable ||
        !enemy.CanReceiveProjectile(0, kBrotherCombatId)) { return false; }
    x = enemy.combat.x;
    y = enemy.combat.y;
    return true;
}

bool CLevel::GetBrotherWaypoint(float x, float y, float targetX, float targetY,
    float &waypointX, float &waypointY) {
    waypointX = targetX;
    waypointY = targetY;
    if (HasClearPath(x, y, targetX, targetY, m_playerRadius)) { return true; }
    if (m_map == nullptr) { return false; }
    ILayerPath *path = m_map->GetPathLayer(m_pathLayer);
    if (path == nullptr) { return false; }
    const int start = path->FindNode(x, y);
    const int destination = path->FindNode(targetX, targetY);
    if (start < 0 || destination < 0) { return false; }
    const int next = path->FindNext(start, destination);
    if (next < 0) { return false; }
    waypointX = path->GetNodes()[next].x;
    waypointY = path->GetNodes()[next].y;
    if (next != start) { path->GetConnectionPoint(start, next, waypointX, waypointY); }
    if (std::hypot(waypointX - x, waypointY - y) < 5) {
        waypointX = path->GetNodes()[next].x;
        waypointY = path->GetNodes()[next].y;
    }
    return true;
}

void CLevel::EnemyMatrix(const ZCombatEnemy &actor, float *matrix) const {
    float identity[16];
    Matrix4dIdentity(identity);
    const ZEnemyCombat &state = actor.model.enemy.combat;
    const float scale = EnemyModelWorldScale(actor.model, actor.data->gameScale, m_cameraScale) * state.scaleFactor;
    BuildEnemyGameMatrix(actor.model, identity, state.x, state.y, scale, state.facing, matrix);
}

void CLevel::PartMatrix(const ZCombatEnemy &actor, int index, float *matrix) const {
    float base[16];
    EnemyMatrix(actor, base);
    const ZEnemyPart &part = actor.model.enemy.GetPart(index);
    if (!part.followsFacing) {
        float identity[16];
        Matrix4dIdentity(identity);
        const ZEnemyCombat &state = actor.model.enemy.combat;
        BuildEnemyGameMatrix(actor.model, identity, state.x, state.y,
            EnemyModelWorldScale(actor.model, actor.data->gameScale, m_cameraScale) * state.scaleFactor, 0, base);
    }
    ZMeshPart placement;
    placement.extraAngleDegrees = part.extraAngleDegrees;
    placement.extraAxisX = part.extraAxisX;
    placement.extraAxisY = part.extraAxisY;
    placement.extraAxisZ = part.extraAxisZ;
    if (part.boneIndex >= 0) {
        actor.model.enemy.GetPart(0).controller.GetAnimation().GetNodeAt(part.boneIndex, placement.attachment);
    }
    MeshCameraBuildPartMatrix(placement, base, matrix);
}

bool CLevel::Anchor(ZCombatId id, int part, int node, float &x, float &y, float &z, float &direction) {
    if (id == kPlayerCombatId && IsMatchSpawnPending(0)) { return false; }
    if (id == kBrotherCombatId && IsMatchSpawnPending(1)) { return false; }
    if (id == kPlayerCombatId && part < 0) {
        x = m_actor.x; y = m_actor.y; z = 0; direction = m_actor.facing - 90;
        return !m_vitals->dead;
    }
    if (id == kBrotherCombatId && m_brotherModel != nullptr) {
        if (part < 0) {
            x = m_brother->x; y = m_brother->y; z = 0; direction = m_brother->facing - 90;
            return !m_brother->vitals.dead;
        }
        if (m_brother->vitals.dead || !m_brotherModel->weapon->gun.IsShooting()) { return false; }
        ZMeshBoneTransform muzzle;
        if (!GetPlayerMuzzle(*m_brotherModel, part, node, muzzle)) { return false; }
        float matrix[16];
        BrotherMatrix(matrix);
        Transform(matrix, muzzle.posX, muzzle.posY, muzzle.posZ, x, y, z);
        direction = m_brother->facing - 90;
        return true;
    }
    ZCombatEnemy *actor = Find(id);
    if (actor == nullptr || actor->model.enemy.combat.removed || actor->model.enemy.combat.dead) { return false; }
    CEnemy &enemy = actor->model.enemy;
    x = enemy.combat.x;
    y = enemy.combat.y;
    z = 0;
    direction = enemy.combat.facing - 90;
    if (part < 0 || part >= static_cast<int>(enemy.GetPartCount()) || node < 0) { return true; }
    ZMeshBoneTransform bone;
    if (!enemy.GetPart(part).controller.GetAnimation().GetNodeAt(node, bone)) { return true; }
    float matrix[16];
    PartMatrix(*actor, part, matrix);
    Transform(matrix, bone.posX, bone.posY, bone.posZ, x, y, z);
    ZMeshPart nodePlacement;
    nodePlacement.attachment = bone;
    float nodeMatrix[16];
    MeshCameraBuildPartMatrix(nodePlacement, matrix, nodeMatrix);
    const float forwardX = -nodeMatrix[1];
    const float forwardY = -nodeMatrix[5];
    if (std::hypot(forwardX, forwardY) > 0.0001f) {
        direction = std::atan2(forwardY, forwardX) / kRadians;
    }
    return true;
}

void CLevel::SelectTarget(ZCombatEnemy &actor) {
    CEnemy &enemy = actor.model.enemy;
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
    ZCombatEnemy *nearest = nullptr;
    float distance = 100000;
    for (auto &other : m_objects.GetEnemies()) {
        const CEnemy &target = other->model.enemy;
        if (!target.combat.enabled || !target.combat.targetable ||
            !target.CanReceiveProjectile(0, enemy.combat.id)) { continue; }
        const float current = std::hypot(target.combat.x - enemy.combat.x, target.combat.y - enemy.combat.y);
        if (current < distance) { distance = current; nearest = other.get(); }
    }
    if (nearest != nullptr) {
        const ZEnemyCombat &target = nearest->model.enemy.combat;
        enemy.SetTarget(target.id, target.x, target.y, true);
    } else { enemy.SetTarget(0, enemy.combat.x, enemy.combat.y, false); }
}

void CLevel::EnemyCircle(const ZCombatEnemy &actor, int part, float &x, float &y, float &radius) const {
    const CEnemy &enemy = actor.model.enemy;
    const ZEnemyCombat &state = enemy.combat;
    x = state.x;
    y = state.y;
    EnemyCollisionCircle(enemy, actor.data->gameScale, part, x, y, radius);
}
