/** @file CombatScene.h
 * @brief Shared actor world. The Arena harness supplies inputs and draws it.
 */
#ifndef GUN_BROS_RE_COMBATSCENE_H
#define GUN_BROS_RE_COMBATSCENE_H
#include "gun_bros/CTargetingController.h"

#include "milestones/EnemyModel.h"
#include "gun_bros/WeaponEffects.h"
#include "gun_bros/CMap.h"
#include "gun_bros/CPlayerProgress.h"
#include "gun_bros/CBrotherAI.h"
#include "runtime/IPropWorld.h"
#include <map>

constexpr float kArenaWidth = 1200;
constexpr float kArenaHeight = 900;
constexpr float kArenaPlayerCollisionRadius = 24;

struct CombatEnemy {
    const EnemyTemplateData *data = nullptr;
    EnemyModel model;
    int contactTimer = 0;
    int brotherContactTimer = 0;
    int corpseMs = 0;
    int objectId = -1;
    bool deathReported = false;
    int navigationTimer = 0;
};

struct CombatDeath {
    int objectId = -1;
    GameObjectRef enemy;
};

struct PickupSpawn {
    GameObjectRef resource;
    float x = 0;
    float y = 0;
};

/** The first level's unarmoured health, read from PLAYER_PROGRESS. */
bool LoadInitialPlayerHealth(CResTOCManager &toc, PackTables &tables, float &health);

class CombatScene : public IProjectileWorld, public IBrotherAIWorld {
public:
    CombatScene(PackTables &tables, const CShaderProgram &program,
        const std::vector<EnemyTemplateData> &catalog, PlayerModel &player,
        PlayerVitals &vitals, WeaponEffects &effects, float playerGameScale);
    void Reset();
    /** Map and geometry must outlive the scene; Arena needs no configuration. */
    void SetMap(CMap &map, const CCollisionData &collision, WeaponCollision &weaponCollision,
        float cameraScale, float playerRadius);
    CombatEnemy *Spawn(std::size_t entry, float x, float y);
    CombatEnemy *SpawnNearby(std::size_t entry);
    void Update(int deltaMs, float moveX, float moveY, bool shoot);
    void PlayerMatrix(float *matrix) const;
    void SetBrother(PlayerModel *model, CBrotherAI *brother);
    void SetBrotherWeapons(const CScript &script, const CGun::Template &pistol, const CGun::Template &rifle);
    bool SwapBrotherWeapon();
    unsigned GetBrotherWeaponSlot() const { return m_brotherWeaponSlot; }
    void ResetBrotherPosition(float x, float y);
    void BrotherMatrix(float *matrix) const;
    CombatId FindBrotherTarget(float x, float y, float radius) override;
    bool GetBrotherTarget(CombatId id, float &x, float &y) override;
    bool GetBrotherWaypoint(float x, float y, float targetX, float targetY,
        float &waypointX, float &waypointY) override;
    void ResolveBrotherForce(float previousX, float previousY, float &x, float &y) override;
    void EnemyMatrix(const CombatEnemy &enemy, float *matrix) const;
    /** Shared centres for the actual hit test and the collision overlay. */
    void EnemyCircle(const CombatEnemy &enemy, int part, float &x, float &y, float &radius) const;
    CombatEnemy *Find(CombatId id);
    std::size_t AliveCount() const;
    float GetPlayerRadius() const { return m_playerRadius; }
    void SetPathLayer(int index) { m_pathLayer = index; }
    void SetLevel(CLevel *level) { m_level = level; }
    void SetProps(IPropWorld *props) { m_props = props; }
    void SetPlayerProgress(CPlayerProgress *progress);
    void AddExperience(unsigned amount);
    void AddXplodium(unsigned amount);
    void AddHealth(unsigned amount);
    bool TouchesPickup(float x, float y) const;
    std::uint64_t GetXplodium() const { return m_xplodium; }
    void SetHorde(bool enabled) { m_horde = enabled; }
    unsigned GetScore() const { return m_score; }
    unsigned GetKillStreak() const { return m_killStreak; }
    void OnWaveCleared(unsigned perfectRewardPercent);
    std::uint64_t GetLastWaveBonus() const { return m_lastWaveBonus; }
    unsigned GetPerfectWaves() const { return m_perfectWaves; }
    unsigned GetClearedWaves() const { return m_clearedWaves; }
    CombatId GetAutoAimTarget() const { return m_autoAim.GetTarget(); }
    bool HasClearPath(float x, float y, float targetX, float targetY, float radius) const;
    /** Read-only movement simulation for a test driver escaping wall contact. */
    bool CanWalkTo(float x, float y, float targetX, float targetY) const;

    CombatTrace Trace(const CombatHit &hit, float x, float y, float dx, float dy,
        float radius, const std::vector<CombatId> &skipTargets) override;
    HitResult ApplyHit(CombatId target, const CombatHit &hit) override;
    float GetDamageMultiplier(CombatId owner, float fallback = 1) const override;
    float GetProjectilePowerupMultiplier(CombatId owner) const override;
    float GetEnemyTimeScale() const override;
    unsigned GetTotalKills() const;
    void SetViewCenter(float x, float y) { m_viewCenterX = x; m_viewCenterY = y; m_hasViewCenter = true; }
    float GetViewCenterX() const { if (m_hasViewCenter) { return m_viewCenterX; } return playerX; }
    float GetViewCenterY() const { if (m_hasViewCenter) { return m_viewCenterY; } return playerY; }
    void Splash(const CombatHit &hit, float radius, float coneDegrees,
        float force, int forceMs) override;
    void SpawnFromProjectile(const GameObjectRef &resource, const CombatHit &hit) override;
    bool FindTarget(const CombatHit &hit, float radius, float &x, float &y) override;
    bool Anchor(CombatId actor, int part, int node,
        float &x, float &y, float &z, float &direction) override;

    std::vector<std::unique_ptr<CombatEnemy>> enemies;
    std::vector<CombatDeath> deaths;
    std::vector<std::uint8_t> levelEvents;
    std::vector<PickupSpawn> pickupSpawns;
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
    CTargetingController m_autoAim;
    void RewardEnemy(const CombatEnemy &actor);
    CPlayerProgress *m_progress = nullptr;
    std::uint64_t m_xplodium = 0;
    unsigned m_xplodiumRemainder = 0;
    bool m_horde = false;
    unsigned m_score = 0;
    unsigned m_killStreak = 0;
    std::uint64_t m_waveXplodium = 0;
    std::uint64_t m_lastWaveBonus = 0;
    unsigned m_waveHits = 0;
    unsigned m_perfectWaves = 0;
    unsigned m_clearedWaves = 0;
    void Actions(CombatEnemy &actor);
    void SelectTarget(CombatEnemy &actor);
    void PartMatrix(const CombatEnemy &actor, int part, float *matrix) const;
    void FinishSpawns();
    struct PendingSpawn {
        std::size_t entry;
        float x;
        float y;
        int objectId = -1;
        bool forcePool = false; // Retained; this host has no fixed original enemy pool.
    };
    std::vector<PendingSpawn> m_pendingSpawns;
    PackTables &m_tables;
    const CShaderProgram &m_program;
    const std::vector<EnemyTemplateData> &m_catalog;
    PlayerModel &m_player;
    PlayerModel *m_brotherModel = nullptr;
    CBrotherAI *m_brother = nullptr;
    const CScript *m_brotherScript = nullptr;
    const CGun::Template *m_brotherWeapons[2]{};
    unsigned m_brotherWeaponSlot = 0;
    PlayerVitals &m_vitals;
    WeaponEffects &m_effects;
    float m_playerGameScale;
    CMap *m_map = nullptr;
    const CCollisionData *m_collision = nullptr;
    WeaponCollision *m_weaponCollision = nullptr;
    float m_cameraScale = 1;
    float m_viewCenterX = 0, m_viewCenterY = 0;
    bool m_hasViewCenter = false;
    float m_playerRadius = kArenaPlayerCollisionRadius;
    float m_left = 35;
    float m_top = 150;
    float m_right = kArenaWidth - 35;
    float m_bottom = kArenaHeight - 35;
    void ResolveMovement(float previousX, float previousY, float &x, float &y, float radius, bool player = true) const;
    void UpdateNavigation(CombatEnemy &actor, int deltaMs);
    int m_pathLayer = -1;
    CLevel *m_level = nullptr;
    IPropWorld *m_props = nullptr;
    CombatId m_nextId = 2;
    float m_playerForceX = 0;
    float m_playerForceY = 0;
    int m_playerForceMs = 0;
    float m_previousPlayerX = 600;
    float m_previousPlayerY = 650;
};
#endif
