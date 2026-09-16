#include "gun_bros_re/gameplay/ZGameScriptObject.h"
/**
 * @file CLevel.h
 * @brief A level: a map plus the script that drives it.
 *
 * Port of CLevel (src/gunbros/level.cpp), cut down to what M3.4 needs.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:114771 (Template::Init),
 *            :114371 (VariableResolver), :117365 (FunctionResolver),
 *            :120988 (where CLevel binds its script and calls export 0)
 *
 * The template is shallow -- a map reference, the script, three uint16 -- and
 * the level object exists here only to be the script's host. Everything a
 * level really does (spawning, objectives, triggers, the camera) is M4 and M5.
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
#include "gun_bros_re/gameplay/CEnemySpawner.h"
#include "gun_bros_re/gameplay/CLevelIndicator.h"

#include <cstdint>

class CMap;

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
class CLevel : public ZGameScriptObject {
public:
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

    /**
     * Bind a template and its map, then run the script's start handler.
     *
     * Same order as the original: the script is set first, the map second, and
     * only then is export 0 called -- which is what lets OnLevelStart reach
     * into the map and set a layer scrolling.
     *
     * The template and map must outlive the level.
     */
    void Bind(const Template &levelTemplate, CMap &map, ZLevelWorld *world = nullptr, int startWave = 0);

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
    /** HUD/movie completion callbacks use event class 4 in the original. */
    void HandleEvent(std::uint8_t event) {
        if (!m_cleared) { m_interpreter.HandleEvent(4, event); }
    }
    void OnEnemyKilled(int objectId, const GameObjectRef &enemy);
    void OnEnemyTeleport(int objectId, const GameObjectRef &enemy);
    bool IsActivePortal(int objectId) const { return m_world != nullptr && m_world->IsActivePortal(objectId); }
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
    ZLevelWorld *m_world = nullptr;
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
};

#endif  // GUN_BROS_RE_GUN_BROS_CLEVEL_H
