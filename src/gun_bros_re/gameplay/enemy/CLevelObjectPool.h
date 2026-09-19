/** Original: src/gunbros/levelObjectPool.cpp constructor :145287, GetEnemy :145509, Release :145426.
 * Windows graphics/resource storage is adapted; original data comes from BIG.
 */
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

#include "gun_bros_re/gameplay/enemy/CEnemy.h"
#include "gun_bros_re/gameplay/pickup/CPickup.h"

#include <map>
#include <memory>
#include <vector>

class CLevel;

/** Owns the level's enemy instances and their allocation bookkeeping. */
// Pickups use a separate twenty-object limit, independent of particle effects.
class CLevelObjectPool {
public:
    CLevelObjectPool() = default;
    CLevelObjectPool(CGunBros &tables, const ZShaderProgram &program,
        const std::vector<CEnemy::Template> &catalog);
    void BindRuntime(CGunBros &tables, const ZShaderProgram &program,
        const std::vector<CEnemy::Template> &catalog);

    void SetLevel(CLevel *level) { m_level = level; }
    void SetUsesMapCoordinates(bool enabled) { m_usesMapCoordinates = enabled; }
    void Clear();
    /** GetPickup :145625 reserves one of twenty independent pickup slots. */
    CPickup *GetPickup();
    void ReleasePickup(std::size_t index);
    void ClearPickups();
    const std::vector<std::unique_ptr<CPickup>> &GetPickups() const { return m_pickups; }
    bool PreloadEnemies(const RequirementList &requirements, const CScript &levelScript);
    CEnemy *SpawnEnemy(std::size_t entry, float x, float y, bool forcePool = false);
    CEnemy *GetNearbyEnemy(std::size_t entry, float centerX, float centerY);
    CEnemy *FindEnemy(Collision::ObjectId id);
    std::size_t GetAliveEnemyCount() const;

    void QueueEnemy(const GameObjectRef &resource, float x, float y,
        int objectId, bool forcePool, Collision::ObjectId summoner);
    std::vector<CEnemy *> FinishEnemySpawns();
    Collision::ObjectId GetSummoner(Collision::ObjectId owner) const;
    void ReleaseEnemy(std::size_t index);

    std::vector<std::unique_ptr<CEnemy>> &GetEnemies() { return m_enemies; }
    const std::vector<std::unique_ptr<CEnemy>> &GetEnemies() const { return m_enemies; }
    const CEnemy::ResourceCache &GetEnemyModelCache() const { return m_enemyModelCache; }
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
        Collision::ObjectId summoner = 0;
    };

    CGunBros *m_tables = nullptr;
    const ZShaderProgram *m_program = nullptr;
    const std::vector<CEnemy::Template> *m_catalog = nullptr;
    CLevel *m_level = nullptr;
    bool m_usesMapCoordinates = false;
    CEnemy::ResourceCache m_enemyModelCache;
    std::vector<std::unique_ptr<CEnemy>> m_enemies;
    std::vector<std::unique_ptr<CPickup>> m_pickups;
    std::vector<PendingEnemy> m_pendingEnemies;
    std::map<Collision::ObjectId, Collision::ObjectId> m_summoners;
    Collision::ObjectId m_nextId = 2;
    unsigned m_spawnCount = 0;
    unsigned m_invalidSpawnCount = 0;
};

#endif
