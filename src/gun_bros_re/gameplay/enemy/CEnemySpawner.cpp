/** Original: src/gunbros/enemySpawner.cpp CEnemySpawner :146035, IEnemySpawnerScriptInterface :146664.
 * Windows graphics/resource storage is adapted; original data comes from BIG.
 */
/** @file CEnemySpawner.cpp
 * @brief CEnemySpawner rules from iOS :146174-146945 and spawner.link.
 */
#include "gun_bros_re/gameplay/ILayerPath.h"
#include "gun_bros_re/gameplay/enemy/CEnemySpawner.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include <cstdio>

void CEnemySpawner::Bind(CLevel &level, CEnemyWorld *world) {
    m_level = &level;
    m_world = world;
    m_spawnCount = 0;
    m_unsupported = 0;
    Reset();
}

void CEnemySpawner::Reset() {
    for (Rule &rule : m_rules) {
        rule = Rule();
    }
    m_paused = false;
    m_maximum = 0;
    m_layer = -1;
    m_allNodes = true;
    m_enabledNodes.clear();
}

bool CEnemySpawner::Spawn(int resource, int layer, int node, int objectId) {
    GameObjectRef enemy;
    if (m_level == nullptr || m_world == nullptr || !m_level->GetResource(resource, enemy)) {
        return false;
    }
    // GetNumFreeEnemies/GetEnemy :147230/:145509 count allocated pool slots.
    if (m_world->CountEnemySlots() >= static_cast<int>(m_level->GetEnemyLimit())) { return false; }
    if (layer < 0) {
        layer = m_layer;
    }
    if (!m_world->SpawnEnemy(enemy, layer, node, objectId)) {
        return false;
    }
    // Script-tracked spawns are the ones a level later addresses by object id
    // -- the pack12 babe among them. Worth seeing which template and node the
    // script rolled.
    if (objectId >= 0) {
        std::printf("[spawner] tracked spawn resource=%d object=%08x:%u layer=%d node=%d id=%d\n",
            resource, enemy.packHash, enemy.localIndex, layer, node, objectId);
    }
    ++m_spawnCount;
    return true;
}

void CEnemySpawner::Update(int deltaMs) {
    if (m_paused || m_world == nullptr || deltaMs <= 0) {
        return;
    }
    if (m_maximum > 0 && m_world->CountEnemies() >= m_maximum) {
        return;
    }
    for (Rule &rule : m_rules) {
        if (!rule.enabled) {
            continue;
        }
        rule.elapsedMs += deltaMs;
        // Original UpdateRule allows at most two spawns to catch up per tick.
        for (int attempt = 0; attempt < 2 && rule.elapsedMs >= rule.intervalMs; ++attempt) {
            GameObjectRef enemy;
            if (!m_level->GetResource(rule.resource, enemy)) {
                break;
            }
            // The resource-specific GetEnemyCount :146595 visits unremoved
            // objects, unlike the global living-enemy maximum above.
            if (rule.maximum >= 0 && m_world->CountEnemySlots(&enemy) >= rule.maximum) {
                break;
            }
            if (!Spawn(rule.resource, rule.layer, -1, -1)) {
                break;
            }
            if (rule.remaining > 0) {
                --rule.remaining;
                if (rule.remaining == 0) {
                    rule.enabled = false;
                    break;
                }
            }
            if (rule.intervalMs <= 0) {
                break;
            }
            rule.elapsedMs -= rule.intervalMs;
        }
    }
}


/**
 * Where a rule-driven spawn appears.
 *
 * CEnemySpawner::GetSpawnPoint :146098 picks between two rules. With an
 * explicit node list (DisableAllNodes + EnableNode) it is
 * GetSpawnPointSpecific :146576: one of the listed nodes, uniformly at random,
 * with no other test. Otherwise it is GetSpawnPointOffScreen :146112, which
 * hands CLayerPathLink::GetSpawnLocation :166819 the player's position
 * (GetSpawnSource :147224) and an offscreen filter built from the camera
 * rectangle grown by ten units on each side (SetupSpawnFilter :147283).
 *
 * GetSpawnLocation walks every node, drops the locked ones and the ones the
 * filter rejects for being on screen, and feeds the rest to a DistanceList
 * :167261 that keeps the five nearest to the player. One of those five is then
 * chosen at random -- so enemies arrive from just outside the view, never in
 * the player's face, and never from the far side of the map.
 *
 * No node qualifying is an ordinary outcome: the original spawns nothing that
 * tick and the rule tries again on the next one.
 */
int CEnemySpawner::GetSpawnPoint(const ILayerPath &path, float sourceX, float sourceY,
    float cameraLeft, float cameraTop, float cameraWidth, float cameraHeight) {
    if (!m_allNodes) {
        if (m_enabledNodes.empty()) { return -1; }
        const int pick = m_level->RandomInteger(0, static_cast<std::int16_t>(m_enabledNodes.size() - 1));
        return m_enabledNodes[pick];
    }
    // CEnemySpawner::SetupSpawnFilter :147283 expands the camera by ten units.
    const COffscreenSpawnLocationFilter filter{cameraLeft - 10, cameraTop - 10,
        cameraLeft + cameraWidth + 10, cameraTop + cameraHeight + 10,
        cameraWidth > 0 && cameraHeight > 0};
    return path.GetSpawnLocation(sourceX, sourceY, filter, m_level->GetRandom());
}
