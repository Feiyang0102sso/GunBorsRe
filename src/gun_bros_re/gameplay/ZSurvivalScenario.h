#pragma once
/** Borrowed runtime views. Observers never own the level, actors or resources. */
#include "gun_bros_re/gameplay/map/CMapInternal.h"

constexpr int kScenarioContinue = -1;

struct ZSurvivalResources {
    CResTOCManager & toc;
    ZPackTables & tables;
    std::vector<CEnemy::Template> & enemies;
    ZPlayerVitals & vitals;
    CPlayerProgress::Template & progressData;
    ZWindow & window;
    CInputPad & survivalHud;
    ZShaderProgram & program;
    CMap & loaded;
    CBrother & player;
    CLevel & scene;
};

struct ZSurvivalState {
    const CGame::Launch &launch;
    ZPlayerVitals & vitals;
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
    CResTOCManager & toc;
    ZPackTables & tables;
    std::vector<CEnemy::Template> & enemies;
    std::string & capturePath;
    std::vector<ZWeaponEntry> & weapons;
    CInputPad & survivalHud;
    CLevel::Props & props;
    bool withBrother;
    CPlayerProgress & progress;
    std::size_t & weaponSlot;
    unsigned & equippedWeaponSlot;
    CPowerUpSelector & powerups;
    CProfileManager * pickupProfile;
    bool tutorial;
    std::uint64_t & accountedXplodium;
    int packIndex;
    const GameObjectRef * archiveLevel;
    bool horde;
    CMPMatch & match;
    CPowerUpSelector & peerPowerups;
    CProfileManager * peerProfile;
    const CPlayerConfiguration &brotherConfiguration;
    bool finalizeProgress = false;
};

// Lifecycle points are independent of individual test cases.
enum class ZSurvivalPhase { Bound, Ready, Advanced, LoopStarting, WorldDrawn, Captured };

class ZSurvivalScenario {
public:
    virtual ~ZSurvivalScenario() = default;
    virtual int OnResources(ZSurvivalResources) { return kScenarioContinue; }
    virtual int OnInventory(CResTOCManager &, CProfileManager &) { return kScenarioContinue; }
    virtual int OnStage(ZSurvivalPhase, ZSurvivalState &) { return kScenarioContinue; }
};
