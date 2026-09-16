/** @file ZLevelHost.h
 * @brief Desktop host connecting original level scripts to the actor world.
 */
#ifndef GUN_BROS_RE_ZLEVELHOST_H
#define GUN_BROS_RE_ZLEVELHOST_H
#include "gun_bros_re/gameplay/CLevel.h"
#include "gun_bros_re/gameplay/ZCombatWorld.h"
#include "gun_bros_re/gameplay/ZPickupScene.h"
#include "gun_bros_re/gameplay/ZPropWorld.h"
#include "gun_bros_re/gameplay/ZPowerupScene.h"
#include "gun_bros_re/gameplay/CMPMatch.h"
#include <chrono>

class CInputPad;
class ZLevelHost : public ZLevelWorld {
public:
    ZLevelHost(ZCombatWorld &scene, CMap &map, const std::vector<ZEnemyTemplateData> &catalog);
    bool Load(CResTOCManager &toc, ZPackTables &tables, std::uint32_t mapPack, unsigned mapIndex,
        const GameObjectRef *selectedLevel = nullptr, bool archive = false);
    /** x, y and facingDegrees all come from the map's PLAYER spawn object;
     *  both brothers spawn with the same angle. */
    void Restart(float x, float y, float facingDegrees);
    void SetHorde(bool enabled) { m_horde = enabled; m_archive = false; m_scene.SetHorde(enabled); }
    void SetStartWave(int wave) { m_startWave = wave; }
    void SetDeathmatch(CMPMatch *match) { m_match = match; m_level.SetDeathmatch(match != nullptr); }
    void SetDialogHud(CInputPad *hud) { m_dialogHud = hud; }
    void SetChallenges(CChallengeManager *manager, CProfileManager *profile, const std::vector<ZWeaponEntry> *weapons) {
        m_challenges = manager; m_challengeProfile = profile; m_challengeWeapons = weapons;
    }
    bool SubmitChallenges(bool ended, bool waveCleared = false);
    void SetHud(CInputPad *hud) { m_hud = hud; }
    bool HasHud() const { return m_hud != nullptr; }
    void Update(int deltaMs, float moveX, float moveY, bool fire);
    void SetSuspended(bool suspended) { m_suspended = suspended; }
    void UpdateAfterDeath(int deltaMs);
    /** Shared gameplay exit gate, also exercised by the fatal-hit regression. */
    bool IsDeathComplete() const {
        if (m_powerups != nullptr && m_powerups->IsMovieActive()) { return false; }
        if (m_peerPowerups != nullptr && m_peerPowerups->IsMovieActive()) { return false; }
        return m_scene.IsTeamDeathComplete();
    }
    /** Shared session exit decision used by the host loop and regression. */
    // CLevel::OnLevelCleared -> CGame::OnMissionSuccess starts mission wrap-up.
    bool IsFinished() const {
        if (m_match != nullptr) { return m_match->GetResult() != CMPMatch::Result::Playing; }
        return m_level.IsCleared() || IsDeathComplete();
    }
    bool IsReadyForResults() const;
    bool IsDeathmatchFading() const { return m_matchFading; }
    /** Desktop cheat: drain the current wave through original Flow callbacks. */
    bool SkipToBoss();
    bool StartBossSkip();
    void AdvanceBossSkip();
    bool IsBossSkipActive() const { return m_bossSkipActive; }
    bool SpawnEnemy(const GameObjectRef &enemy, int layer, int node, int objectId) override;
    int CountEnemies(const GameObjectRef *enemy = nullptr, int objectId = -1) const override;
    int CountEnemySlots(const GameObjectRef *enemy = nullptr) const override;
    void StartObjectLayer(int layer) override;
    bool SpawnMapObject(const ZPlacedObject &object, int objectId) override;
    void SendEnemyMessage(int objectId, int message) override;
    void SendPropMessage(int objectId, int message) override;
    void SetEnemyPortal(int enemyId, int propId) override;
    bool IsActivePortal(int propId) const override { return m_props != nullptr && m_props->IsActivePortal(propId); }
    void PlayLevelSound(const GameObjectRef &sound) override;

    void SetProps(ZPropWorld *props) { m_props = props; }
    void SetPowerups(ZPowerupScene *powerups) { m_powerups = powerups; }
    void SetPeerPowerups(ZPowerupScene *powerups) { m_peerPowerups = powerups; }
    void OnWaveCleared(unsigned perfectRewardPercent) override;
    void SetPickups(ZPickupScene *pickups, ZWeaponEffects *effects) { m_pickups = pickups; m_effects = effects; }
    bool SpawnPickup(const GameObjectRef &pickup, int layer, int node, int objectId, bool nearby) override;
    bool SpawnPickupAt(const GameObjectRef &pickup, float x, float y, int objectId) override;
    bool SpawnMPMatchPickup(const GameObjectRef &pickup, int layer) override;
    bool GetObjectPosition(int objectId, float &x, float &y) const override;
    std::uint64_t ResolveIndicatorTarget(int objectId) const override;
    bool GetIndicatorTarget(std::uint64_t key, float &x, float &y) const override;
    CLevel &GetLevel() { return m_level; }
    bool IsTransitioning() const;
    unsigned GetTransitionElapsed() const;
    unsigned GetKills() const { return m_level.GetKills(); }
    /** Spawn placement evidence: the closest rule-driven spawn to the player,
     *  and how many of them appeared inside the camera rectangle. */
    float GetClosestSpawnDistance() const { return m_closestSpawnDistance; }
    unsigned GetOnScreenSpawns() const { return m_onScreenSpawns; }
    /** Visible world size at the original 0.8 baseline; scale varies per tick. */
    void SetViewSize(float width, float height) { m_viewWidth = width; m_viewHeight = height; }
    /** Seed this level's script rolls; see CLevel::SetScriptRandomSeed. */
    void SetScriptRandomSeed(std::uint32_t seed) { m_scriptRandomSeed = seed; m_hasScriptRandomSeed = true; }
    const std::string &GetDialogText() const { return m_dialogText; }
    void CompleteDialog();
    unsigned GetPowerupCount(unsigned localIndex) const override;
private:
    void FinishBossSkip();
    bool m_bossSkipActive = false;
    unsigned m_bossSkipIntroSerial = 0, m_bossSkipDefeated = 0;
    int m_bossSkipElapsedMs = 0;
    std::int16_t m_bossSkipPreviousFlag = 0;
    std::chrono::steady_clock::time_point m_bossSkipStarted;
    CMPMatch *m_match = nullptr;
    // User-requested host hold after the original death animation and particles.
    static constexpr int MatchEndingHoldMs = 100;
    int m_matchEndingHoldMs = 0;
    bool m_matchFading = false;
    bool m_suspended = false;
    void UpdateDialog(int deltaMs);
    void UpdateMapInteractions(float previousX, float previousY);
    void UpdateCamera(int deltaMs = 0);
    CChallengeManager *m_challenges = nullptr;
    CProfileManager *m_challengeProfile = nullptr;
    const std::vector<ZWeaponEntry> *m_challengeWeapons = nullptr;
    GameObjectRef m_levelReference;
    bool m_challengeSessionEnded = false;
    CLevel::Template m_template;
    CLevel m_level;
    ZCombatWorld &m_scene;
    CMap &m_map;
    const std::vector<ZEnemyTemplateData> &m_catalog;
    int m_transitionMs = 1200;
    int m_transitionDuration = 1200;
    unsigned m_bossIntroSerial = 0;
    unsigned m_spawnSerial = 0;
    int m_startWave = 0;
    bool m_archive = false;
    bool m_horde = false;
    CInputPad *m_dialogHud = nullptr;
    CInputPad *m_hud = nullptr;
    bool m_bossWave = false;
    std::uint32_t m_scriptRandomSeed = 0;
    bool m_hasScriptRandomSeed = false;
    // The camera rectangle the off-screen spawn filter tests against, kept as
    // UpdateCamera computes it.
    float m_closestSpawnDistance = -1;
    unsigned m_onScreenSpawns = 0;
    float m_cameraLeft = 0, m_cameraTop = 0, m_cameraWidth = 0, m_cameraHeight = 0;
    float m_viewWidth = 572;
    float m_viewHeight = 429;
    CResTOCManager *m_toc = nullptr;
    std::string m_dialogText;
    bool m_dialogBound = false;
    unsigned m_dialogSerial = 0;
    ZPickupScene *m_pickups = nullptr;
    ZWeaponEffects *m_effects = nullptr;
    ZPropWorld *m_props = nullptr;
    ZPowerupScene *m_powerups = nullptr;
    ZPowerupScene *m_peerPowerups = nullptr;
};
#endif
