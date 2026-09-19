/** @file CLevelActors.cpp
 * @brief CLevel actor transforms, targeting and brother integration.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "engine/core/ZMatrix4d.h"
#include "engine/graphics/CMeshCamera.h"
#include "gun_bros_re/data/store/CStoreItem.h"
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
    const float scale = m_playerModel->GetWorldScale(m_playerGameScale, m_cameraScale);
    MeshCameraBuildGameMatrix(identity, m_actor.x, m_actor.y, scale, m_actor.facing, matrix);
}

void CLevel::SetBrother(CBrother *model, CBrotherAI *brother) {
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
    return m_brotherModel->OnSwapGun();
}

bool CLevel::SwapBrotherWeapon() {
    if (IsDeathmatch()) { return FinishMatchWeaponSwap(1); }
    if (m_brotherScript == nullptr || m_brotherModel == nullptr || m_brother->vitals.dead) { return true; }
    const unsigned next = 1 - m_brotherWeaponSlot;
    RetireOwner(Collision::Brother);
    // CBrother native 3 changes the gun while the same body/script continues
    // the swap sequence. Reuse the two stable banks used by the local player.
    if (m_brotherModel->uiOtherWeapon == nullptr) {
        if (!m_brotherModel->PrepareSecondaryWeapon(*m_tables, *m_brotherWeapons[1], "AI brother swap") ||
            !m_brotherModel->CreateBuffers(*m_program)) { return false; }
    }
    m_brotherModel->SelectWeapon(next == 0);
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
    const float scale = m_brotherModel->GetWorldScale(m_playerGameScale, m_cameraScale);
    MeshCameraBuildGameMatrix(identity, m_brother->x, m_brother->y, scale, m_brother->facing, matrix);
}

Collision::ObjectId CLevel::FindBrotherTarget(float x, float y, float radius) {
    if (IsDeathmatch()) {
        if (!IsMatchSpawnPending(0) && !m_vitals->dead && std::hypot(x - m_actor.x, y - m_actor.y) <= radius && HasLineOfFire(x, y, m_actor.x, m_actor.y)) { return Collision::Player; }
        return 0;
    }
    Collision::ObjectId nearest = 0;
    for (const auto &actor : m_objects.GetEnemies()) {
        float targetX = 0;
        float targetY = 0;
        const Collision::ObjectId id = actor->combat.id;
        if (!GetBrotherTarget(id, targetX, targetY)) { continue; }
        const float distance = std::hypot(targetX - x, targetY - y);
        if (distance < radius) { radius = distance; nearest = id; }
    }
    return nearest;
}

bool CLevel::GetBrotherTarget(Collision::ObjectId id, float &x, float &y) {
    if (IsDeathmatch() && id == Collision::Player && !IsMatchSpawnPending(0) && !m_vitals->dead) { x = m_actor.x; y = m_actor.y; return true; }
    CEnemy *actor = Find(id);
    if (actor == nullptr) { return false; }
    const CEnemy &enemy = *actor;
    if (!enemy.combat.enabled || !enemy.combat.targetable ||
        !enemy.CanReceiveProjectile(0, Collision::Brother)) { return false; }
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


bool CLevel::Anchor(Collision::ObjectId id, int part, int node, float &x, float &y, float &z, float &direction) {
    if (id == Collision::Player && IsMatchSpawnPending(0)) { return false; }
    if (id == Collision::Brother && IsMatchSpawnPending(1)) { return false; }
    if (id == Collision::Player && part < 0) {
        x = m_actor.x; y = m_actor.y; z = 0; direction = m_actor.facing - 90;
        return !m_vitals->dead;
    }
    if (id == Collision::Brother && m_brotherModel != nullptr) {
        if (part < 0) {
            x = m_brother->x; y = m_brother->y; z = 0; direction = m_brother->facing - 90;
            return !m_brother->vitals.dead;
        }
        if (m_brother->vitals.dead || !m_brotherModel->weapon->IsShooting()) { return false; }
        ZMeshBoneTransform muzzle;
        if (!m_brotherModel->GetMuzzle(part, node, muzzle)) { return false; }
        float matrix[16];
        BrotherMatrix(matrix);
        Transform(matrix, muzzle.posX, muzzle.posY, muzzle.posZ, x, y, z);
        direction = m_brother->facing - 90;
        return true;
    }
    CEnemy *actor = Find(id);
    if (actor == nullptr || actor->combat.removed || actor->combat.dead) { return false; }
    CEnemy &enemy = *actor;
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


bool CLevel::ParticleAnchor(Collision::ObjectId actor, float &x, float &y, float &z, float &angle) {
    // CBrother::GetParticleEffectAnchor :134152 returns position and zero
    // rotation even while dying. Respawn explicitly detaches the previous life.
    if (actor == Collision::Player) {
        if (IsMatchSpawnPending(0)) { return false; }
        x = m_actor.x; y = m_actor.y; z = 0; angle = 0;
        return true;
    }
    if (actor == Collision::Brother && m_brother != nullptr) {
        if (IsMatchSpawnPending(1)) { return false; }
        x = m_brother->x; y = m_brother->y; z = 0; angle = 0;
        return true;
    }
    return Anchor(actor, -1, -1, x, y, z, angle);
}

bool CLevel::LinkedParticleAnchor(Collision::ObjectId id, int node, float &x, float &y, float &z, float &angle) {
    auto *actor = Find(id);
    if (actor == nullptr) { return false; }
    const auto &state = actor->combat;
    float nodeDirection = 0;
    // GetParticleEffectAnchor :68615 re-reads the active part on each update.
    if (!Anchor(id, state.variables[14], node, x, y, z, nodeDirection)) { return false; }
    angle = state.facing;
    return true;
}
