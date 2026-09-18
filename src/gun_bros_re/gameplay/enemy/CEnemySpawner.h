/** Original: src/gunbros/enemySpawner.cpp CEnemySpawner :146035, IEnemySpawnerScriptInterface :146664.
 * Windows graphics/resource storage is adapted; original data comes from BIG.
 */
/** @file CEnemySpawner.h
 * @brief Original ten-rule enemy scheduler, with a world-owned spawn boundary.
 */
#ifndef GUN_BROS_RE_CENEMYSPAWNER_H
#define GUN_BROS_RE_CENEMYSPAWNER_H

#include "gun_bros_re/data/CGameAssetRef.h"
#include <array>
#include <vector>

class CLevel;
class ILayerPath;
#include "gun_bros_re/gameplay/map/CLayerObject.h"

#include "gun_bros_re/gameplay/enemy/CEnemyWorld.h"

// TODO(network): restore CNetworkEnemySpawner, SpawnPacket and remote enemy
// synchronization from enemySpawner.cpp :147304 and enemy.cpp ProcessNetworkStatePacket / ProcessNetworkEventPacket.
// Local cooperative/PvP Bots use CLevel directly and do not require this path.
class CEnemySpawner {
public:
    struct Rule {
        bool enabled = false;
        int maximum = -1;
        int intervalMs = 0;
        int elapsedMs = 0;
        int remaining = 0; // Zero means unlimited; positive values count down.
        int layer = -1;
        int resource = -1;
    };

    void Bind(CLevel &level, CEnemyWorld *world);
    void Reset();
    /** Select explicit nodes or delegate offscreen placement to the path layer. */
    int GetSpawnPoint(const ILayerPath &path, float sourceX, float sourceY,
        float cameraLeft, float cameraTop, float cameraWidth, float cameraHeight);
    void Update(int deltaMs);
    std::int16_t FunctionResolver(std::uint8_t function, const std::int16_t *arguments,
        std::uint8_t argumentCount);
    const std::array<Rule, 10> &GetRules() const { return m_rules; }
    unsigned GetSpawnCount() const { return m_spawnCount; }
    unsigned GetUnsupportedCount() const { return m_unsupported; }
    int GetSpawnLayer() const { return m_layer; }
    const std::vector<int> &GetEnabledNodes() const { return m_enabledNodes; }
    bool AllNodesEnabled() const { return m_allNodes; }

private:
    bool Spawn(int resource, int layer, int node, int objectId);
    CLevel *m_level = nullptr;
    CEnemyWorld *m_world = nullptr;
    std::array<Rule, 10> m_rules;
    bool m_paused = false;
    bool m_allNodes = true;
    int m_maximum = 0;
    int m_layer = -1;
    unsigned m_spawnCount = 0;
    unsigned m_unsupported = 0;
    std::vector<int> m_enabledNodes;
};

#endif
