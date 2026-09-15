#pragma once
/** @file SurvivalScenario.h
 * @brief Hooks a development scenario can use to take over a survival session.
 *
 * The session loop owns the scene; a scenario only observes it or takes it
 * over. Every hook returns -1 to let the session continue, or >= 0 to end it
 * with that exit code. Production installs no scenario, so all of this is
 * inert unless SurvivalLaunch::scenario is set.
 *
 * The fixtures below are views onto session-local state. They live here, in
 * src/, so the loop never has to include anything out of tests/.
 */
#include "gun_bros_re/gameplay/MapWorldInternal.h"

/** Continue the session; the hook had nothing to say. */
constexpr int kScenarioContinue = -1;

struct SurvivalRewardsFixture {
    unsigned & checkFailures;
    bool check;
    CResTOCManager & toc;
    PackTables & tables;
    std::vector<EnemyTemplateData> & enemies;
    PlayerVitals & vitals;
    CPlayerProgress::Template & progressData;
    CWindow & window;
    SurvivalHud & survivalHud;
    CShaderProgram & program;
    MapDetail::LoadedMap & loaded;
    PlayerModel & player;
    WeaponEffects & effects;
};

struct SurvivalPowerupInventoryFixture {
    bool powerupStudy;
    CResTOCManager & toc;
    CProfileManager &researchProfile;
};

struct SurvivalDeathFixture {
    unsigned & checkFailures;
    const std::string & packShortName;
    bool deathStudy;
    PlayerVitals & vitals;
    CWindow & window;
    CShaderProgram & program;
    CQuadBatch & batch;
    MapDetail::LoadedMap & loaded;
    PlayerModel & player;
    WeaponEffects & effects;
    CombatScene & scene;
    CBrotherAI & brother;
    PlayerModel & brotherModel;
    SurvivalSession & session;
    float startX;
    float startY;
    float startFacing;
};

struct SurvivalBossFixture {
    unsigned & checkFailures;
    const std::string & packShortName;
    bool bossStudy;
    CResTOCManager & toc;
    PackTables & tables;
    std::vector<EnemyTemplateData> & enemies;
    PlayerVitals & vitals;
    CWindow & window;
    CShaderProgram & program;
    MapDetail::LoadedMap & loaded;
    PlayerModel & player;
    CombatScene & scene;
    SurvivalSession & session;
    float startX;
    float startY;
    float startFacing;
};

struct SurvivalFeedbackFixture {
    unsigned & checkFailures;
    std::string & capturePath;
    bool feedbackStudy;
    CResTOCManager & toc;
    PackTables & tables;
    std::vector<WeaponEntry> & weapons;
    std::vector<EnemyTemplateData> & enemies;
    PlayerVitals & vitals;
    SurvivalHud & survivalHud;
    CShaderProgram & program;
    MapDetail::LoadedMap & loaded;
    PlayerModel & player;
    CombatScene & scene;
    SurvivalSession & session;
    MapDetail::MapPropWorld & props;
    float startX;
    float startY;
    float startFacing;
};

struct SurvivalLevelSoundsFixture {
    unsigned & checkFailures;
    bool check;
    SurvivalSession & session;
};

struct SurvivalPropRoutesFixture {
    unsigned & checkFailures;
    bool check;
    MapDetail::MapPropWorld & props;
};

struct SurvivalTriggerRoutesFixture {
    unsigned & checkFailures;
    bool check;
    SurvivalSession & session;
    float startX;
    float startY;
    float startFacing;
};

struct SurvivalPlacedPropsFixture {
    bool check;
    MapDetail::LoadedMap & loaded;
};

struct SurvivalBrotherPoseFixture {
    unsigned & checkFailures;
    bool check;
    bool withBrother;
    CombatScene & scene;
    CBrotherAI & brother;
    PlayerModel & brotherModel;
};

struct SurvivalTutorialFixture {
    unsigned & checkFailures;
    std::string & capturePath;
    bool check;
    SurvivalGameContext * gameContext;
    PackTables & tables;
    std::vector<WeaponEntry> & weapons;
    PlayerVitals & vitals;
    CPlayerProgress & progress;
    CShaderProgram & program;
    MapDetail::LoadedMap & loaded;
    PlayerModel & player;
    std::size_t & weaponSlot;
    unsigned & equippedWeaponSlot;
    CombatScene & scene;
    CBrotherAI & brother;
    SurvivalSession & session;
    PowerupScene & powerups;
    PickupScene & pickups;
    CProfileManager * pickupProfile;
    bool tutorial;
    std::uint64_t & accountedXplodium;
};

struct SurvivalWavesFixture {
    unsigned & checkFailures;
    std::string & capturePath;
    const std::string & packShortName;
    unsigned mapIndex;
    bool check;
    unsigned checkWaves;
    unsigned startWave;
    SurvivalGameContext * gameContext;
    bool withBrother;
    bool powerupStudy;
    const MissionEntry * archiveMission;
    CResTOCManager & toc;
    PackTables & tables;
    std::vector<WeaponEntry> & weapons;
    std::vector<EnemyTemplateData> & enemies;
    PlayerVitals & vitals;
    CPlayerProgress & progress;
    CWindow & window;
    CShaderProgram & program;
    MapDetail::LoadedMap & loaded;
    PlayerModel & player;
    std::size_t & weaponSlot;
    WeaponEffects & effects;
    CombatScene & scene;
    CBrotherAI & brother;
    PlayerModel & brotherModel;
    SurvivalSession & session;
    PickupScene & pickups;
    MapDetail::MapPropWorld & props;
    bool tutorial;
    float startX;
    float startY;
    float startFacing;
    int packIndex;
    const GameObjectRef *archiveLevel;
};

struct SurvivalHordeFixture {
    unsigned & checkFailures;
    std::string & capturePath;
    bool check;
    unsigned startWave;
    PlayerVitals & vitals;
    MapDetail::LoadedMap & loaded;
    CombatScene & scene;
    SurvivalSession & session;
    bool horde;
};

struct SurvivalCampaignFixture {
    unsigned & checkFailures;
    std::string & capturePath;
    const std::string & packShortName;
    unsigned mapIndex;
    bool check;
    const MissionEntry * archiveMission;
    PlayerVitals & vitals;
    MapDetail::LoadedMap & loaded;
    CombatScene & scene;
    SurvivalSession & session;
    PickupScene & pickups;
    bool horde;
};

struct SurvivalPowerupCaptureFixture {
    std::string & capturePath;
    bool powerupStudy;
    CombatScene & scene;
    PowerupScene & powerups;
};

/** Everything the scene-ready hook can reach, once the map, players, session
 * and match are all built but before the first frame is presented. */
struct SurvivalSceneFixture {
    unsigned & checkFailures;
    const std::string & packShortName;
    unsigned mapIndex;
    bool localLive;
    bool deathStudy;
    CResTOCManager & toc;
    PackTables & tables;
    PlayerVitals & vitals;
    CWindow & window;
    CShaderProgram & program;
    CQuadBatch & batch;
    MapDetail::LoadedMap & loaded;
    PlayerModel & player;
    WeaponEffects & effects;
    CombatScene & scene;
    CBrotherAI & brother;
    PlayerModel & brotherModel;
    SurvivalSession & session;
    SurvivalHud & survivalHud;
    CPlayerProgress & progress;
    CMPMatch & match;
    PickupScene & pickups;
    PowerupScene & powerups;
    PowerupScene & peerPowerups;
    CProfileManager * peerProfile;
    SurvivalGameContext * gameContext;
    float startX;
    float startY;
    float startFacing;
};

/** Optional development hook set. Default implementations let the session run. */
class ISurvivalScenario {
public:
    virtual ~ISurvivalScenario() = default;

    /** Catalogs and progress are loaded; no scene exists yet. */
    virtual int OnRewards(SurvivalRewardsFixture) { return kScenarioContinue; }
    /** The research powerup inventory is ready. */
    virtual int OnPowerupInventory(SurvivalPowerupInventoryFixture) { return kScenarioContinue; }
    /** Map, players, session and match are built; the loop has not started. */
    virtual int OnSceneReady(SurvivalSceneFixture) { return kScenarioContinue; }

    virtual int OnBoss(SurvivalBossFixture) { return kScenarioContinue; }
    virtual int OnFeedback(SurvivalFeedbackFixture) { return kScenarioContinue; }
    virtual int OnLevelSounds(SurvivalLevelSoundsFixture) { return kScenarioContinue; }
    virtual int OnPropRoutes(SurvivalPropRoutesFixture) { return kScenarioContinue; }
    virtual int OnTriggerRoutes(SurvivalTriggerRoutesFixture) { return kScenarioContinue; }
    virtual int OnPlacedProps(SurvivalPlacedPropsFixture) { return kScenarioContinue; }
    virtual int OnBrotherPose(SurvivalBrotherPoseFixture) { return kScenarioContinue; }
    virtual int OnTutorial(SurvivalTutorialFixture) { return kScenarioContinue; }
    virtual int OnWaves(SurvivalWavesFixture) { return kScenarioContinue; }
    virtual int OnHorde(SurvivalHordeFixture) { return kScenarioContinue; }
    virtual int OnCampaign(SurvivalCampaignFixture) { return kScenarioContinue; }
    virtual int OnPowerupCapture(SurvivalPowerupCaptureFixture) { return kScenarioContinue; }

    /** Last call before the frame loop begins; may still adjust the scene. */
    virtual int OnLoopStarting(CombatScene &, PlayerVitals &) { return kScenarioContinue; }
};
