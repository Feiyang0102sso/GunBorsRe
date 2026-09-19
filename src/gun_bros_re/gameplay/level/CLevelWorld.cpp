/** @file CLevelWorld.cpp
 * @brief CLevel map collision, navigation and enemy object access.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/collision/Collision.h"
#include "gun_bros_re/debug/PerformanceProbe.h"
#include <algorithm>
#include <cmath>

namespace {
using Collision::EdgeFraction;
}

void CLevel::SetMap(CMap &map, const CCollisionData &collision, CCollisionData::Scene &weaponCollision,
    float cameraScale, float playerRadius) {
    m_map = &map;
    m_objects.SetUsesMapCoordinates(true);
    m_collision = &collision;
    m_weaponCollision = &weaponCollision;
    m_cameraScale = cameraScale;
    m_playerRadius = playerRadius;
    m_actor.BindLevel(map, collision, m_objects, playerRadius);
    const CLayerCamera::Rectangle bounds = map.GetCameraExtent();
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
    m_flock.Clear();
    m_experienceTexts.clear();
    m_actor.ResetXplodiumRemainder();
    m_hasViewCenter = false;
    Clear();
    ResetPickups();
    m_objects.Clear();
    m_vitals->Reset();
    m_playerModel->powerups = {};
    m_actor.GetTargetingController().Reset();
    if (m_brotherModel != nullptr) { m_brotherModel->powerups = {}; }
    ResetCombatProgress();
    if (m_playerModel->weapon != nullptr) {
        // Reset the script and gun state as well as health. The replacement
        // copies templates before retiring the old equipment.
        if (m_playerModel->EquipWeapon(*m_tables, m_playerModel->GetScript(), *m_playerModel->ActiveWeapon().GetTemplate(), "arena reset")) {
            m_playerModel->CreateBuffers(*m_program);
        }
    }
    m_actor.forceMs = 0;
    if (m_brotherModel != nullptr) {
        m_brother->Reset(m_actor.x, m_actor.y, m_actor.facing);
        if (m_brotherModel->EquipWeapon(*m_tables, m_brotherModel->GetScript(), *m_brotherModel->weapon->GetTemplate(), "brother reset")) {
            m_brotherModel->CreateBuffers(*m_program);
            m_brotherWeaponSlot = 0;
        }
    }
    m_actor.x = 600;
    m_actor.y = 650;
    m_actor.previousX = m_actor.x;
    m_actor.previousY = m_actor.y;
    m_playerModel->SetLevelContext(GetScriptLevel());
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
