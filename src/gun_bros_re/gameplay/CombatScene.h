#include "gun_bros_re/data/CChallengeManager.h"
/** @file CombatScene.h
 * @brief Shared actor world. The Arena harness supplies inputs and draws it.
 */
#ifndef GUN_BROS_RE_COMBATSCENE_H
#define GUN_BROS_RE_COMBATSCENE_H
#include "gun_bros_re/gameplay/CTargetingController.h"

#include "gun_bros_re/gameplay/EnemyModel.h"
#include "gun_bros_re/gameplay/WeaponEffects.h"
#include "gun_bros_re/gameplay/CMap.h"
#include "gun_bros_re/data/CPlayerProgress.h"
#include "gun_bros_re/gameplay/CBrotherAI.h"
#include "gun_bros_re/gameplay/IPropWorld.h"
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
    bool mapPlaced = false; // Map mechanisms are not dynamic wave enemies.
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
    /** Original map requirements and LEVEL script references, before simulation. */
    bool PreloadEnemies(const RequirementList &requirements, const CScript &levelScript);
    CombatEnemy *SpawnNearby(std::size_t entry);
    void Update(int deltaMs, float moveX, float moveY, bool shoot);
    void PlayerMatrix(float *matrix) const;
    void SetBrother(PlayerModel *model, CBrotherAI *brother);
    void SetBrotherWeapons(const CScript &script, const CGun::Template &pistol, const CGun::Template &rifle);
    /** AI and desktop cheats enter the original swap script before changing guns. */
    bool RequestBrotherWeaponSwap();
    bool SwapBrotherWeapon();
    unsigned GetBrotherWeaponSlot() const { return m_brotherWeaponSlot; }
    /** facingDegrees comes from the map PLAYER object, as the player's does. */
    void ResetBrotherPosition(float x, float y, float facingDegrees);
    void BrotherMatrix(float *matrix) const;
    CombatId FindBrotherTarget(float x, float y, float radius) override;
    bool GetBrotherTarget(CombatId id, float &x, float &y) override;
    bool GetBrotherWaypoint(float x, float y, float targetX, float targetY,
        float &waypointX, float &waypointY) override;
    void ResolveBrotherForce(float previousX, float previousY, float &x, float &y) override;
    void EnemyMatrix(const CombatEnemy &enemy, float *matrix) const;
    struct MovementBounds { float left, top, right, bottom; };
    /** Inclusive player-center limits used by ResolveMovement. */
    MovementBounds GetPlayerMovementBounds() const { return {m_left, m_top, m_right, m_bottom}; }
    /** Shared centres for the actual hit test and the collision overlay. */
    void EnemyCircle(const CombatEnemy &enemy, int part, float &x, float &y, float &radius) const;
    struct HealthBar {
        float x, y, width, height, border, fraction, red;
    };
    /** BIG bounds give world top-centre x/y; width/height/border are screen pixels.
     * The HUD projection converts this anchor to a screen top-left rectangle.
     */
    std::vector<HealthBar> EnemyHealthBars(float viewportScale = 1) const;
    /** CEffectLayer::TextEffect, captured in screen space on a real death.
     * The HUD resolves the number's original STR template and bitmap font. */
    struct ExperienceText {
        unsigned amount = 0;
        float x = 0, y = 0, alpha = 1;
        unsigned elapsedMs = 0;
    };
    const std::vector<ExperienceText> &GetExperienceTexts() const { return m_experienceTexts; }
    void UpdateExperienceTexts(int deltaMs);
    /** Windows camera projection into the HUD's 1024 x 768 logical surface. */
    void SetTextView(float left, float top, float scaleX, float scaleY) {
        m_textViewX = left; m_textViewY = top;
        m_textScaleX = scaleX; m_textScaleY = scaleY;
    }
    CombatEnemy *Find(CombatId id);
    std::size_t AliveCount() const;
    float GetPlayerRadius() const { return m_playerRadius; }
    void SetPathLayer(int index) { m_pathLayer = index; }
    void SetLevel(CLevel *level) { m_level = level; }
    CLevel *GetLevel() const { return m_level; }
    PlayerVitals &GetPlayerVitals() { return m_vitals; }
    /** Windows cheat enters the same BIG death export as a fatal hit. */
    bool Suicide();
    PlayerVitals *GetBrotherVitals() {
        if (m_brother != nullptr) { return &m_brother->vitals; }
        return nullptr;
    }
    CLevel *GetScriptLevel() override { return m_level; }
    void SetProps(IPropWorld *props) { m_props = props; }
    void SetPlayerProgress(CPlayerProgress *progress);
    void AddExperience(unsigned amount);
    std::uint64_t GetExperience() const { if (m_progress) { return m_progress->GetExperience(); } return 0; }
    void AddXplodium(unsigned amount);
    void AddHealth(unsigned amount);
    bool TouchesPickup(float x, float y) const;
    std::uint64_t GetXplodium() const { return m_xplodium; }
    void SetHorde(bool enabled) { m_horde = enabled; }
    unsigned GetScore() const { return m_score; }
    unsigned GetKillStreak() const { return m_killStreak; }
    unsigned GetBestKillStreak() const { return m_bestKillStreak; }
    void OnWaveCleared(unsigned perfectRewardPercent);
    std::uint64_t GetLastWaveBonus() const { return m_lastWaveBonus; }
    unsigned GetPerfectWaves() const { return m_perfectWaves; }
    unsigned GetClearedWaves() const { return m_clearedWaves; }
    const std::vector<bool> &GetWavePerfectResults() const { return m_wavePerfectResults; }
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
    const EnemyModelCache &GetEnemyModelCache() const { return m_enemyModelCache; }
    const std::vector<WeaponCombatProgress> &GetWeaponProgress() const { return m_weaponProgress; }
    std::vector<CChallengeManager::Kill> TakeChallengeKills() { auto result = std::move(m_challengeKills); m_challengeKills.clear(); return result; }
    std::vector<GameObjectRef> TakeChallengePowerups() { auto result = std::move(m_challengePowerups); m_challengePowerups.clear(); return result; }
    void RecordChallengePowerup(const GameObjectRef &ref) { m_challengePowerups.push_back(ref); }
    const std::vector<EnemyCasualty> &GetCasualties() const { return m_casualties; }
    void SetViewCenter(float x, float y) { m_viewCenterX = x; m_viewCenterY = y; m_hasViewCenter = true; }
    /** The camera rectangle projectile culling tests against; see CBullet::CanBeCulled :60583. */
    void SetViewSize(float width, float height) { m_effects.SetViewBounds(GetViewCenterX(), GetViewCenterY(), width, height); }
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
    struct Teleport { int objectId; GameObjectRef enemy; };
    std::vector<Teleport> teleports;
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
    std::vector<EnemyCombat *> m_flockEnemies;
    EnemyModelCache m_enemyModelCache;
    CTargetingController m_autoAim;
    void RewardEnemy(const CombatEnemy &actor);
    std::vector<WeaponCombatProgress> m_weaponProgress;
    std::vector<EnemyCasualty> m_casualties;
    std::vector<CChallengeManager::Kill> m_challengeKills;
    std::vector<GameObjectRef> m_challengePowerups;
    std::vector<ExperienceText> m_experienceTexts;
    float m_textViewX = 0, m_textViewY = 0, m_textScaleX = 1, m_textScaleY = 1;
    CPlayerProgress *m_progress = nullptr;
    std::uint64_t m_xplodium = 0;
    unsigned m_xplodiumRemainder = 0;
    bool m_horde = false;
    unsigned m_score = 0;
    unsigned m_killStreak = 0;
    unsigned m_bestKillStreak = 0;
    std::uint64_t m_waveXplodium = 0;
    std::uint64_t m_lastWaveBonus = 0;
    unsigned m_waveHits = 0;
    unsigned m_perfectWaves = 0;
    unsigned m_clearedWaves = 0;
    std::vector<bool> m_wavePerfectResults;
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
    void ResolvePlayerMovement(float previousX, float previousY, float &x, float &y) const;
    void UpdateNavigation(CombatEnemy &actor, int deltaMs);
    int m_pathLayer = -1;
    CLevel *m_level = nullptr;
    IPropWorld *m_props = nullptr;
    CombatId m_nextId = 2;
    float m_playerForceX = 0;
    float m_playerForceY = 0;
    int m_playerForceMs = 0;
    void ApplyBrotherForce(CombatId target, float x, float y, int durationMs);
    float m_previousPlayerX = 600;
    float m_previousPlayerY = 650;
};
#endif
