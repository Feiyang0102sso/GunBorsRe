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
    CLevel & scene;
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
    CLevel & scene;
    CBrotherAI & brother;
    ZPlayerModel & brotherModel;
    CGame & session;
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
    CLevel & scene;
    CGame & session;
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
    CLevel & scene;
    CGame & session;
    MapDetail::ZMapPropWorld & props;
    float startX;
    float startY;
    float startFacing;
};

struct SurvivalLevelSoundsFixture {
    unsigned & checkFailures;
    bool check;
    CGame & session;
    CLevel & scene;
};

struct SurvivalPropRoutesFixture {
    unsigned & checkFailures;
    bool check;
    MapDetail::ZLoadedMap & loaded;
    CLevel & scene;
};

struct SurvivalTriggerRoutesFixture {
    unsigned & checkFailures;
    bool check;
    CGame & session;
    CMap & map;
    CLevel & scene;
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
    CLevel & scene;
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
    CLevel & scene;
    CBrotherAI & brother;
    CGame & session;
    CPowerUpSelector & powerups;
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
    CLevel & scene;
    CBrotherAI & brother;
    ZPlayerModel & brotherModel;
    CGame & session;
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
    CLevel & scene;
    CGame & session;
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
    CLevel & scene;
    CGame & session;
    ZPickupScene & pickups;
    bool horde;
};

struct SurvivalPowerupCaptureFixture {
    std::string & capturePath;
    bool powerupStudy;
    CLevel & scene;
    CPowerUpSelector & powerups;
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
    CLevel & scene;
    CBrotherAI & brother;
    ZPlayerModel & brotherModel;
    CGame & session;
    CInputPad & survivalHud;
    CPlayerProgress & progress;
    CMPMatch & match;
    ZPickupScene & pickups;
    CPowerUpSelector & powerups;
    CPowerUpSelector & peerPowerups;
    CProfileManager * peerProfile;
    ZSurvivalGameContext * gameContext;
    float startX;
    float startY;
    float startFacing;
};
