#pragma once
// Test-only borrowed views, assembled by SurvivalCheckScenario.
#include "gun_bros_re/gameplay/ZSurvivalScenario.h"

struct SurvivalRewardsFixture {
    unsigned & checkFailures;
    bool check;
    CResTOCManager & toc;
    ZPackTables & tables;
    std::vector<ZEnemyTemplateData> & enemies;
    ZPlayerVitals & vitals;
    CPlayerProgress::Template & progressData;
    ZWindow & window;
    CInputPad & survivalHud;
    ZShaderProgram & program;
    MapDetail::ZLoadedMap & loaded;
    ZPlayerModel & player;
    ZWeaponEffects & effects;
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
    ZPlayerVitals & vitals;
    ZWindow & window;
    ZShaderProgram & program;
    ZQuadBatch & batch;
    MapDetail::ZLoadedMap & loaded;
    ZPlayerModel & player;
    ZWeaponEffects & effects;
    ZCombatWorld & scene;
    CBrotherAI & brother;
    ZPlayerModel & brotherModel;
    ZLevelHost & session;
    float startX;
    float startY;
    float startFacing;
};

struct SurvivalBossFixture {
    unsigned & checkFailures;
    const std::string & packShortName;
    bool bossStudy;
    CResTOCManager & toc;
    ZPackTables & tables;
    std::vector<ZEnemyTemplateData> & enemies;
    ZPlayerVitals & vitals;
    ZWindow & window;
    ZShaderProgram & program;
    MapDetail::ZLoadedMap & loaded;
    ZPlayerModel & player;
    ZCombatWorld & scene;
    ZLevelHost & session;
    float startX;
    float startY;
    float startFacing;
};

struct SurvivalFeedbackFixture {
    unsigned & checkFailures;
    std::string & capturePath;
    bool feedbackStudy;
    CResTOCManager & toc;
    ZPackTables & tables;
    std::vector<ZWeaponEntry> & weapons;
    std::vector<ZEnemyTemplateData> & enemies;
    ZPlayerVitals & vitals;
    CInputPad & survivalHud;
    ZShaderProgram & program;
    MapDetail::ZLoadedMap & loaded;
    ZPlayerModel & player;
    ZCombatWorld & scene;
    ZLevelHost & session;
    MapDetail::ZMapPropWorld & props;
    float startX;
    float startY;
    float startFacing;
};

struct SurvivalLevelSoundsFixture {
    unsigned & checkFailures;
    bool check;
    ZLevelHost & session;
    ZWeaponEffects & effects;
};

struct SurvivalPropRoutesFixture {
    unsigned & checkFailures;
    bool check;
    MapDetail::ZLoadedMap & loaded;
    ZCombatWorld & scene;
};

struct SurvivalTriggerRoutesFixture {
    unsigned & checkFailures;
    bool check;
    ZLevelHost & session;
    CMap & map;
    ZCombatWorld & scene;
    float startX;
    float startY;
    float startFacing;
};

struct SurvivalPlacedPropsFixture {
    bool check;
    MapDetail::ZLoadedMap & loaded;
};

struct SurvivalBrotherPoseFixture {
    unsigned & checkFailures;
    bool check;
    bool withBrother;
    ZCombatWorld & scene;
    CBrotherAI & brother;
    ZPlayerModel & brotherModel;
};

struct SurvivalTutorialFixture {
    unsigned & checkFailures;
    std::string & capturePath;
    bool check;
    ZSurvivalGameContext * gameContext;
    ZPackTables & tables;
    std::vector<ZWeaponEntry> & weapons;
    ZPlayerVitals & vitals;
    CPlayerProgress & progress;
    ZShaderProgram & program;
    MapDetail::ZLoadedMap & loaded;
    ZPlayerModel & player;
    std::size_t & weaponSlot;
    unsigned & equippedWeaponSlot;
    ZCombatWorld & scene;
    CBrotherAI & brother;
    ZLevelHost & session;
    ZPowerupScene & powerups;
    ZPickupScene & pickups;
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
    ZSurvivalGameContext * gameContext;
    bool withBrother;
    bool powerupStudy;
    const ZMissionEntry * archiveMission;
    CResTOCManager & toc;
    ZPackTables & tables;
    std::vector<ZWeaponEntry> & weapons;
    std::vector<ZEnemyTemplateData> & enemies;
    ZPlayerVitals & vitals;
    CPlayerProgress & progress;
    ZWindow & window;
    ZShaderProgram & program;
    MapDetail::ZLoadedMap & loaded;
    ZPlayerModel & player;
    std::size_t & weaponSlot;
    ZWeaponEffects & effects;
    ZCombatWorld & scene;
    CBrotherAI & brother;
    ZPlayerModel & brotherModel;
    ZLevelHost & session;
    ZPickupScene & pickups;
    MapDetail::ZMapPropWorld & props;
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
    ZPlayerVitals & vitals;
    MapDetail::ZLoadedMap & loaded;
    ZCombatWorld & scene;
    ZLevelHost & session;
    bool horde;
};

struct SurvivalCampaignFixture {
    unsigned & checkFailures;
    std::string & capturePath;
    const std::string & packShortName;
    unsigned mapIndex;
    bool check;
    const ZMissionEntry * archiveMission;
    ZPlayerVitals & vitals;
    MapDetail::ZLoadedMap & loaded;
    ZCombatWorld & scene;
    ZLevelHost & session;
    ZPickupScene & pickups;
    bool horde;
};

struct SurvivalPowerupCaptureFixture {
    std::string & capturePath;
    bool powerupStudy;
    ZCombatWorld & scene;
    ZPowerupScene & powerups;
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
    ZPackTables & tables;
    ZPlayerVitals & vitals;
    ZWindow & window;
    ZShaderProgram & program;
    ZQuadBatch & batch;
    MapDetail::ZLoadedMap & loaded;
    ZPlayerModel & player;
    ZWeaponEffects & effects;
    ZCombatWorld & scene;
    CBrotherAI & brother;
    ZPlayerModel & brotherModel;
    ZLevelHost & session;
    CInputPad & survivalHud;
    CPlayerProgress & progress;
    CMPMatch & match;
    ZPickupScene & pickups;
    ZPowerupScene & powerups;
    ZPowerupScene & peerPowerups;
    CProfileManager * peerProfile;
    ZSurvivalGameContext * gameContext;
    float startX;
    float startY;
    float startFacing;
};
