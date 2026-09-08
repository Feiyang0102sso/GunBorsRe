/** @file CombatScene.h
 * @brief Shared actor world. The Arena harness supplies inputs and draws it.
 */
#ifndef GUN_BROS_RE_COMBATSCENE_H
#define GUN_BROS_RE_COMBATSCENE_H

#include "milestones/EnemyModel.h"
#include "gun_bros/WeaponEffects.h"
#include <map>

constexpr float kArenaWidth = 1200;
constexpr float kArenaHeight = 900;
constexpr float kPlayerCollisionRadius = 24;

struct CombatEnemy {
    const EnemyTemplateData *data = nullptr;
    EnemyModel model;
    int contactTimer = 0;
    int corpseMs = 0;
};

/** The first level's unarmoured health, read from PLAYER_PROGRESS. */
bool LoadInitialPlayerHealth(CResTOCManager &toc, PackTables &tables, float &health);

class CombatScene : public IProjectileWorld {
public:
    CombatScene(PackTables &tables, const CShaderProgram &program,
        const std::vector<EnemyTemplateData> &catalog, PlayerModel &player,
        PlayerVitals &vitals, WeaponEffects &effects, float playerGameScale);
    void Reset();
    CombatEnemy *Spawn(std::size_t entry, float x, float y);
    CombatEnemy *SpawnNearby(std::size_t entry);
    void Update(int deltaMs, float moveX, float moveY, bool shoot);
    void PlayerMatrix(float *matrix) const;
    void EnemyMatrix(const CombatEnemy &enemy, float *matrix) const;
    /** Shared centres for the actual hit test and the collision overlay. */
    void EnemyCircle(const CombatEnemy &enemy, int part, float &x, float &y, float &radius) const;
    CombatEnemy *Find(CombatId id);
    std::size_t AliveCount() const;

    CombatTrace Trace(const CombatHit &hit, float x, float y, float dx, float dy,
        float radius, const std::vector<CombatId> &skipTargets) override;
    HitResult ApplyHit(CombatId target, const CombatHit &hit) override;
    void Splash(const CombatHit &hit, float radius, float coneDegrees,
        float force, int forceMs) override;
    void SpawnFromProjectile(const GameObjectRef &resource, const CombatHit &hit) override;
    bool FindTarget(const CombatHit &hit, float radius, float &x, float &y) override;
    bool Anchor(CombatId actor, int part, int node,
        float &x, float &y, float &z, float &direction) override;

    std::vector<std::unique_ptr<CombatEnemy>> enemies;
    float playerX = 600;
    float playerY = 650;
    float facing = 0;
    float damageDealt = 0;
    float lastDamage = 0;
    unsigned hits = 0;
    unsigned kills = 0;
    unsigned spawned = 0;
    unsigned invalidSpawns = 0;

private:
    void Actions(CombatEnemy &actor);
    void SelectTarget(CombatEnemy &actor);
    void PartMatrix(const CombatEnemy &actor, int part, float *matrix) const;
    void FinishSpawns();
    struct PendingSpawn { std::size_t entry; float x; float y; };
    std::vector<PendingSpawn> m_pendingSpawns;
    PackTables &m_tables;
    const CShaderProgram &m_program;
    const std::vector<EnemyTemplateData> &m_catalog;
    PlayerModel &m_player;
    PlayerVitals &m_vitals;
    WeaponEffects &m_effects;
    float m_playerGameScale;
    CombatId m_nextId = 2;
    float m_playerForceX = 0;
    float m_playerForceY = 0;
    int m_playerForceMs = 0;
    float m_previousPlayerX = 600;
    float m_previousPlayerY = 650;
};
#endif
