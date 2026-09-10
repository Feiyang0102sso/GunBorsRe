/** @file SurvivalSession.h
 * @brief Desktop host connecting original level scripts to the actor world.
 */
#ifndef GUN_BROS_RE_SURVIVALSESSION_H
#define GUN_BROS_RE_SURVIVALSESSION_H
#include "gun_bros/CLevel.h"
#include "runtime/CombatScene.h"
#include "runtime/PickupScene.h"
#include "runtime/IPropWorld.h"
#include "runtime/PowerupScene.h"

class SurvivalHud;
class SurvivalSession : public IEnemySpawnWorld {
public:
    SurvivalSession(CombatScene &scene, CMap &map, const std::vector<EnemyTemplateData> &catalog);
    bool Load(CResTOCManager &toc, PackTables &tables, std::uint32_t mapPack, unsigned mapIndex,
        const GameObjectRef *selectedLevel = nullptr, bool archive = false);
    /** x, y and facingDegrees all come from the map's PLAYER spawn object;
     *  both brothers spawn with the same angle. */
    void Restart(float x, float y, float facingDegrees);
    void SetHorde(bool enabled) { m_horde = enabled; m_archive = false; m_scene.SetHorde(enabled); }
    void SetStartWave(int wave) { m_startWave = wave; }
    void SetDialogHud(SurvivalHud *hud) { m_dialogHud = hud; }
    void SetOriginalHud(SurvivalHud *hud) { m_originalHud = hud; }
    bool HasOriginalHud() const { return m_originalHud != nullptr; }
    void Update(int deltaMs, float moveX, float moveY, bool fire);
    void UpdateAfterDeath(int deltaMs);
    bool SpawnEnemy(const GameObjectRef &enemy, int layer, int node, int objectId) override;
    int CountEnemies(const GameObjectRef *enemy = nullptr, int objectId = -1) const override;
    void StartObjectLayer(int layer) override;
    bool SpawnMapObject(const PlacedObject &object, int objectId) override;
    void SendEnemyMessage(int objectId, int message) override;
    void SendPropMessage(int objectId, int message) override;
    void PlayLevelSound(const GameObjectRef &sound) override;
    unsigned CheckLevelSounds();
    unsigned CheckTriggerRoutes(float startX, float startY, float startFacing);
    void SetProps(IPropWorld *props) { m_props = props; }
    void SetPowerups(PowerupScene *powerups) { m_powerups = powerups; }
    void OnWaveCleared(unsigned perfectRewardPercent) override;
    void SetPickups(PickupScene *pickups, WeaponEffects *effects) { m_pickups = pickups; m_effects = effects; }
    bool SpawnPickup(const GameObjectRef &pickup, int layer, int node, int objectId, bool nearby) override;
    bool SpawnPickupAt(const GameObjectRef &pickup, float x, float y, int objectId) override;
    bool GetObjectPosition(int objectId, float &x, float &y) const override;
    std::uint64_t ResolveIndicatorTarget(int objectId) const override;
    bool GetIndicatorTarget(std::uint64_t key, float &x, float &y) const override;
    CLevel &GetLevel() { return m_level; }
    bool IsTransitioning() const;
    unsigned GetTransitionElapsed() const;
    unsigned GetKills() const { return m_kills; }
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
    void UpdateDialog(int deltaMs);
    void UpdateMapInteractions(float previousX, float previousY);
    void UpdateCamera(int deltaMs = 0);
    /** CEnemySpawner::GetSpawnPoint :146098; -1 when no node qualifies. */
    int ChooseSpawnNode(const ILayerPath &path);
    CLevel::Template m_template;
    CLevel m_level;
    CombatScene &m_scene;
    CMap &m_map;
    const std::vector<EnemyTemplateData> &m_catalog;
    int m_transitionMs = 1200;
    int m_transitionDuration = 1200;
    unsigned m_bossIntroSerial = 0;
    unsigned m_spawnSerial = 0;
    unsigned m_kills = 0;
    int m_startWave = 0;
    bool m_archive = false;
    bool m_horde = false;
    SurvivalHud *m_dialogHud = nullptr;
    SurvivalHud *m_originalHud = nullptr;
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
    PickupScene *m_pickups = nullptr;
    WeaponEffects *m_effects = nullptr;
    IPropWorld *m_props = nullptr;
    PowerupScene *m_powerups = nullptr;
};
#endif
