/**
 * @file CLevelObjectPool.h
 * @brief Runtime storage and allocation for level objects.
 *
 * Restores the ownership boundary of CLevelObjectPool from level.cpp.
 * Reference: CLevelObjectPool::CLevelObjectPool :145287,
 * CLevelObjectPool::GetEnemy :145509, and CLevelObjectPool::Clear :145701.
 */
#ifndef GUN_BROS_RE_CLEVELOBJECTPOOL_H
#define GUN_BROS_RE_CLEVELOBJECTPOOL_H

#include "gun_bros_re/gameplay/ZEnemyModel.h"

#include <map>
#include <memory>
#include <vector>

class CLevel;

struct ZCombatEnemy {
    const ZEnemyTemplateData *data = nullptr;
    ZEnemyModel model;
    int contactTimer = 0;
    int brotherContactTimer = 0;
    int corpseMs = 0;
    int objectId = -1;
    bool mapPlaced = false; // Map mechanisms are not dynamic wave enemies.
    bool deathReported = false;
    int navigationTimer = 0;
    unsigned assistMask[2]{}; // Each peer's original two gun configuration bits.
};

/** Owns the level's enemy instances and their allocation bookkeeping. */
class CLevelObjectPool {
public:
    CLevelObjectPool() = default;
    CLevelObjectPool(ZPackTables &tables, const ZShaderProgram &program,
        const std::vector<ZEnemyTemplateData> &catalog);
    void BindRuntime(ZPackTables &tables, const ZShaderProgram &program,
        const std::vector<ZEnemyTemplateData> &catalog);

    void SetLevel(CLevel *level) { m_level = level; }
    void SetUsesMapCoordinates(bool enabled) { m_usesMapCoordinates = enabled; }
    void Clear();
    bool PreloadEnemies(const RequirementList &requirements, const CScript &levelScript);
    ZCombatEnemy *SpawnEnemy(std::size_t entry, float x, float y, bool forcePool = false);
    ZCombatEnemy *GetNearbyEnemy(std::size_t entry, float centerX, float centerY);
    ZCombatEnemy *FindEnemy(ZCombatId id);
    std::size_t GetAliveEnemyCount() const;

    void QueueEnemy(const GameObjectRef &resource, float x, float y,
        int objectId, bool forcePool, ZCombatId summoner);
    std::vector<ZCombatEnemy *> FinishEnemySpawns();
    ZCombatId GetSummoner(ZCombatId owner) const;
    void ReleaseEnemy(std::size_t index);

    std::vector<std::unique_ptr<ZCombatEnemy>> &GetEnemies() { return m_enemies; }
    const std::vector<std::unique_ptr<ZCombatEnemy>> &GetEnemies() const { return m_enemies; }
    const ZEnemyModelCache &GetEnemyModelCache() const { return m_enemyModelCache; }
    unsigned GetSpawnCount() const { return m_spawnCount; }
    unsigned GetInvalidSpawnCount() const { return m_invalidSpawnCount; }
    void RecordInvalidSpawn() { ++m_invalidSpawnCount; }

private:
    struct PendingEnemy {
        std::size_t entry = 0;
        float x = 0;
        float y = 0;
        int objectId = -1;
        bool forcePool = false;
        ZCombatId summoner = 0;
    };

    ZPackTables *m_tables = nullptr;
    const ZShaderProgram *m_program = nullptr;
    const std::vector<ZEnemyTemplateData> *m_catalog = nullptr;
    CLevel *m_level = nullptr;
    bool m_usesMapCoordinates = false;
    ZEnemyModelCache m_enemyModelCache;
    std::vector<std::unique_ptr<ZCombatEnemy>> m_enemies;
    std::vector<PendingEnemy> m_pendingEnemies;
    std::map<ZCombatId, ZCombatId> m_summoners;
    ZCombatId m_nextId = 2;
    unsigned m_spawnCount = 0;
    unsigned m_invalidSpawnCount = 0;
};

#endif
