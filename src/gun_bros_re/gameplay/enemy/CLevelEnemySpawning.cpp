/** @file CLevelEnemySpawning.cpp
 * Original: src/gunbros/level.cpp OnEnemyKilled :119306, OnEnemyTeleport :118252,
 * SpawnEnemy and levelObjectPool.cpp GetEnemy :145509. CLevel implementation.
 */
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/data/CFriendPowerManager.h"
#include "engine/core/ZMatrix4d.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

bool CLevel::SpawnEnemy(const GameObjectRef &enemy, int layerIndex, int nodeIndex, int objectId) {
    if (m_playerModel == nullptr || m_map == nullptr || m_catalog == nullptr) { return false; }
    const bool hasAuthoredRoute = layerIndex >= 0 && nodeIndex >= 0;
    std::size_t entryIndex = 0;
    while (entryIndex < m_catalog->size()) {
        const CEnemy::Template &entry = (*m_catalog)[entryIndex];
        if (entry.packHash == enemy.packHash && entry.ordinal == enemy.localIndex) { break; }
        ++entryIndex;
    }
    if (entryIndex == m_catalog->size()) { return false; }
    if (layerIndex < 0) { layerIndex = m_pathLayer; }
    ILayerPath *path = m_map->GetPathLayer(layerIndex);
    if (path == nullptr || path->GetNodes().empty()) { return false; }
    const auto &nodes = path->GetNodes();
    if (nodeIndex < 0) {
        nodeIndex = m_spawner.GetSpawnPoint(*path, GetPlayer().x, GetPlayer().y,
            m_cameraLeft, m_cameraTop, m_cameraWidth, m_cameraHeight);
    }
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) { return false; }
    CEnemy *actor = Spawn(entryIndex, nodes[nodeIndex].x, nodes[nodeIndex].y);
    if (actor == nullptr) { return false; }
    if (objectId < 0) {
        const float distance = std::hypot(GetPlayer().x - nodes[nodeIndex].x,
            GetPlayer().y - nodes[nodeIndex].y);
        if (m_closestSpawnDistance < 0 || distance < m_closestSpawnDistance) {
            m_closestSpawnDistance = distance;
        }
        if (m_cameraWidth > 0 && nodes[nodeIndex].x >= m_cameraLeft && nodes[nodeIndex].y >= m_cameraTop &&
            nodes[nodeIndex].x <= m_cameraLeft + m_cameraWidth && nodes[nodeIndex].y <= m_cameraTop + m_cameraHeight) {
            ++m_onScreenSpawns;
        }
    }
    actor->objectId = objectId;
    if (hasAuthoredRoute) { actor->SetPath(path); }
    SetIndicator(objectId, 0, actor->combat.id);
    return true;
}

void CLevel::SendEnemyMessage(int objectId, int message) {
    if (m_playerModel == nullptr) { return; }
    for (const auto &actor : GetEnemies()) {
        if (actor->objectId != objectId) { continue; }
        const unsigned before = actor->GetStateId();
        actor->HandleMessage(message);
        if (before != actor->GetStateId()) {
            std::printf("[map-enemy] id=%d message=%d state=%u->%u\n",
                objectId, message, before, actor->GetStateId());
        }
        return;
    }
}

void CLevel::SetEnemyPortal(int enemyId, int propId) {
    if (m_playerModel == nullptr) { return; }
    for (const auto &actor : GetEnemies()) {
        if (actor->objectId != enemyId) { continue; }
        actor->combat.portalObjectId = propId;
        actor->combat.portalActive = false;
        return;
    }
}

int CLevel::CountEnemySlots(const GameObjectRef *enemy) const {
    if (m_playerModel == nullptr) { return 0; }
    int count = 0;
    for (const auto &actor : GetEnemies()) {
        if (actor->combat.removed) { continue; }
        if (enemy == nullptr || (actor->data->packHash == enemy->packHash && actor->data->ordinal == enemy->localIndex)) {
            ++count;
        }
    }
    return count;
}

int CLevel::CountEnemies(const GameObjectRef *enemy, int objectId) const {
    if (m_playerModel == nullptr) { return 0; }
    int count = 0;
    for (const auto &actor : GetEnemies()) {
        const CEnemy::CombatState &state = actor->combat;
        if (state.dead || state.removed) { continue; }
        if (objectId >= 0 && actor->objectId != objectId) { continue; }
        if (enemy == nullptr || (actor->data->packHash == enemy->packHash && actor->data->ordinal == enemy->localIndex)) {
            ++count;
        }
    }
    return count;
}
