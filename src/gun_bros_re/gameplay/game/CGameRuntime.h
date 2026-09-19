#pragma once
/** CGame's desktop session storage and stage implementation.
 * References borrow the resources created by CGame::Run; they outlive this session.
 * This nested host representation is not the original CGame memory layout.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/game/CGameFlow.h"
#include "gun_bros_re/gameplay/map/CMapResources.h"
#include "gun_bros_re/gameplay/map/CCameraDrawing.h"
#include "gun_bros_re/ui/CInputPad.h"
#include "gun_bros_re/gameplay/CBGM.h"
#include "gun_bros_re/data/ZMissionCatalog.h"
#include "engine/graphics/ZMarkerBatch.h"
#include "engine/graphics/ZQuadBatch.h"
#include "engine/graphics/ZShaderProgram.h"
#include "engine/core/ZMatrix4d.h"
#include "gun_bros_re/ZHostSettings.h"
#include "gun_bros_re/gameplay/game/ZGameObserver.h"
#include "gun_bros_re/gameplay/game/ZLiveShopSession.h"
#include "gun_bros_re/gameplay/brother/bot/ZLocalCoopBot.h"
#include "gun_bros_re/gameplay/brother/bot/ZLocalPVPBot.h"
#include "gun_bros_re/gameplay/brother/bot/ZLocalBotFriend.h"
#include "gun_bros_re/ZLocalOnlineServices.h"

struct CGame::Session {
    const CGame::Launch &launch;
    ZPlayerVitals &vitals;
    ZWindow &window;
    ZShaderProgram &program;
    ZQuadBatch &batch;
    CMap &loaded;
    CBrother &player;
    CLevel &scene;
    CBrotherAI &brother;
    CBrother &brotherModel;
    CGame &session;
    float startX;
    float startY;
    float startFacing;
    CResTOCManager &toc;
    ZPackTables &tables;
    std::vector<CEnemy::Template> &enemies;
    std::vector<ZWeaponEntry> &weapons;
    CInputPad &survivalHud;
    CLevel::Props &props;
    bool withBrother;
    CPlayerProgress &progress;
    std::size_t &weaponSlot;
    unsigned &equippedWeaponSlot;
    CPowerUpSelector &powerups;
    CProfileManager *pickupProfile;
    bool tutorial;
    std::uint64_t &accountedXplodium;
    int packIndex;
    const GameObjectRef *archiveLevel;
    bool horde;
    CMPMatch &match;
    CPowerUpSelector &peerPowerups;
    CProfileManager *peerProfile;
    const CPlayerConfiguration &brotherConfiguration;

    const std::vector<CMPMatch::Entry> &matches;
    CPlayerProgress &peerProgress;
    CChallengeManager &challenges;
    ZMarkerBatch &markers;
    ZShaderProgram &markerProgram;
    ZLocalCoopBot *localBot;
    ZLocalPVPBot *deathmatchBot;
    CBGM &music;
    const CPlayerProgress::Template &progressData;
    ZGameObserver::Options options;

    CGameFlow *gameContext = launch.gameContext;
    const std::string &packShortName = launch.packShortName;
    unsigned mapIndex = launch.mapIndex;
    const ZMissionEntry *archiveMission = launch.archiveMission;
    bool showCollisions = options.showCollisions;
    unsigned runtimeFailures = 0;
    bool paused = false;
    bool shopOpen = false, itemChoice = false;
    int accumulator = 0;
    int lastSavedWave = session.GetLevel().GetWave();
    int lastSavedTutorialStep = session.GetLevel().GetTutorialStep();
    bool savedDeath = false;
    std::size_t pendingWeapon = weapons.size();
    unsigned pendingEquippedSlot = 0, primaryEquippedSlot = 0;
    bool swapEventAccepted = false;
    ZLiveShopSession liveShop, botShop;
    unsigned deathShopSerial = UINT32_MAX;
    bool matchShopCounted = false, matchShopPurchased = false;
    std::vector<ZStoreEntry> matchStore;
    bool deathShop = false;
    std::string brotherName;
    GameObjectRef leftPowerup = powerups.GetEquipped(0);
    GameObjectRef rightPowerup = powerups.GetEquipped(1);
    ZGameObserver::Frame frame{pickupProfile,
                               survivalHud,
                               session,
                               scene,
                               player,
                               vitals,
                               window,
                               music,
                               leftPowerup,
                               rightPowerup,
                               paused,
                               shopOpen,
                               equippedWeaponSlot,
                               accumulator};
    std::uint64_t previous = 0, menuTicks = 0, frameTicks = 0;
    unsigned menuElapsed = 0;
    CCamera::Viewport camera;
    int width = 0, height = 0;
    float baselineZoom = 1;
    bool hudOwnsPointer = false;

    int Run();
    int UpdateShop();
    int ProcessInput();
    int Update();
    int Draw();
    int Finish();
    ZInputPadState BuildHudState();
    bool OpenShop(unsigned peer, bool exempt = false);
    void CloseShop();
    // Lifecycle/frame access is shared with callers; all test policy lives in tests/.
    int Notify(ZGameObserver::FramePhase phase);
    int Notify(ZGameObserver::Stage phase);
};
