#pragma once
// Test-only borrowed views, assembled by SurvivalCheckScenario.
#include "gun_bros_re/gameplay/game/CGameRuntime.h"
#include "gun_bros_re/gameplay/map/CMapInternal.h"

struct SurvivalRewardsFixture {
    unsigned & checkFailures;
    bool check;
    CResTOCManager & toc;
    ZPackTables & tables;
    std::vector<CEnemy::Template> & enemies;
    CBrother::Vitals & vitals;
    CPlayerProgress::Template & progressData;
    ZWindow & window;
    CInputPad & survivalHud;
    ZShaderProgram & program;
    CMap & loaded;
    CBrother & player;
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
    CBrother::Vitals & vitals;
    ZWindow & window;
    ZShaderProgram & program;
    ZQuadBatch & batch;
    CMap & loaded;
    CBrother & player;
    CLevel & scene;
    CBrotherAI & brother;
    CBrother & brotherModel;
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
    std::vector<CEnemy::Template> & enemies;
    CBrother::Vitals & vitals;
    ZWindow & window;
    ZShaderProgram & program;
    CMap & loaded;
    CBrother & player;
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
    std::vector<CEnemy::Template> & enemies;
    CBrother::Vitals & vitals;
    CInputPad & survivalHud;
    ZShaderProgram & program;
    CMap & loaded;
    CBrother & player;
    CLevel & scene;
    CGame & session;
    CLevel::Props & props;
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
    CMap & loaded;
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
    CMap & loaded;
};

struct SurvivalBrotherPoseFixture {
    unsigned & checkFailures;
    bool check;
    bool withBrother;
    CLevel & scene;
    CBrotherAI & brother;
    CBrother & brotherModel;
};

struct SurvivalTutorialFixture {
    unsigned & checkFailures;
    std::string & capturePath;
    bool check;
    CGameFlow * gameContext;
    ZPackTables & tables;
    std::vector<ZWeaponEntry> & weapons;
    CBrother::Vitals & vitals;
    CPlayerProgress & progress;
    ZShaderProgram & program;
    CMap & loaded;
    CBrother & player;
    std::size_t & weaponSlot;
    unsigned & equippedWeaponSlot;
    CLevel & scene;
    CBrotherAI & brother;
    CGame & session;
    CPowerUpSelector & powerups;
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
    CGameFlow * gameContext;
    bool withBrother;
    bool powerupStudy;
    const ZMissionEntry * archiveMission;
    CResTOCManager & toc;
    ZPackTables & tables;
    std::vector<ZWeaponEntry> & weapons;
    std::vector<CEnemy::Template> & enemies;
    CBrother::Vitals & vitals;
    CPlayerProgress & progress;
    ZWindow & window;
    ZShaderProgram & program;
    CMap & loaded;
    CBrother & player;
    std::size_t & weaponSlot;
    CLevel & scene;
    CBrotherAI & brother;
    CBrother & brotherModel;
    CGame & session;
    CLevel::Props & props;
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
    CBrother::Vitals & vitals;
    CMap & loaded;
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
    CBrother::Vitals & vitals;
    CMap & loaded;
    CLevel & scene;
    CGame & session;
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
    CBrother::Vitals & vitals;
    ZWindow & window;
    ZShaderProgram & program;
    ZQuadBatch & batch;
    CMap & loaded;
    CBrother & player;
    CLevel & scene;
    CBrotherAI & brother;
    CBrother & brotherModel;
    CGame & session;
    CInputPad & survivalHud;
    CPlayerProgress & progress;
    CMPMatch & match;
    CPowerUpSelector & powerups;
    CPowerUpSelector & peerPowerups;
    CProfileManager * peerProfile;
    CGameFlow * gameContext;
    float startX;
    float startY;
    float startFacing;
};
