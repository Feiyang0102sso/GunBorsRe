/** @file CLevelWorld.cpp
 * @brief CLevel map collision, navigation and enemy object access.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/ZCombatGeometry.h"
#include "gun_bros_re/debug/PerformanceProbe.h"
#include <algorithm>
#include <cmath>

namespace {
using CombatGeometry::EdgeFraction;
}

void CLevel::SetMap(CMap &map, const CCollisionData &collision, ZWeaponCollision &weaponCollision,
    float cameraScale, float playerRadius) {
    m_map = &map;
    m_objects.SetUsesMapCoordinates(true);
    m_collision = &collision;
    m_weaponCollision = &weaponCollision;
    m_cameraScale = cameraScale;
    m_playerRadius = playerRadius;
    m_actor.BindLevel(map, collision, m_objects, playerRadius);
    const ZMapRectangle bounds = map.GetCameraExtent();
    m_left = bounds.x + playerRadius;
    m_top = bounds.y + playerRadius;
    m_right = bounds.x + bounds.width - playerRadius;
    m_bottom = bounds.y + bounds.height - playerRadius;
}

void CLevel::ResolveMovement(float previousX, float previousY, float &x, float &y, float radius, bool player) const {
    // CEnemy::UpdatePathFinder (:70142) advances on its navigation path;
    // TestCollisions (:73079) tests bullets/player, not the player's wall
    // circle resolver. Applying that resolver again can block authored portals.
    if (m_collision != nullptr && player) {
        const ZCollisionPoint position = m_collision->ResolveCircleMovement(
            ZCollisionPoint(previousX, previousY), ZCollisionPoint(x - previousX, y - previousY), radius);
        x = position.x;
        y = position.y;
    }
    // Camera bounds constrain the player. Authored enemy spawn nodes can be
    // outside the visible rectangle and must remain there until they enter.
    if (player || m_map == nullptr) {
        x = std::clamp(x, m_left, m_right);
        y = std::clamp(y, m_top, m_bottom);
    }
}

bool CLevel::HasClearPath(float x, float y, float targetX, float targetY, float radius) const {
    if (m_collision == nullptr) { return true; }
    const auto &vertices = m_collision->GetVertices();
    const float left = std::min(x, targetX) - radius, right = std::max(x, targetX) + radius;
    const float top = std::min(y, targetY) - radius, bottom = std::max(y, targetY) + radius;
    for (const ZCollisionEdge &edge : m_collision->GetEdges()) {
        if (!edge.enabled) { continue; }
        // Broad phase only; the existing swept-circle narrow phase is unchanged.
        const auto &first = vertices[edge.firstVertex], &second = vertices[edge.secondVertex];
        if (std::max(first.x, second.x) < left || std::min(first.x, second.x) > right ||
            std::max(first.y, second.y) < top || std::min(first.y, second.y) > bottom) { continue; }
        if (EdgeFraction(x, y, targetX - x, targetY - y, vertices[edge.firstVertex], vertices[edge.secondVertex], radius) < 1) {
            return false;
        }
    }
    return true;
}

bool CLevel::CanWalkTo(float x, float y, float targetX, float targetY) const {
    const float distance = std::hypot(targetX - x, targetY - y);
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / 4)));
    const float dx = (targetX - x) / steps, dy = (targetY - y) / steps;
    for (int step = 0; step < steps; ++step) {
        const float previousX = x, previousY = y;
        x += dx; y += dy;
        ResolveMovement(previousX, previousY, x, y, m_playerRadius);
    }
    return std::hypot(targetX - x, targetY - y) < 1;
}

void CLevel::ResolveBrotherForce(float previousX, float previousY, float &x, float &y) {
    ResolveMovement(previousX, previousY, x, y, m_playerRadius);
}

void CLevel::UpdateNavigation(ZCombatEnemy &actor, int deltaMs) {
    PerformanceProbe::Scope timing(PerformanceProbe::counters.navigationMs);
    CEnemy::CombatState &state = actor.model.enemy.combat;
    if (m_map == nullptr || state.behaviour != 0 || state.dead) {
        state.hasNavigationTarget = false;
        return;
    }
    actor.navigationTimer -= deltaMs;
    if (actor.navigationTimer > 0 && state.hasNavigationTarget &&
        std::hypot(state.navigationX - state.x, state.navigationY - state.y) > 10) { return; }
    actor.navigationTimer = 240;
    state.hasNavigationTarget = false;
    const float radius = actor.model.enemy.GetPart(0).radius * m_cameraScale;
    if (HasClearPath(state.x, state.y, state.targetX, state.targetY, radius)) { return; }
    ILayerPath *path = m_map->GetPathLayer(m_pathLayer);
    if (path == nullptr) { return; }
    const int start = path->FindNode(state.x, state.y);
    const int destination = path->FindNode(state.targetX, state.targetY);
    const auto &nodes = path->GetNodes();
    if (start < 0 || destination < 0) { return; }
    int next = path->FindNext(start, destination);
    if (next < 0) { return; }
    const int adjacent = next;
    // Skip centres only when the actual collision sweep has a clear corridor.
    for (int lookAhead = 0; lookAhead < 8 && next != destination; ++lookAhead) {
        const int farther = path->FindNext(next, destination);
        if (farther < 0 || farther == next ||
            !HasClearPath(state.x, state.y, nodes[farther].x, nodes[farther].y, radius)) { break; }
        next = farther;
    }
    state.hasNavigationTarget = true;
    state.navigationX = nodes[next].x;
    state.navigationY = nodes[next].y;
    if (next == adjacent && next != start) {
        path->GetConnectionPoint(start, next, state.navigationX, state.navigationY);
    }
}

void CLevel::Reset() {
    m_deathChoiceHandled[0] = 0; m_deathChoiceHandled[1] = 0;
    m_multiplayer[0] = {}; m_multiplayer[1] = {};
    m_localBotReviveRequested = false;
    m_reviveProgress = 0;
    m_reviveCount = 0;
    m_reviveTarget = 0;
    StopEffect(m_reviveEffectHandle);
    m_reviveEffectHandle = 0;
    m_reviveEffectState = 0;
    m_reviveEffectTarget = 0;
    m_peerIndicatorVisible = false;
    m_flockEnemies.clear();
    m_experienceTexts.clear();
    m_actor.ResetXplodiumRemainder();
    m_hasViewCenter = false;
    Clear();
    m_objects.Clear();
    m_vitals->Reset();
    m_playerModel->powerups = {};
    m_actor.GetTargetingController().Reset();
    if (m_brotherModel != nullptr) { m_brotherModel->powerups = {}; }
    ResetCombatProgress();
    if (m_playerModel->weapon != nullptr) {
        // Reset the script and gun state as well as health. The replacement
        // copies templates before retiring the old equipment.
        if (EquipPlayerWeapon(*m_tables, m_playerModel->weapon->playerScript,
            m_playerModel->ActiveWeapon().data, "arena reset", *m_playerModel)) {
            CreatePlayerBuffers(*m_playerModel, *m_program);
        }
    }
    m_actor.forceMs = 0;
    if (m_brotherModel != nullptr) {
        m_brother->Reset(m_actor.x, m_actor.y, m_actor.facing);
        if (EquipPlayerWeapon(*m_tables, m_brotherModel->weapon->playerScript,
            m_brotherModel->weapon->data, "brother reset", *m_brotherModel)) {
            CreatePlayerBuffers(*m_brotherModel, *m_program);
            m_brotherWeaponSlot = 0;
        }
    }
    m_actor.x = 600;
    m_actor.y = 650;
    m_actor.previousX = m_actor.x;
    m_actor.previousY = m_actor.y;
    m_playerModel->weapon->brother.SetLevelContext(GetScriptLevel());
    m_actor.facing = 0;
    damageDealt = 0;
    lastDamage = 0;
    hits = 0;
    kills = 0;
}

bool CLevel::CanBrotherWalk(float x, float y, float destinationX, float destinationY) const {
    if (destinationX < m_left || destinationX > m_right ||
        destinationY < m_top || destinationY > m_bottom) {
        return false;
    }
    const float distance = std::hypot(destinationX - x, destinationY - y);
    if (IsDeathmatch() && distance > 0 && distance <= 48 &&
        !HasClearPath(x, y, x, y, m_playerRadius)) {
        float resolvedX = destinationX;
        float resolvedY = destinationY;
        ResolveMovement(x, y, resolvedX, resolvedY, m_playerRadius);
        return std::hypot(resolvedX - destinationX, resolvedY - destinationY) < 0.1f;
    }
    return HasClearPath(x, y, destinationX, destinationY, m_playerRadius);
}

bool CLevel::PreloadEnemies(const RequirementList &requirements, const CScript &levelScript) {
    return m_objects.PreloadEnemies(requirements, levelScript);
}

ZCombatEnemy *CLevel::Spawn(std::size_t entry, float x, float y) {
    ZCombatEnemy *actor = m_objects.SpawnEnemy(entry, x, y);
    if (actor != nullptr) { SelectTarget(*actor); }
    return actor;
}

ZCombatEnemy *CLevel::SpawnNearby(std::size_t entry) {
    ZCombatEnemy *actor = m_objects.GetNearbyEnemy(entry, m_actor.x, m_actor.y);
    if (actor != nullptr) { SelectTarget(*actor); }
    return actor;
}

ZCombatEnemy *CLevel::Find(ZCombatId id) {
    return m_objects.FindEnemy(id);
}

const ZCombatEnemy *CLevel::Find(ZCombatId id) const {
    for (const auto &actor : m_objects.GetEnemies()) {
        if (actor->model.enemy.combat.id == id) { return actor.get(); }
    }
    return nullptr;
}

std::size_t CLevel::AliveCount() const {
    return m_objects.GetAliveEnemyCount();
}
