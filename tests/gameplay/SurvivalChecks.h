#pragma once
#include "gun_bros_re/gameplay/MapWorldInternal.h"

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
int CheckSurvivalRewards(SurvivalRewardsFixture fixture);

struct SurvivalPowerupInventoryFixture {
    bool powerupStudy;
    CResTOCManager & toc;
    CProfileManager &researchProfile;
};
int CheckSurvivalPowerupInventory(SurvivalPowerupInventoryFixture fixture);

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
int CheckSurvivalDeath(SurvivalDeathFixture fixture);

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
int CheckSurvivalBoss(SurvivalBossFixture fixture);

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
int CheckSurvivalFeedback(SurvivalFeedbackFixture fixture);

struct SurvivalLevelSoundsFixture {
    unsigned & checkFailures;
    bool check;
    SurvivalSession & session;
};
int CheckSurvivalLevelSounds(SurvivalLevelSoundsFixture fixture);

struct SurvivalPropRoutesFixture {
    unsigned & checkFailures;
    bool check;
    MapDetail::MapPropWorld & props;
};
int CheckSurvivalPropRoutes(SurvivalPropRoutesFixture fixture);

struct SurvivalTriggerRoutesFixture {
    unsigned & checkFailures;
    bool check;
    SurvivalSession & session;
    float startX;
    float startY;
    float startFacing;
};
int CheckSurvivalTriggerRoutes(SurvivalTriggerRoutesFixture fixture);

struct SurvivalPlacedPropsFixture {
    bool check;
    MapDetail::LoadedMap & loaded;
};
int CheckSurvivalPlacedProps(SurvivalPlacedPropsFixture fixture);

struct SurvivalBrotherPoseFixture {
    unsigned & checkFailures;
    bool check;
    bool withBrother;
    CombatScene & scene;
    CBrotherAI & brother;
    PlayerModel & brotherModel;
};
int CheckSurvivalBrotherPose(SurvivalBrotherPoseFixture fixture);

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
int CheckSurvivalTutorial(SurvivalTutorialFixture fixture);

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
int CheckSurvivalWaves(SurvivalWavesFixture fixture);

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
int CheckSurvivalHorde(SurvivalHordeFixture fixture);

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
int CheckSurvivalCampaign(SurvivalCampaignFixture fixture);

struct SurvivalPowerupCaptureFixture {
    std::string & capturePath;
    bool powerupStudy;
    CombatScene & scene;
    PowerupScene & powerups;
};
int CheckSurvivalPowerupCapture(SurvivalPowerupCaptureFixture fixture);
