/** @file CEnemySpawner.h
 * @brief Original ten-rule enemy scheduler, with a world-owned spawn boundary.
 */
#ifndef GUN_BROS_RE_CENEMYSPAWNER_H
#define GUN_BROS_RE_CENEMYSPAWNER_H

#include "gun_bros/CGameAssetRef.h"
#include <array>
#include <vector>

class CLevel;
struct PlacedObject;

/** The level world decides which spawn nodes are free and owns the actors. */
class IEnemySpawnWorld {
public:
    virtual ~IEnemySpawnWorld() = default;
    virtual bool SpawnEnemy(const GameObjectRef &enemy, int layer, int node, int objectId) = 0;
    virtual int CountEnemies(const GameObjectRef *enemy = nullptr, int objectId = -1) const = 0;
    virtual bool SpawnMapObject(const PlacedObject &object, int objectId) { return false; }
    virtual void SendEnemyMessage(int objectId, int message) {}
    virtual void SendPropMessage(int objectId, int message) {}
    virtual void PlayLevelSound(const GameObjectRef &sound) {}
    virtual void OnWaveCleared(unsigned perfectRewardPercent) {}
    virtual bool SpawnPickup(const GameObjectRef &pickup, int layer, int node, int objectId, bool nearby) { return false; }
    virtual bool SpawnPickupAt(const GameObjectRef &pickup, float x, float y, int objectId) { return false; }
    virtual bool GetObjectPosition(int objectId, float &x, float &y) const { return false; }
    virtual bool GetIndicatorTarget(std::uint64_t key, float &x, float &y) const { return false; }
    virtual unsigned GetPowerupCount(unsigned localIndex) const { return 0; }
};

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

    void Bind(CLevel &level, IEnemySpawnWorld *world);
    void Reset();
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
    IEnemySpawnWorld *m_world = nullptr;
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
