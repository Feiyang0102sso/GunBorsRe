#include "gun_bros_re/gameplay/ZGameScriptObject.h"
/**
 * @file CLevel.h
 * @brief A level: a map plus the script that drives it.
 *
 * Port of CLevel (src/gunbros/level.cpp), including the active world update.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:114771 (Template::Init),
 *            :114371 (VariableResolver), :117365 (FunctionResolver),
 *            :120988 (where CLevel binds its script and calls export 0)
 *
 * Most native functions are still unimplemented, and
 * calling one logs its id and arguments rather than failing. That log is the
 * list of what the levels in these archives actually ask for, which is how the
 * rest of the resolver should be prioritised.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CLEVEL_H
#define GUN_BROS_RE_GUN_BROS_CLEVEL_H

#include "engine/resources/CArrayInputStream.h"
#include "engine/glu/script/CScript.h"
#include "engine/glu/script/CScriptInterpreter.h"
#include "engine/glu/script/ScriptResolver.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include "gun_bros_re/gameplay/enemy/CEnemySpawner.h"
#include "gun_bros_re/gameplay/level/CLevelIndicator.h"
#include "gun_bros_re/gameplay/enemy/CLevelObjectPool.h"
#include "gun_bros_re/gameplay/map/CMap.h"
#include "gun_bros_re/gameplay/enemy/CFlock.h"
#include "gun_bros_re/gameplay/brother/CPlayer.h"
#include "gun_bros_re/gameplay/brother/CBrotherAI.h"
#include "gun_bros_re/gameplay/ZCombatTypes.h"
#include "gun_bros_re/gameplay/ZMultiplayerStatistics.h"
#include "gun_bros_re/gameplay/ZProjectileTypes.h"
#include "gun_bros_re/gameplay/ZBulletResources.h"
#include "gun_bros_re/effects/ZParticleResources.h"
#include "gun_bros_re/gameplay/ZCombatAudio.h"
#include "gun_bros_re/effects/ZEffectColors.h"
#include "gun_bros_re/effects/CEffectLayer.h"
#include "gun_bros_re/effects/CParticleSystem.h"
#include "engine/glu/sprite/ZSpriteRenderer.h"
#include "gun_bros_re/data/CChallengeManager.h"

#include <cmath>
#include <cstdint>
#include <string>

class CGame;
class CMap;
class CMPMatch;
class CProfileManager;
class CPowerup;
class CPowerUpSelector;
struct ZPowerupEntry;

constexpr float kArenaWidth = 1200;
constexpr float kArenaHeight = 900;
constexpr float kArenaPlayerCollisionRadius = 24;

/** The first level's unarmoured health, read from PLAYER_PROGRESS. */
bool LoadInitialPlayerHealth(CResTOCManager &toc, ZPackTables &tables, float &health);

// The CLevel functions implemented so far, all of which only touch the map.
// Reference: :117497 (setCameraLayer), :117508 (setCollisionLayer),
//            :117823 (setTileLayerSpeed)
constexpr std::uint8_t kLevelFunctionSetCameraLayer = 1;
constexpr std::uint8_t kLevelFunctionSetCollisionLayer = 2;
constexpr std::uint8_t kLevelFunctionSetTileLayerSpeed = 42;

// Both speed arguments are fixed-point, and the resolver divides by this
// before passing them to CLayerTile::SetSpeed. Reference: :117827
constexpr float kLevelSpeedArgumentUnit = 1.0f / 256.0f;

// The export the engine calls once the map is bound. Reference: :121003
constexpr std::uint8_t kLevelExportOnLevelStart = 0;

// How many variables CLevel::VariableResolver answers for. Reference: :114371
constexpr std::uint32_t kLevelVariableCount = 8;

/** A level and the script it runs. */
class CLevel : public ZGameScriptObject, private CEnemyWorld,
    public ZProjectileWorld, public ZBrotherAIWorld {
public:
    // Internal implementation of the original level-owned prop pool.
    class Props;

    bool IsManualSpawnTag(unsigned char tag) const { return m_manualSpawnTags[tag]; }
    /**
     * What a LEVEL resource holds.
     *
     * Wire format:
     *   GameObjectRef mapRef
     *   CScript       script
     *   uint16        unknown[3]
     */
    struct Template {
        GameObjectRef mapRef;
        CScript script;

        // Fields 92, 94 and 96 of the template. Read to keep the stream in
        // step; no reader for them has been traced yet.
        // Correction: Init :121799 and OnWaveCleared :116983 identify these.
        std::uint16_t wavesPerRevolution;
        std::uint16_t waveLimit;
        std::uint16_t perfectWaveRewardPercent;

        Template();

        /** @return false when the stream ran out before the template ended. */
        bool Init(CArrayInputStream &stream);
    };

    CLevel();
    CLevel(CResTOCManager &toc, ZPackTables &tables, const ZShaderProgram &program,
        std::shared_ptr<CParticlePool> particlePool = nullptr, std::shared_ptr<CParticleSystem> mapParticles = nullptr);
    ~CLevel();
    /** Windows audio adaptation: coalesce identical one-shots within one tick. */
    void BeginAudioFrame();
    /** Consume gun cues, then advance both new and existing projectiles. */
    void Update(CBrother &player, const float *modelToScene, float facingDegrees,
                int deltaMs, const ZWeaponCollision *collision = nullptr);
    /** Consume another brother's cues without advancing all projectiles twice. */
    void EmitBrother(CBrother &player, const float *modelToScene, float facingDegrees,
        ZCombatId owner, const ZWeaponCollision *collision = nullptr);
    /** Optional world-to-screen projection for the rotating character preview. */
    void Draw(const float *sceneMvp, const float *previewProjection = nullptr, float meshCameraScale = 1.0f,
              ZWeaponDrawPass pass = ZWeaponDrawPass::All, bool mapParticlesInQueue = false);
    std::vector<CParticleSystem::RenderItem> GetMapParticleItems() const;
    void DrawMapParticle(const CParticleSystem::RenderItem &item, const float *sceneMvp);
    void Clear();
    void SetCombatWorld(ZProjectileWorld *world);
    /**
     * The camera rectangle CBullet::CanBeCulled :60583 tests against.
     *
     * Without one no projectile is culled, which is what the standalone
     * research scenes and the arena had before.
     */
    void SetViewBounds(float centerX, float centerY, float width, float height);
    /** Enemy/manual projectile speed is in world units per second. */
    ZCombatId SpawnProjectile(const GameObjectRef &resource, float x, float y, float z,
        float direction, float speed, ZCombatId owner, int ownerType, int part = 0, int node = 0);
    void ResolveHit(ZCombatId projectile, ZHitResult result);
    void Emit(const ZGunCue &cue, float x, float y, float z, float direction,
        ZCombatId actor = 0, int slot = 0, int part = 0, int node = 0);
    bool RemoveOldestProjectile(ZCombatId owner);
    void RetireOwner(ZCombatId owner);
    void PlayMoveSound(const GameObjectRef &sound);
    /** CPickup owns an emitter handle; stopping it preserves living particles. */
    // The historical StopEffect name now means immediate Stop. Use StopSpawning to drain.
    std::uint64_t StartPersistentEffect(const GameObjectRef &resource, float x, float y, bool loop = false);
    void StopEffect(std::uint64_t handle);
    /** CPickup::OnRemove :99723 and CTransferEffect::Update :174361 drain. */
    void StopSpawning(std::uint64_t handle);
    /** Standalone research scenes have no brother/projectile update. */
    void AdvanceAmbientEffects(int deltaMs);
    /** Finite actor bursts include emitted particles after their emitter ends. */
    bool HasActorBurst(ZCombatId actor) const;
    void SetPaused(bool paused);
    std::size_t GetBulletCount() const;
    std::size_t GetParticleCount() const;
    std::size_t GetEffectCount() const;
    std::size_t GetTrailCount() const;
    std::size_t GetRibbonCount() const;
    std::size_t GetDrawnBeamQuadCount() const;
    std::size_t GetDrawnLightningQuadCount() const;
    std::size_t GetShotCount() const;
    std::vector<ZWeaponProjectileState> GetProjectileStates() const;
    std::size_t GetSoundCueCount() const;
    /** Voices sounding right now. One WAV never occupies more than one. */
    unsigned GetVoiceCount() const;

    void BindCombat(const std::vector<CEnemy::Template> &catalog, CBrother &player,
        ZPlayerVitals &vitals, float playerGameScale);

    /** Bind the original session owner after the level runtime is constructed. */
    void AttachRuntime(CGame &game, const std::vector<CEnemy::Template> &catalog);
    struct PickupCollection {
        GameObjectRef resource;
        int objectId = 0;
        unsigned peer = 0;
    };
    bool InitPickups(CResTOCManager &toc, ZPackTables &tables, const ZShaderProgram &program,
        CProfileManager *profile = nullptr);
    void ResetPickups();
    bool SpawnPickupAt(const GameObjectRef &pickup, float x, float y, int objectId = 0) override;
    void UpdatePickupAnimations(int deltaMs);
    void UpdatePickups(int deltaMs);
    void DrawPickups(const float *matrix, float scale);
    bool GetPickupPosition(int objectId, float &x, float &y) const;
    bool GetPickupIndicatorTarget(unsigned serial, float &x, float &y) const;
    bool FindNearestPickup(float x, float y, float &goalX, float &goalY) const;
    std::size_t GetPickupCount() const { return m_objects.GetPickups().size(); }
    unsigned GetPickupSpawnCount() const { return m_pickupSpawned; }
    unsigned GetPickupCollectedCount() const { return m_pickupCollected; }
    unsigned GetPickupFailureCount() const { return m_pickupFailures; }
    const std::vector<PickupCollection> &GetPickupCollections() const { return m_pickupCollections; }
    void SetPowerup(CPowerup *powerup, ZCombatId owner = kPlayerCombatId) {
        if (owner == kBrotherCombatId) { m_peerPowerups = powerup; }
        else { m_powerups = powerup; }
    }
    bool UsePowerup(CPowerUpSelector &selector, const ZPowerupEntry &entry, bool fromSelector, bool decrement);
    void UpdatePowerup(CPowerup &powerup, int deltaMs);
    void ResetPowerup(CPowerup &powerup);
    void SetMatch(CMPMatch *match) {
        m_match = match;
        ZGameScriptObject::SetDeathmatch(match != nullptr);
    }
    void SetArchive(bool archive) { m_archive = archive; }
    void SetViewSize(float width, float height);
    void ResetWorld(float x, float y, float facingDegrees);
    void RefreshCamera() { UpdateCamera(); }

    void Reset();
    /** Map and geometry must outlive the level runtime. */
    void SetMap(CMap &map, const CCollisionData &collision, ZWeaponCollision &weaponCollision,
        float cameraScale, float playerRadius);
    CEnemy *Spawn(std::size_t entry, float x, float y);
    bool PreloadEnemies(const RequirementList &requirements, const CScript &levelScript);
    CEnemy *SpawnNearby(std::size_t entry);
    void Update(int deltaMs, float moveX, float moveY, bool shoot);
    void PlayerMatrix(float *matrix) const;
    void SetBrother(CBrother *model, CBrotherAI *brother);
    void SetLocalLive(bool enabled) { m_localLive = enabled; }
    bool IsLocalLive() const { return m_localLive; }
    bool IsDeathmatch() const { return m_match != nullptr; }
    void SetDeathmatch(CMPMatch *match, const std::vector<ZWeaponEntry> *weapons);
    bool StartDeathmatch();
    bool IsMatchSpawnPending(unsigned peer) const;
    void UpdateDeathmatch(unsigned deltaMs);
    bool AdvanceDeathmatchEnding(int deltaMs);
    bool RespawnDeathmatch(unsigned peer, bool initial = false, bool resumeFromShop = false);
    bool EquipMatchGun(unsigned peer, const GameObjectRef &ref, bool resetActor = false);
    bool CollectMatchWeapon(unsigned peer, unsigned index);
    bool RequestMatchWeaponSwap(unsigned peer);
    bool FinishMatchWeaponSwap(unsigned peer);
    const GameObjectRef &MatchGun(unsigned peer, unsigned slot) const { return m_gunConfigurations[peer][slot]; }
    GameObjectRef ActiveMatchGun(unsigned peer) const;
    bool SelectMatchGun(unsigned peer, unsigned slot, const GameObjectRef &gun);
    void SetMatchShopping(unsigned peer, bool shopping) { m_matchShopping[peer] = shopping; }
    bool HasLineOfFire(float x, float y, float targetX, float targetY) const;
    bool CanHitBrother(const ZCombatHit &hit, ZCombatId target) const;
    void RecordMatchDeath(unsigned peer, int killer);
    void SetLocalBot(bool enabled) { m_localBot = enabled; }
    bool HasLocalBot() const { return m_localBot && m_brotherModel != nullptr; }
    bool KillTestBot();
    bool ReviveTestBot();
    bool ReviveActor(ZCombatId actor, unsigned reason);
    void SetAfterDeathAvailability(bool player, bool peer) {
        m_afterDeathAvailable[0] = player;
        m_afterDeathAvailable[1] = peer;
    }
    bool NeedsDeathChoice(unsigned peer) const;
    void FinishDeathChoice(unsigned peer);
    std::vector<ZBrotherAIWorld::Threat> GetBrotherThreats() const override;
    bool CanBrotherWalk(float x, float y, float destinationX, float destinationY) const override;
    void ActorPosition(ZCombatId actor, float &x, float &y) const;
    void SetPeerProgress(CPlayerProgress *progress) { m_peerProgress = progress; }
    std::uint64_t GetPeerExperience() const {
        if (m_peerProgress != nullptr) { return m_peerProgress->GetExperience(); }
        return 0;
    }
    void SetPlayerGunSlot(unsigned slot) { m_playerGunSlot = slot; }
    const ZMultiplayerStatistics &GetMultiplayerStatistics(unsigned peer) const { return m_multiplayer[peer]; }
    void ClearWaveStatistics();
    void AddPeerExperience(unsigned amount);
    void AddPeerXplodium(unsigned amount);
    void SetPeerProfile(CProfileManager *profile) { m_peerProfile = profile; }
    void SetGunConfiguration(unsigned peer, unsigned slot, const GameObjectRef &ref, unsigned masteryLimit);
    bool BrotherTouchesPickup(float x, float y) const;
    bool BrotherIsCloser(float x, float y) const;
    bool IsPlayerDown() const override { return m_vitals != nullptr && m_vitals->dead; }
    bool IsTeamDeathComplete() const;
    bool IsRescuePending() const {
        return m_localLive && m_brother != nullptr && m_vitals != nullptr &&
            m_vitals->dead != m_brother->vitals.dead;
    }
    float GetReviveProgress() const { return m_reviveProgress; }
    unsigned GetReviveCount() const { return m_reviveCount; }
    bool SetReviveResources(const CScript &script);
    unsigned GetReviveEffectState() const { return m_reviveEffectState; }
    void UpdatePeerIndicator(unsigned deltaMs, float left, float top, float width, float height);
    const CLevelIndicator *PeerIndicator() const {
        if (!m_peerIndicatorVisible) { return nullptr; }
        return &m_peerIndicator;
    }
    void SetBrotherWeapons(const CScript &script, const CGun::Template &pistol, const CGun::Template &rifle);
    bool RequestBrotherWeaponSwap();
    bool SwapBrotherWeapon();
    unsigned GetBrotherWeaponSlot() const { return m_brotherWeaponSlot; }
    void ResetBrotherPosition(float x, float y, float facingDegrees);
    void BrotherMatrix(float *matrix) const;
    ZCombatId FindBrotherTarget(float x, float y, float radius) override;
    bool GetBrotherTarget(ZCombatId id, float &x, float &y) override;
    bool GetBrotherWaypoint(float x, float y, float targetX, float targetY,
        float &waypointX, float &waypointY) override;
    void ResolveBrotherForce(float previousX, float previousY, float &x, float &y) override;
    void EnemyMatrix(const CEnemy &enemy, float *matrix) const;
    struct MovementBounds { float left, top, right, bottom; };
    void ResolveMovement(float previousX, float previousY, float &x, float &y,
        float radius, bool player = true) const;
    MovementBounds GetPlayerMovementBounds() const { return {m_left, m_top, m_right, m_bottom}; }
    /** Current BIG geometry, including the active prop collision snapshot. */
    const CCollisionData *GetCollisionData() const { return m_collision; }
    const ILayerPath *GetNavigationPath() const {
        if (m_map == nullptr) { return nullptr; }
        return m_map->GetPathLayer(m_pathLayer);
    }
    void EnemyCircle(const CEnemy &enemy, int part, float &x, float &y, float &radius) const;
    struct HealthBar {
        float x, y, width, height, border, fraction, red;
        float green = 0, blue = 0;
    };
    std::vector<HealthBar> EnemyHealthBars(float viewportScale = 1) const;
    struct ExperienceText {
        unsigned amount = 0;
        float x = 0, y = 0, alpha = 1;
        unsigned elapsedMs = 0;
    };
    const std::vector<ExperienceText> &GetExperienceTexts() const { return m_experienceTexts; }
    void UpdateExperienceTexts(int deltaMs);
    void SetTextView(float left, float top, float scaleX, float scaleY) {
        m_textViewX = left;
        m_textViewY = top;
        m_textScaleX = scaleX;
        m_textScaleY = scaleY;
    }
    CEnemy *Find(ZCombatId id);
    const CEnemy *Find(ZCombatId id) const;
    std::size_t AliveCount() const;
    float GetPlayerRadius() const { return m_playerRadius; }
    ZPlayerVitals &GetPlayerVitals() { return *m_vitals; }
    bool Suicide();
    ZPlayerVitals *GetBrotherVitals() {
        if (m_brother != nullptr) { return &m_brother->vitals; }
        return nullptr;
    }
    CLevel *GetScriptLevel() override {
        if (m_template == nullptr) { return nullptr; }
        return this;
    }
    void SetProps(Props *props) { m_props = props; }
    void SetPlayerProgress(CPlayerProgress *progress);
    void AddExperience(unsigned amount);
    std::uint64_t GetExperience() const {
        if (m_actor.GetProgress() != nullptr) { return m_actor.GetProgress()->GetExperience(); }
        return 0;
    }
    void AddXplodium(unsigned amount);
    void AddHealth(unsigned amount);
    bool TouchesPickup(float x, float y) const;
    std::uint64_t GetXplodium() const { return m_actor.GetXplodium(); }
    ZCombatId GetAutoAimTarget() const { return m_actor.GetTargetingController().GetTarget(); }
    bool HasClearPath(float x, float y, float targetX, float targetY, float radius) const;
    bool TestEnemyLineOfSight(float x, float y, float targetX, float targetY) const;
    bool CanWalkTo(float x, float y, float targetX, float targetY) const;
    ZCombatTrace Trace(const ZCombatHit &hit, float x, float y, float dx, float dy,
        float radius, const std::vector<ZCombatId> &skipTargets) override;
    ZHitResult ApplyHit(ZCombatId target, const ZCombatHit &hit) override;
    float GetDamageMultiplier(ZCombatId owner, float fallback = 1) const override;
    float GetProjectilePowerupMultiplier(ZCombatId owner) const override;
    float GetEnemyTimeScale() const override;
    unsigned GetTotalKills() const;
    const CEnemy::ResourceCache &GetEnemyModelCache() const { return m_objects.GetEnemyModelCache(); }
    void SetViewCenter(float x, float y) {
        m_viewCenterX = x;
        m_viewCenterY = y;
        m_hasViewCenter = true;
    }
    float GetViewCenterX() const { return m_hasViewCenter ? m_viewCenterX : m_actor.x; }
    float GetViewCenterY() const { return m_hasViewCenter ? m_viewCenterY : m_actor.y; }
    void Splash(const ZCombatHit &hit, float radius, float coneDegrees,
        float force, int forceMs) override;
    void SplashBrothers(float x, float y, float radius, float damage, float force, int forceMs);
    void SpawnFromProjectile(const GameObjectRef &resource, const ZCombatHit &hit) override;
    bool FindTarget(const ZCombatHit &hit, float radius, float &x, float &y) override;
    bool ParticleAnchor(ZCombatId actor, float &x, float &y, float &z, float &angle) override;
    bool LinkedParticleAnchor(ZCombatId actor, int node, float &x, float &y, float &z, float &angle) override;
    bool Anchor(ZCombatId actor, int part, int node,
        float &x, float &y, float &z, float &direction) override;
    std::vector<std::unique_ptr<CEnemy>> &GetEnemies() { return m_objects.GetEnemies(); }
    const std::vector<std::unique_ptr<CEnemy>> &GetEnemies() const { return m_objects.GetEnemies(); }
    CPlayer &GetPlayer() { return m_actor; }
    const CPlayer &GetPlayer() const { return m_actor; }
    float damageDealt = 0;
    float lastDamage = 0;
    unsigned hits = 0;
    unsigned kills = 0;
    unsigned GetSpawnCount() const { return m_objects.GetSpawnCount(); }
    unsigned GetInvalidSpawnCount() const { return m_objects.GetInvalidSpawnCount(); }
    void RecordInvalidSpawn() { m_objects.RecordInvalidSpawn(); }

    /**
     * Bind a template and its map, then run the script's start handler.
     *
     * Same order as the original: the script is set first, the map second, and
     * only then is export 0 called -- which is what lets OnLevelStart reach
     * into the map and set a layer scrolling.
     *
     * The template and map must outlive the level.
     */
    void Bind(const Template &levelTemplate, CMap &map, CEnemyWorld *world = nullptr, int startWave = 0);

    /**
     * Seed the stream CGame natives 1 and 2 draw from for this level's script.
     *
     * The original singleton CRandGen is seeded from the clock in its
     * constructor (:370383), so a level script that rolls for a spawn -- the
     * pack12 script picks one of four babes and one of four nodes with
     * CGame.random(0, 99) -- gets a different answer every session. This port
     * gives each script host its own stream, so without a seed every session
     * replays the same rolls. Research checks keep the fixed default.
     */
    void SetScriptRandomSeed(std::uint32_t seed) { SetRandomSeed(seed); }
    void SetWave(int wave);
    void EnableTutorial(bool enabled) { m_tutorialEnabled = enabled; }
    int GetTutorialStep() const { return m_tutorialStep; }
    std::int16_t *TutorialStepVariable() { return &m_tutorialStep; }
    void TutorialAdvance();
    bool CanBrotherShoot() const { return m_brotherCanShoot; }
    const Template &GetTemplate() const { return *m_template; }
    int GetWaveLimit() const { return m_template->waveLimit; }
    void Update(int deltaMs);
    void Update(int deltaMs, float moveX, float moveY, bool fire, bool advanceScript);
    void UpdateAfterDeath(int deltaMs);
    bool IsDeathComplete() const;
    bool IsPowerupMovieActive() const;
    std::vector<std::string> TakePowerupUseMessages();
    /** HUD/movie completion callbacks use event class 4 in the original. */
    void HandleEvent(std::uint8_t event) {
        if (!m_cleared) { m_interpreter.HandleEvent(4, event); }
    }
    void OnEnemyKilled(int objectId, const GameObjectRef &enemy);
    /** CLevel::OnEnemyKilled reward/statistic half, before Flow export 5. */
    void RewardEnemy(const CEnemy &actor);
    void OnEnemyTeleport(int objectId, const GameObjectRef &enemy);
    void BeginCombatFrame();
    void QueueLevelEvent(std::uint8_t event) { m_pendingLevelEvents.push_back(event); }
    void QueueEnemyTeleport(int objectId, const GameObjectRef &enemy);
    void QueuePickupSpawn(const GameObjectRef &pickup, float x, float y);
    std::size_t GetTeleportEventCount() const { return m_pendingTeleports.size(); }
    bool IsActivePortal(int objectId) const override;
    void OnPickupCollected(int objectId, const GameObjectRef &pickup);
    void OnDeathmatchKill(float x, float y);
    void OnPropEvent(int objectId, const GameObjectRef &prop, bool entered);
    /** Original trigger export 6; disabled and paused groups do not fire. */
    bool OnTrigger(int group);
    int GetTriggerLayer() const { return m_triggerLayer; }
    void UpdateProximitySpawns(float left, float top, float width, float height);
    void CheckForCameraChange(float playerX, float playerY);
    bool GetResource(int index, GameObjectRef &out) const;
    bool GetStringResource(int index, CGameAssetRef &out) const;
    int GetDialogResource() const { return m_dialogResource; }
    unsigned GetDialogSerial() const { return m_dialogSerial; }
    unsigned GetDialogArrow() const { return m_dialogArrow; }
    unsigned GetTriggerCount() const { return m_triggerCount; }
    bool DoesDialogAutoClose() const { return m_dialogAutoClose; }
    bool IsDialogCloseRequested() const { return m_dialogCloseRequested; }
    const GameObjectRef &GetNextLevel() const { return m_nextLevel; }
    void CompleteDialog();
    CEnemySpawner &GetSpawner() { return m_spawner; }
    bool SetIndicator(int objectId, unsigned type, std::uint64_t targetKey = 0);
    void RemoveIndicator(int objectId);
    void UpdateIndicators(int deltaMs, float left, float top, float width, float height);
    const std::vector<CLevelIndicator> &GetIndicators() const { return m_indicators; }
    unsigned GetKills() const { return m_kills; }
    int GetWave() const { return m_variables[0]; }
    /** DrawEnemyHealthBars :120501 reads Flow variable 4, not GetRevolution. */
    bool HasLargeEnemyHealthBars() const { return m_variables[4] != 0; }
    // CLevel::GetRevolution/GetRevolutionCount use Flow variable 2 as divisor.
    int GetWavesPerRevolution() const { return m_variables[2]; }
    int GetRealWave() const {
        if (m_variables[2] > 0) { return m_variables[0] % m_variables[2]; }
        return 0;
    }
    int GetStateId() const { return m_interpreter.GetStateId(); }
    int GetStopwatchTime() const { return m_stopwatchMs; }
    bool IsBrotherLabelVisible() const { return m_brotherLabelVisible; }
    float GetBrotherLabelAlpha() const { return m_brotherLabelAlpha; }
    float GetObjectTimeScale() const { return m_objectTimeScale; }
    /** Native 5 / UpdateNormal :121317, before native 58's selective scaling. */
    int TransformWorldElapseMS(int deltaMs) const;
    float GetWorldTimeScale() const { return m_worldTimeScale; }
    unsigned GetEnemyLimit() const { return m_enemyLimit; }
    int GetXplodiumMultiplierPercent() const { return m_xplodiumMultiplierPercent; }
    bool CanPlayerMove() const { return m_playerCanMove; }
    bool CanPlayerShoot() const { return m_playerCanShoot; }
    unsigned GetBossIntroSerial() const { return m_bossIntroSerial; }
    /** CEnemy::SetCameraTarget :68799 switches mode before setting world XY. */
    void FocusCameraOnEnemy(float x, float y);
    int GetRespawnPathLayer() const { return m_respawnPathLayer; }
    unsigned GetStatisticsGroup() const { return m_statisticsGroup; }
    unsigned GetKillsInStatisticsGroup(unsigned group) const { return m_statisticsKills[group & 255]; }
    unsigned GetStat42Bits() const { return m_stat42Bits; }
    bool IsCleared() const { return m_cleared; }
    bool IsPaused() const { return m_paused; }
    int GetObjectLayer() const { return m_objectLayer; }
    int GetPathLayer() const { return m_pathLayer; }
    float GetEnemyMultiplier(int enemy, int attribute) const;
    float GetEnemyMultiplier(const GameObjectRef &enemy, int attribute) const;
    int CountEnemies(const GameObjectRef *enemy = nullptr, int objectId = -1) const override;
    bool SpawnEnemy(const GameObjectRef &enemy, int layer, int node, int objectId) override;
    bool GetObjectPosition(int objectId, float &x, float &y) const override;
    bool GetIndicatorTarget(std::uint64_t key, float &x, float &y) const override;
    float GetClosestSpawnDistance() const { return m_closestSpawnDistance; }
    unsigned GetOnScreenSpawns() const { return m_onScreenSpawns; }
    unsigned GetPowerupCount(unsigned localIndex) const override;
    void ResetCombatProgress();
    void ResolveWaveReward(unsigned perfectRewardPercent);
    void SetHorde(bool enabled) { m_horde = enabled; }
    unsigned GetScore() const { return m_score; }
    unsigned GetKillStreak() const { return m_killStreak; }
    unsigned GetBestKillStreak() const { return m_bestKillStreak; }
    void ResetKillStreak() { m_killStreak = 0; }
    std::uint64_t GetLastWaveBonus() const { return m_lastWaveBonus; }
    unsigned GetPerfectWaves() const { return m_perfectWaves; }
    unsigned GetClearedWaves() const { return m_clearedWaves; }
    const std::vector<bool> &GetWavePerfectResults() const { return m_wavePerfectResults; }
    const std::vector<ZWeaponCombatProgress> &GetWeaponProgress() const { return m_weaponProgress; }
    const std::vector<CEnemyCasualty> &GetCasualties() const { return m_casualties; }
    std::vector<CChallengeManager::Kill> TakeChallengeKills();
    std::vector<GameObjectRef> TakeChallengePowerups();
    void RecordChallengePowerup(const GameObjectRef &ref) { m_challengePowerups.push_back(ref); }
    void AddMatchScore(unsigned points, unsigned streak);
    void CreditWeaponProgress(const GameObjectRef &weapon, unsigned experience, unsigned masteryLimit);

    /** How many native calls were made that nothing implements. */
    std::uint32_t GetUnimplementedCallCount() const { return m_unimplementedCalls; }

    /**
     * Run one of CLevel's native functions. Called by ScriptResolver for
     * class id 5. Reference: :117365
     */
    std::int16_t FunctionResolver(std::uint8_t function, const std::int16_t *arguments,
                                  std::uint8_t argumentCount);

    /**
     * Where one of CLevel's eight script variables lives. Reference: :114371
     * @return null when the variable is outside that range.
     */
    std::int16_t *VariableResolver(std::uint8_t variable);

private:
    void ApplyPickupActions(CPickup &pickup, unsigned peer);
    void UpdateLocalRevive(int deltaMs);
    void CreditAssistMastery(unsigned peer, unsigned slot, unsigned experience);
    void Actions(CEnemy &actor);
    void SelectTarget(CEnemy &actor);
    void PartMatrix(const CEnemy &actor, int part, float *matrix) const;
    void FinishSpawns();
    ZCombatId ParticipantOwner(ZCombatId owner) const;
    void UpdateNavigation(CEnemy &actor);
    void ApplyBrotherForce(ZCombatId target, float x, float y, int durationMs);

    CPlayer m_actor;
    const std::vector<ZWeaponEntry> *m_matchWeapons = nullptr;
    unsigned m_auxiliaryMs[2]{};
    unsigned m_matchSlots[2]{};
    bool m_matchShopping[2]{};
    ZCollisionPoint m_matchInitialSpawns[2];
    float m_matchInitialAngles[2]{};
    ZCollisionPoint m_matchMapSpawn;
    bool m_matchSwap[2]{};
    unsigned m_matchStreaks[2]{};
    std::vector<unsigned> m_matchDeaths;
    bool m_localLive = false;
    bool m_localBot = false;
    bool m_localBotReviveRequested = false;
    ZMultiplayerStatistics m_multiplayer[2];
    CPlayerProgress *m_peerProgress = nullptr;
    unsigned m_playerGunSlot = 0;
    CProfileManager *m_peerProfile = nullptr;
    GameObjectRef m_gunConfigurations[2][2];
    unsigned m_gunMasteryLimits[2][2]{};
    bool m_afterDeathAvailable[2]{};
    unsigned m_deathChoiceHandled[2]{};
    float m_reviveProgress = 0;
    unsigned m_reviveCount = 0;
    ZCombatId m_reviveTarget = 0;
    GameObjectRef m_reviveEffects[2];
    std::uint64_t m_reviveEffectHandle = 0;
    unsigned m_reviveEffectState = 0;
    ZCombatId m_reviveEffectTarget = 0;
    CLevelIndicator m_peerIndicator;
    bool m_peerIndicatorVisible = false;
    std::vector<CEnemy::CombatState *> m_flockEnemies;
    CFlock m_flock;
    std::vector<ExperienceText> m_experienceTexts;
    float m_textViewX = 0;
    float m_textViewY = 0;
    float m_textScaleX = 1;
    float m_textScaleY = 1;
    ZPackTables *m_tables = nullptr;
    const ZShaderProgram *m_program = nullptr;
    CLevelObjectPool m_objects;
    CBrother *m_playerModel = nullptr;
    CBrother *m_brotherModel = nullptr;
    CBrotherAI *m_brother = nullptr;
    const CScript *m_brotherScript = nullptr;
    const CGun::Template *m_brotherWeapons[2]{};
    unsigned m_brotherWeaponSlot = 0;
    ZPlayerVitals *m_vitals = nullptr;
    float m_playerGameScale = 1;
    const CCollisionData *m_collision = nullptr;
    ZWeaponCollision *m_weaponCollision = nullptr;
    float m_cameraScale = 1;
    float m_viewCenterX = 0;
    float m_viewCenterY = 0;
    bool m_hasViewCenter = false;
    float m_playerRadius = kArenaPlayerCollisionRadius;
    float m_left = 35;
    float m_top = 150;
    float m_right = kArenaWidth - 35;
    float m_bottom = kArenaHeight - 35;

    void UpdateScript(int deltaMs);
    void UpdateMapInteractions(float previousX, float previousY);
    void UpdateCamera(int deltaMs = 0);
    int CountEnemySlots(const GameObjectRef *enemy = nullptr) const override;
    void StartObjectLayer(int layer) override;
    bool SpawnMapObject(const CLayerObject::Object &object, int objectId) override;
    void SendEnemyMessage(int objectId, int message) override;
    void SendPropMessage(int objectId, int message) override;
    void SetEnemyPortal(int enemyId, int propId) override;
    void PlayLevelSound(const GameObjectRef &sound) override;
    void OnWaveCleared(unsigned perfectRewardPercent) override;
    bool SpawnPickup(const GameObjectRef &pickup, int layer, int node, int objectId, bool nearby) override;
    bool SpawnMPMatchPickup(const GameObjectRef &pickup, int layer) override;
    std::uint64_t ResolveIndicatorTarget(int objectId) const override;
    unsigned m_kills = 0;
    bool m_tutorialEnabled = false;
    unsigned m_triggerCount = 0;
    std::int16_t m_tutorialStep = -1;
    bool m_brotherCanShoot = true;
    std::vector<CLevelIndicator> m_indicators;
    void SpawnMapObjects(int tag, int objectId = -1);
    /** setCameraLayer. Reference: :117497 */
    void SetCameraLayer(const std::int16_t *arguments, std::uint8_t argumentCount);

    /** setCollisionLayer. Reference: :117508 */
    void SetCollisionLayer(const std::int16_t *arguments,
                           std::uint8_t argumentCount);

    /** setTileLayerSpeed. Reference: :117823 */
    void SetTileLayerSpeed(const std::int16_t *arguments, std::uint8_t argumentCount);

    const Template *m_template;
    CMap *m_map;
    CScriptInterpreter m_interpreter;

    // The eight variables CLevel::VariableResolver hands out pointers to. In
    // the original these are fields of the level object; here they are just
    // storage, so a script that reads or writes one behaves consistently even
    // though nothing else touches them yet. Their meanings come with M4.
    std::int16_t m_variables[kLevelVariableCount];

    std::uint32_t m_unimplementedCalls;
    CEnemySpawner m_spawner;
    CEnemyWorld *m_world = nullptr;
    int m_timerMs = 0;
    int m_timerFunction = -1;
    int m_eventTimerMs = 0;
    int m_objectLayer = -1;
    std::vector<bool> m_cameraEntered;
    std::vector<bool> m_spawnedObjects;
    bool m_manualSpawnTags[256] = {};
    int m_pathLayer = -1;
    int m_triggerLayer = -1;
    bool m_triggerEnabled[32] = {};
    int m_triggerPauseMs[32] = {};
    unsigned m_dialogArrow = 0;
    int m_dialogResource = -1;
    unsigned m_dialogSerial = 0;
    bool m_dialogAutoClose = false;
    bool m_dialogCloseRequested = false;
    GameObjectRef m_nextLevel;
    bool m_cleared = false;
    bool m_paused = false;
    int m_stopwatchMs = 0;
    bool m_stopwatchRunning = false;
    bool m_brotherLabelVisible = false;
    float m_brotherLabelAlpha = 0;
    // CLevelObjectPool constructor :145400; native 59 may raise this to 50.
    unsigned m_enemyLimit = 20;
    int m_xplodiumMultiplierPercent = 100;
    bool m_playerCanMove = true, m_playerCanShoot = true;
    unsigned m_bossIntroSerial = 0;
    int m_respawnPathLayer = -1;
    float m_objectTimeScale = 1;
    float m_worldTimeScale = 1;
    std::uint8_t m_statisticsGroup = 0;
    unsigned m_statisticsKills[256] = {};
    unsigned m_stat42Bits = 0; // Original CPlayerStatistics record 42, native 82.
    float m_globalEnemyMultipliers[5] = {1, 1, 1, 1, 1};
    float m_enemyMultipliers[32][5] = {};
    CGame *m_game = nullptr;
    const std::vector<CEnemy::Template> *m_catalog = nullptr;
    CMPMatch *m_match = nullptr;
    // BIG pickup templates and expanded Sprite frames; no live object ownership.
    // Templates/packs belong to the level; expanded frames belong to CSpritePlayer.
    std::map<std::uint32_t, std::vector<CPickup::Template>> m_pickupTemplates;
    std::map<std::uint32_t, std::unique_ptr<CSpriteGlu>> m_pickupSpritePacks;
    std::unique_ptr<ZQuadBatch> m_pickupBatch;
    CProfileManager *m_pickupProfile = nullptr;
    unsigned m_pickupSpawned = 0, m_pickupCollected = 0, m_pickupFailures = 0;
    std::vector<PickupCollection> m_pickupCollections;
    // CLevel owns live bullets; each bullet owns its four attached effects.
    // CMap's system and the transient layer retain independent pools/lifetimes.
    ZProjectileWorld *m_projectileWorld = nullptr;
    ZCombatId m_nextProjectile = 1;
    std::unique_ptr<ZSpriteRenderer> m_effectSprites;
    std::unique_ptr<ZCombatAudio> m_combatAudio;
    std::unique_ptr<ZBulletResources> m_bulletResources;
    std::unique_ptr<ZParticleResources> m_particleResources;
    ZProjectileView m_projectileView;
    std::uint32_t m_effectRandom = 1;
    std::size_t m_shotsFired = 0, m_drawnBeamQuads = 0, m_drawnLightningQuads = 0;
    float m_effectPlayerX = 0, m_effectPlayerY = 0;
    std::shared_ptr<CParticleSystem> m_mapParticles;
    std::shared_ptr<CParticlePool> m_effectLayerPool;
    CEffectLayer m_effectLayer;
    ZEffectColors m_effectColors;
    std::vector<std::unique_ptr<CBullet>> m_bullets;
    std::map<ZCombatId, std::weak_ptr<CBrother::PowerupParticles>> m_brotherParticles;
    float RandomProjectile(float minimum, float maximum);
    CParticleEffectPlayer *StartParticleEffect(const GameObjectRef &ref, float x, float y, float z, float angle);
    void EmitBulletCue(const ZGunCue &cue, float x, float y, float z, float direction, CBullet *owner = nullptr);
    void AdvanceParticles(int deltaMs);
    Props *m_props = nullptr;
    bool CommitPowerupUse(CPowerUpSelector &selector, const GameObjectRef &resource, unsigned count);
    CPowerup *m_powerups = nullptr;
    CPowerup *m_peerPowerups = nullptr;
    unsigned m_spawnSerial = 0;
    bool m_archive = false;
    float m_closestSpawnDistance = -1;
    unsigned m_onScreenSpawns = 0;
    float m_cameraLeft = 0;
    float m_cameraTop = 0;
    float m_cameraWidth = 0;
    float m_cameraHeight = 0;
    float m_viewWidth = 572;
    float m_viewHeight = 429;
    std::vector<ZWeaponCombatProgress> m_weaponProgress;
    std::vector<CEnemyCasualty> m_casualties;
    std::vector<CChallengeManager::Kill> m_challengeKills;
    std::vector<GameObjectRef> m_challengePowerups;
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
    struct PendingTeleport {
        int objectId = -1;
        GameObjectRef enemy;
    };
    struct PendingPickup {
        GameObjectRef resource;
        float x = 0;
        float y = 0;
    };
    std::vector<std::uint8_t> m_pendingLevelEvents;
    std::vector<PendingTeleport> m_pendingTeleports;
    std::vector<PendingPickup> m_pendingPickups;
};

#endif  // GUN_BROS_RE_GUN_BROS_CLEVEL_H
