#include "gun_bros_re/debug/SurvivalDebug.h"
#include "gun_bros_re/debug/FrameRateOverlay.h"
#include "gun_bros_re/debug/DebugKeys.h"
#include "gun_bros_re/debug/CheatActions.h"
#include "gun_bros_re/debug/DebugMaps.h"
#include "gun_bros_re/gameplay/SurvivalRuntime.h"
#if GB_ENABLE_TESTS
#include "gameplay/SurvivalStudy.h"
#endif
#if GB_ENABLE_TESTS
#include "gameplay/SurvivalChecks.h"
#include "gameplay/CampaignDoorChecks.h"
#include "gameplay/DebugMapChecks.h"
#endif
#include "gun_bros_re/gameplay/MapWorldInternal.h"
#if GB_ENABLE_TESTS
#include "TestOutput.h"
#endif
using namespace MapDetail;

#if GB_ENABLE_TESTS
int RunSurvivalSession(const SurvivalLaunch &launch, const SurvivalDevelopment *development) {
#else
int RunSurvivalSession(const SurvivalLaunch &launch) {
#endif

    const auto &bigDirectory = launch.bigDirectory;
    const auto &packShortName = launch.packShortName;
    const unsigned mapIndex = launch.mapIndex;
    const unsigned weaponIndex = launch.weaponIndex;
    const int armorIndex = launch.armorIndex;
    const unsigned startWave = launch.startWave;
    auto *gameContext = launch.gameContext;
    const bool withBrother = launch.withBrother;
    const auto *archiveMission = launch.archiveMission;
    auto *sharedWindow = launch.window;
#if GB_ENABLE_TESTS
    SurvivalDevelopment defaults;
    if (development == nullptr) { development = &defaults; }
    const auto &screenshotPath = development->screenshotPath;
    const unsigned advanceMs = development->advanceMs;
    const bool firePreview = development->firePreview;
    bool showCollisions = development->showCollisions;
    const bool check = development->check;
    const unsigned checkWaves = development->checkWaves;
    const bool powerupStudy = development->powerupStudy;
    const bool performanceStudy = development->performanceStudy;
    const bool feedbackStudy = development->feedbackStudy;
    const bool bossStudy = development->bossStudy;
    const bool deathStudy = development->deathStudy;
#else
    const std::string screenshotPath;
    constexpr unsigned advanceMs = 0;
    bool showCollisions = false;
    constexpr bool firePreview = false, check = false, powerupStudy = false;
    constexpr bool performanceStudy = false, feedbackStudy = false, bossStudy = false, deathStudy = false;
#endif

    std::string capturePath = screenshotPath;
    unsigned checkFailures = 0;
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
    const int packIndex = toc.GetPackIndexFromName(packShortName.c_str());
    if (packIndex < 0) { return 1; }
    PackTables tables(toc);
    if (!tables.HasLatestBigVersion()) {
        std::printf("[survival] BigVersion 1 required; older formats are supported for resource viewing only\n");
        return 1;
    }
    std::vector<WeaponEntry> weapons;
    std::vector<EnemyTemplateData> enemies;
    PlayerVitals vitals;
    vitals.invincible = false;
    CPlayerProgress::Template progressData;
    CPlayerProgress progress;
    if (!LoadPlayerProgress(toc, tables, progressData)) { return 1; }
    progress.Bind(progressData);
    if (gameContext != nullptr) {
        progress.SetExperience(gameContext->profile.experience);
        gameContext->startingExperience = gameContext->profile.experience;
    }
    if (gameContext != nullptr && gameContext->tutorial) {
        // The zero-price level-one rifle is pack3 STORE 4 -> pack5 GUN 4.
        GameObjectRef rifle;
        rifle.packHash = CStringToKey("pack5");
        rifle.localIndex = 4;
        gameContext->profile.Grant(6, rifle);
        gameContext->profile.configuration.guns[1] = rifle;
    }
    CWindow ownedWindow;
    CWindow &window = sharedWindow ? *sharedWindow : ownedWindow;
    if (!window.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
    if (!SetDebugFPS(window, GameHostSettings().drawFPS, bigDirectory)) { return 1; }
    window.SetEscapeCloses(false);
    // Both the retail frontend and standalone survival research use shortcuts.
#if GB_ENABLE_CHEATS
    window.EnableCheats(true);
#endif
    CBGM ownedMusic;
    CBGM *activeMusic = &ownedMusic;
    if (gameContext != nullptr && gameContext->music != nullptr) { activeMusic = gameContext->music; }
    CBGM &music = *activeMusic;
    if (gameContext != nullptr) { music.SetEnabled(gameContext->profile.musicEnabled); }
    MovieRenderer loadingMovies;
    CResPackTOC *loadingCore = toc.GetPack(toc.GetCorePackIndex());
    if (!loadingMovies.Init(*loadingCore, *loadingCore)) { return 1; }
    const CProfileManager *loadingProfile = nullptr;
    if (gameContext != nullptr) { loadingProfile = &gameContext->profile; }
    LoadingScreen loading(window, loadingMovies, tables, loadingProfile, true, false, &music);
    if (!loading.IsValid()) { return 1; }
    if (!LoadWeaponCatalog(toc, tables, weapons) || !LoadEnemyCatalog(toc, tables, enemies) ||
        !LoadInitialPlayerHealth(toc, tables, vitals.maximum)) { return 1; }
    SurvivalHud survivalHud;
    if (!survivalHud.Init(toc, tables)) { return 1; }
    CShaderProgram program, markerProgram;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0") ||
        !markerProgram.Load(kShaderDirectory, "ogles_vs_mvp_constcolor", "ogles_ps_constcolor")) { return 1; }
    CQuadBatch batch;
    CMarkerBatch markers;
    if (!batch.Create(program) || !markers.Create(markerProgram)) { return 1; }
    LoadedMap loaded;
    if (!LoadMap(toc, packIndex, mapIndex, loaded)) { return 1; }
    LoadPlacedPlayers(toc, program, loaded);
    if (loaded.players.empty()) { return 1; }
    // The second brother will be driven by the partner system, not a stationary clone.
    loaded.players.resize(1);
    PlayerModel &player = *loaded.players[0].model;
    player.vitals = &vitals;
    if (gameContext != nullptr) {
        for (const auto &entry : gameContext->profile.weaponMastery) {
            const std::uint64_t key = (static_cast<std::uint64_t>(entry.resource.packHash) << 8) | entry.resource.localIndex;
            player.masteryByWeapon[key] = entry.experience;
        }
    }
    std::size_t weaponSlot = weaponIndex % weapons.size();
    unsigned equippedWeaponSlot = 0;
    std::size_t pendingWeapon = weapons.size();
    unsigned pendingEquippedSlot = 0, primaryEquippedSlot = 0;
    bool swapEventAccepted = false;
    unsigned combatSwapEvents = 0;
    if (gameContext != nullptr) {
        // CBrother::Bind :135820 selects the saved 1001 activeWeaponSlot.
        equippedWeaponSlot = gameContext->profile.activeWeaponSlot;
        const GameObjectRef &ref = gameContext->profile.configuration.guns[equippedWeaponSlot];
        bool found = false;
        for (std::size_t index = 0; index < weapons.size(); ++index) {
            if (weapons[index].packHash == ref.packHash && weapons[index].ordinal == ref.localIndex) {
                weaponSlot = index;
                found = true;
                break;
            }
        }
        if (!found) { return 1; }
    }
    if (!EquipControlledPlayer(tables, loaded, program, weapons[weaponSlot])) { return 1; }
    if (armorIndex >= 0) {
        std::vector<ArmorEntry> armors;
        if (!LoadArmorCatalog(toc, tables, armors) || armorIndex >= static_cast<int>(armors.size()) ||
            !EquipPlayerArmor(tables, armors[armorIndex].data, program, player)) { return 1; }
    }
    if (gameContext != nullptr) {
        std::vector<ArmorEntry> armors;
        if (!LoadArmorCatalog(toc, tables, armors)) { return 1; }
        for (const GameObjectRef &ref : gameContext->profile.configuration.armor) {
            if (ref.IsNull()) { continue; }
            bool found = false;
            for (const ArmorEntry &entry : armors) {
                if (entry.packHash == ref.packHash && entry.ordinal == ref.localIndex) {
                    if (!EquipPlayerArmor(tables, entry.data, program, player)) { return 1; }
                    found = true;
                    break;
                }
            }
            if (!found) { return 1; }
        }
    }
    WeaponEffects effects(toc, tables, program);
#if GB_ENABLE_TESTS
    {
        const int result = CheckSurvivalRewards({
            checkFailures, check, toc, tables, enemies, vitals, progressData, window, survivalHud, program, loaded, player, effects
        });
        if (result >= 0) { return result; }
    }
#endif

    CombatScene scene(tables, program, enemies, player, vitals, effects, loaded.playerTemplate->gameScale);
    if (gameContext != nullptr) {
        music.SetEnabled(gameContext->profile.musicEnabled);
        CAudioPlayer::SetEffectsEnabled(gameContext->profile.soundEnabled);
        player.brotherIndex = gameContext->profile.playerBrother;
    }
    CBrotherAI brother;
    PlayerModel brotherModel;
    CPlayerConfiguration brotherConfiguration;
    brotherConfiguration.SetDefaults(toc.GetPack(toc.GetCorePackIndex())->GetPackHash());
    // Local default partner: Whippersnappers and the free ER97E Elite rifle.
    // These are core gun 0 and pack5 gun 4 in the original store catalogue.
    brotherConfiguration.guns[1].packHash = toc.GetPack(toc.GetPackIndexFromName("pack5"))->GetPackHash();
    brotherConfiguration.guns[1].localIndex = 4;
    if (withBrother) {
        brother.vitals.maximum = progress.GetHealth();
        brother.vitals.invincible = false;
        brotherModel.vitals = &brother.vitals;
        brotherModel.human = false;
        brotherModel.brotherIndex = 1;
        if (gameContext != nullptr) { brotherModel.brotherIndex = 1 - gameContext->profile.playerBrother; }
        std::size_t brotherWeaponSlot = 0;
        for (std::size_t index = 0; index < weapons.size(); ++index) {
            if (weapons[index].packHash == brotherConfiguration.guns[0].packHash &&
                weapons[index].ordinal == brotherConfiguration.guns[0].localIndex) {
                brotherWeaponSlot = index;
                break;
            }
        }
        if (!BuildPlayerBody(tables, player.moveSet, brotherModel) ||
            !EquipPlayerWeapon(tables, loaded.playerTemplate->script, weapons[brotherWeaponSlot].data,
                "AI brother", brotherModel) || !CreatePlayerBuffers(brotherModel, program)) { return 1; }
        for (const GameObjectRef &ref : brotherConfiguration.armor) {
            if (ref.IsNull()) { continue; }
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(ref.packHash, GameSection::Armor, ref.localIndex, payload)) { return 1; }
            CArrayInputStream input(payload);
            CArmor::Template armor;
            if (!armor.Init(input) || !EquipPlayerArmor(tables, armor, program, brotherModel)) { return 1; }
        }
        // Regression: a local default partner must never inherit premium gear.
        
#if GB_ENABLE_TESTS
if (check) {
            const unsigned coreHash = toc.GetPack(toc.GetCorePackIndex())->GetPackHash();
            bool defaultEquipment = weapons[brotherWeaponSlot].packHash == coreHash && weapons[brotherWeaponSlot].ordinal == 0;
            for (unsigned slot = 0; slot < 3; ++slot) {
                if (brotherModel.armor[slot] == nullptr || PlayerArmorMultiplier(brotherModel, slot) != 1.0f) {
                    defaultEquipment = false;
                }
            }
            std::printf("[brother-equipment-check] gun=%s default=%d\n", weapons[brotherWeaponSlot].name.c_str(), defaultEquipment);
            if (!defaultEquipment) { return 1; }
        }
#endif

        scene.SetBrother(&brotherModel, &brother);
        const WeaponEntry *rifle = nullptr;
        for (const WeaponEntry &entry : weapons) {
            if (entry.packHash == brotherConfiguration.guns[1].packHash && entry.ordinal == brotherConfiguration.guns[1].localIndex) {
                rifle = &entry;
                break;
            }
        }
        if (rifle == nullptr) { return 1; }
        scene.SetBrotherWeapons(loaded.playerTemplate->script, weapons[brotherWeaponSlot].data, rifle->data);
        
#if GB_ENABLE_TESTS
if (check) {
            if (!scene.SwapBrotherWeapon() || scene.GetBrotherWeaponSlot() != 1 ||
                !scene.SwapBrotherWeapon() || scene.GetBrotherWeaponSlot() != 0) { return 1; }
            std::printf("[brother-equipment-check] pistol-rifle-pistol=1 player-unchanged=1\n");
        }
#endif

    }
    scene.SetPlayerProgress(&progress);
    SurvivalSession session(scene, loaded.map, enemies);
    CProfileManager researchProfile;
    CProfileManager *pickupProfile = nullptr;
    if (gameContext != nullptr) { pickupProfile = &gameContext->profile; }
    else { pickupProfile = &researchProfile; }
    PowerupScene powerups(toc, tables, player, vitals, scene, effects, *pickupProfile);
    if (!powerups.Init()) { return 1; }
#if GB_ENABLE_TESTS
    {
        const int result = CheckSurvivalPowerupInventory({
            powerupStudy, toc, researchProfile
        });
        if (result >= 0) { return result; }
    }
#endif

    session.SetPowerups(&powerups);
    PickupScene pickups(toc, tables, program, pickupProfile);
    if (!pickups.Init()) { return 1; }
    session.SetPickups(&pickups, &effects);
    const GameObjectRef *archiveLevel = nullptr;
    if (archiveMission != nullptr) { archiveLevel = &archiveMission->data.level; }
    else if (gameContext != nullptr && gameContext->profile.nativeArchive && !gameContext->tutorial && gameContext->planet < 4) {
        archiveLevel = &gameContext->profile.nativeArchive->survivalLevels[gameContext->planet];
    }
    session.SetDialogHud(&survivalHud);
#if GB_ENABLE_TESTS
    if (launch.debugMap != nullptr) { archiveLevel = &launch.debugMap->level; }
#endif
    if (!session.Load(toc, tables, toc.GetPack(packIndex)->GetPackHash(), mapIndex, archiveLevel, archiveMission != nullptr)) { return 1; }
    const bool horde = archiveMission != nullptr && archiveMission->data.type == 2;
    if (horde && gameContext != nullptr) {
        gameContext->mission = archiveMission->resource;
        gameContext->missionLevel = archiveMission->data.level;
    }
    if (horde) { session.SetHorde(true); }
    session.SetOriginalHud(&survivalHud);
    const float startX = loaded.players[0].x;
    const float startY = loaded.players[0].y;
    // The map PLAYER object's spawn angle; CBrother::Spawn :135887 writes the
    // same value to both brothers.
    const float startFacing = loaded.players[0].facingDegrees;
    session.SetStartWave(static_cast<int>(startWave));
    // The original seeds its one CRandGen from the clock (:370383), so a level
    // script's rolls differ every session. Real play does the same; research
    // runs keep the fixed default stream so their results stay comparable.
    if (!check && !bossStudy && capturePath.empty()) {
        session.SetScriptRandomSeed(static_cast<std::uint32_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    }
    const bool tutorial = gameContext != nullptr && gameContext->tutorial;
    session.GetLevel().EnableTutorial(tutorial);
    scene.SetMap(loaded.map, loaded.collisionScene, loaded.weaponCollision, kLevelCameraScale, kPlayerCollisionRadius);
    session.Restart(startX, startY, startFacing);
    std::uint64_t accountedXplodium = 0;
    int lastSavedWave = session.GetLevel().GetWave();
    bool savedDeath = false;
    // CMap::SetObjectLayer (:91935) activates one layer. Preview may combine
    // layers, but survival must not inherit deathmatch/campaign obstacles.
    // Correction: preload all layers, then OnStart activates only authored
    // layers in script order; previously spawned props survive layer switches.
    LoadProps(toc, loaded);
    BuildCollisionScene(loaded);
    MapPropWorld props(loaded, scene, session.GetLevel(), effects);
    session.SetProps(&props);
    scene.SetProps(&props);
    session.Restart(startX, startY, startFacing);
    loading.Finish();
#if GB_ENABLE_TESTS
    if (development->debugMapProfileCheck) {
        if (gameContext == nullptr) { return 1; }
        return CheckDebugMapProfile(tables, player, progress, *gameContext, scene, session.GetLevel());
    }
    if (development->campaignDoorCheck) { return CheckCampaignDoorPassage(loaded, scene, session); }
    if (development->campaignTargetCheck) { return CheckCampaignTargets(loaded, scene, session); }
    if (development->campaignProgressionCheck) { return CheckCampaignProgression(loaded, scene, session, mapIndex); }
    if (development->campaignRescueCheck) { return CheckCampaignRescue(loaded, scene, session); }
    if (development->campaignPortalCheck) { return CheckCampaignPortal(loaded, scene, session); }
    if (development->campaignCacheCheck) { return CheckCampaignCache(loaded, scene, session, pickups); }
    {
        const int result = CheckSurvivalDeath({
            checkFailures, packShortName, deathStudy, vitals, window, program, batch, loaded, player, effects, scene, brother, brotherModel, session, startX, startY, startFacing
        });
        if (result >= 0) { return result; }
    }
#endif

    if (!loading.IsValid()) { return 1; }
    if (loading.Cancelled()) { return 0; }
    // CGunBros::OnLoaded :79321: battle music begins only after game binding.
    music.SetPaused(false);
    music.SetVolume(1.0f);
    if (!music.NextTrack()) { return 1; }
#if GB_ENABLE_TESTS
    {
        const int result = CheckSurvivalBoss({
            checkFailures, packShortName, bossStudy, toc, tables, enemies, vitals, window, program, loaded, player, scene, session, startX, startY, startFacing
        });
        if (result >= 0) { return result; }
    }
#endif

#if GB_ENABLE_TESTS
    {
        const int result = CheckSurvivalFeedback({
            checkFailures, capturePath, feedbackStudy, toc, tables, weapons, enemies, vitals, survivalHud, program, loaded, player, scene, session, props, startX, startY, startFacing
        });
        if (result >= 0) { return result; }
    }
#endif

#if GB_ENABLE_TESTS
    {
        const int result = CheckSurvivalLevelSounds({
            checkFailures, check, session
        });
        if (result >= 0) { return result; }
    }
#endif

#if GB_ENABLE_TESTS
    {
        const int result = CheckSurvivalPropRoutes({
            checkFailures, check, props
        });
        if (result >= 0) { return result; }
    }
#endif

#if GB_ENABLE_TESTS
    {
        const int result = CheckSurvivalTriggerRoutes({
            checkFailures, check, session, startX, startY, startFacing
        });
        if (result >= 0) { return result; }
    }
#endif

#if GB_ENABLE_TESTS
    {
        const int result = CheckSurvivalPlacedProps({
            check, loaded
        });
        if (result >= 0) { return result; }
    }
#endif

#if GB_ENABLE_TESTS
    {
        const int result = CheckSurvivalBrotherPose({
            checkFailures, check, withBrother, scene, brother, brotherModel
        });
        if (result >= 0) { return result; }
    }
#endif

#if GB_ENABLE_TESTS
    {
        const int result = CheckSurvivalTutorial({
            checkFailures, capturePath, check, gameContext, tables, weapons, vitals, progress, program, loaded, player, weaponSlot, equippedWeaponSlot, scene, brother, session, powerups, pickups, pickupProfile, tutorial, accountedXplodium
        });
        if (result >= 0) { return result; }
    }
#endif

#if GB_ENABLE_TESTS
    {
        const int result = CheckSurvivalWaves({
            checkFailures, capturePath, packShortName, mapIndex, check, checkWaves, startWave, gameContext, withBrother, powerupStudy, archiveMission, toc, tables, weapons, enemies, vitals, progress, window, program, loaded, player, weaponSlot, effects, scene, brother, brotherModel, session, pickups, props, tutorial, startX, startY, startFacing, packIndex, archiveLevel
        });
        if (result >= 0) { return result; }
    }
#endif

#if GB_ENABLE_TESTS
    {
        const int result = CheckSurvivalHorde({
            checkFailures, capturePath, check, startWave, vitals, loaded, scene, session, horde
        });
        if (result >= 0) { return result; }
    }
#endif

#if GB_ENABLE_TESTS
    {
        const int result = CheckSurvivalCampaign({
            checkFailures, capturePath, packShortName, mapIndex, check, archiveMission, vitals, loaded, scene, session, pickups, horde
        });
        if (result >= 0) { return result; }
    }
#endif

    for (unsigned elapsed = 0; elapsed < advanceMs; elapsed += 16) {
        session.Update(16, 0, 0, firePreview);
        AdvanceProps(loaded.props, 16);
        AdvanceTileLayers(loaded.map, 16);
    }
#if GB_ENABLE_TESTS
    {
        const int result = CheckSurvivalPowerupCapture({
            capturePath, powerupStudy, scene, powerups
        });
        if (result >= 0) { return result; }
    }
#endif

    bool paused = false;
    int lastSavedTutorialStep = session.GetLevel().GetTutorialStep();
    bool shopOpen = false, itemChoice = false;
    GameObjectRef leftPowerup = powerups.GetEquipped(0);
    GameObjectRef rightPowerup = powerups.GetEquipped(1);
#if GB_ENABLE_TESTS
    const bool checkControls = gameContext != nullptr && gameContext->checkControls;
#else
    constexpr bool checkControls = false;
#endif
    unsigned controlFrame = 0;
    // The keyboard fixture needs owned charges; selector purchases/equipping
    // are separately exercised by RunOriginalPowerupSelectorCheck.
    
#if GB_ENABLE_TESTS
if (checkControls) { pickupProfile->AddPowerup(rightPowerup, 2); }
#endif

    const std::uint64_t controlsInitialBucks = pickupProfile->warbucks;
    const bool controlsInitialSound = pickupProfile->soundEnabled;
    const unsigned controlsInitialGrenades = pickupProfile->GetPowerupCount(rightPowerup);
    const GameObjectRef controlsInitialLeft = leftPowerup;
    // Run the real HUD hit tests and the same host action path. No OS input.
    constexpr SurvivalHudAction controlActions[] = {SurvivalHudAction::OpenShop, SurvivalHudAction::CloseShop,
        SurvivalHudAction::SwapWeapon, SurvivalHudAction::Pause, SurvivalHudAction::Resume};
    constexpr unsigned controlClickCount = sizeof(controlActions) / sizeof(controlActions[0]);
    // Test through the production key dispatcher after the mouse regression.
    const KeyCode controlKeys[] = {KeyCode::Digit1, KeyCode::Escape, KeyCode::Digit2,
        KeyCode::Q, KeyCode::None, KeyCode::E, KeyCode::F, KeyCode::R};
    const unsigned controlKeyCount = sizeof(controlKeys) / sizeof(controlKeys[0]);
    unsigned controlsBeforeKeys = 0;
    unsigned controlsLeftBeforeKeys = 0;
    // Presentation reads a snapshot; its actions re-enter the same keyboard path.
    const auto buildHudState = [&]() {
        SurvivalHudState state;
        state.health = vitals.health;
        state.maximumHealth = vitals.maximum;
        state.brotherHealth = brother.vitals.health;
        state.brotherMaximumHealth = brother.vitals.maximum;
        state.withBrother = withBrother;
        state.wave = std::min(session.GetLevel().GetWave(), session.GetLevel().GetWaveLimit() - 1);
        state.horde = horde;
        state.score = scene.GetScore();
        state.killStreak = scene.GetKillStreak();
        state.stopwatchMs = session.GetLevel().GetStopwatchTime();
        state.bossIntroSerial = session.GetLevel().GetBossIntroSerial();
        state.xplodiumMultiplier = session.GetLevel().GetXplodiumMultiplierPercent();
        state.level = progress.GetLevel();
        state.experience = progress.GetExperienceInLevel();
        state.experienceDelta = progress.GetExperienceDelta();
        state.xplodium = scene.GetXplodium();
        state.kills = scene.GetTotalKills();
        state.enemies = session.CountEnemies();
        state.weaponSlot = equippedWeaponSlot;
        state.swapKeyDown = window.IsKeyDown(KeyCode::Digit2) || window.IsKeyDown(KeyCode::N) || window.IsKeyDown(KeyCode::M);
        state.weapon = weapons[weaponSlot].name;
        if (gameContext != nullptr) {
            state.guns[0] = gameContext->profile.configuration.guns[0];
            state.guns[1] = gameContext->profile.configuration.guns[1];
        } else {
            state.guns[0].packHash = weapons[weaponSlot].packHash;
            state.guns[0].localIndex = static_cast<std::uint8_t>(weapons[weaponSlot].ordinal);
        }
        state.paused = paused;
        state.shopOpen = shopOpen;
        state.itemChoice = itemChoice;
        state.leftPowerup = leftPowerup;
        state.rightPowerup = rightPowerup;
        state.leftCount = pickupProfile->GetPowerupCount(leftPowerup);
        state.rightCount = pickupProfile->GetPowerupCount(rightPowerup);
        state.inventory = pickupProfile->powerups;
        state.coins = pickupProfile->coins;
        state.warbucks = pickupProfile->warbucks;
        state.soundEnabled = pickupProfile->soundEnabled;
        state.musicEnabled = pickupProfile->musicEnabled;
        state.originalUi = true;
        state.dockedSticks = pickupProfile->options.DockedSticks();
        state.powerupStatus.healthPercent = static_cast<int>(std::lround(vitals.health * 100 / vitals.maximum));
        state.powerupStatus.shield = player.weapon->brother.IsShield();
        state.powerupStatus.frenzy = player.weapon->brother.IsFrenzy();
        state.powerupStatus.autoFire = player.weapon->brother.IsAutoFire();
        state.powerupStatus.turret = player.weapon->brother.IsTurretActive();
        for (unsigned type = 0; type < 3; ++type) { state.powerupStatus.frenzyTypes[type] = player.weapon->brother.IsFrenzyType(type); }
        state.dead = vitals.dead;
        state.inputHidden = vitals.inputHidden;
        state.cleared = session.GetLevel().IsCleared();
        state.transitioning = session.IsTransitioning();
        state.transitionTime = session.GetTransitionElapsed();
        state.perfectBonus = scene.GetLastWaveBonus();
        state.damageHits = vitals.hits;
        PopulateSurvivalDebugInfo(state, scene, effects, packShortName, mapIndex, showCollisions);
        state.dialog = session.GetDialogText();
        state.tutorialStep = session.GetLevel().GetTutorialStep();
        if (archiveMission != nullptr) { state.mission = archiveMission->title; }
        if (powerups.GetSelected() != nullptr) {
            state.item = powerups.GetSelected()->name;
            state.itemCount = powerups.GetCount();
        }
        PopulateDebugBuffs(state, player);
        return state;
    };
    int accumulator = 0;
    std::uint64_t previous = window.GetTicksMs();
    Camera camera;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    std::printf("[survival] WASD move, mouse aim/fire, Q/E powerups, 1 shop, 2 swap weapon, Esc/space pause\n");
    auto menuTicks = window.GetTicksMs();

#if GB_ENABLE_TESTS
    // Deterministic 1,200 rendered frames, with real waves, AI and projectiles.
    std::unique_ptr<ISurvivalInputDriver> performancePilot;
    std::ofstream performanceReport;
    std::vector<double> performanceCpu;
    unsigned performanceFrame = 0;
#endif
    
#if GB_ENABLE_TESTS
if (performanceStudy) {
        performancePilot = CreateSurvivalInputDriver(scene, loaded.map.GetVisibleBounds());
        if (!performancePilot) { return 1; }
        vitals.invincible = true;
        if (!window.SetVSync(false)) { return 1; }
        std::printf("[performance] vsync=0 fixed-step=16ms rendered-frames=1200\n");
        performanceReport.open(TestOutput::Path("performance-frames.csv"));
        performanceReport << "frame,update_ms,geometry_ms,world_ms,hud_ms,present_ms,alive,spawned\n";
    }
#endif

    while (window.PumpEvents()) {
        const auto performanceStart = std::chrono::steady_clock::now();
        const auto frameTicks = window.GetTicksMs();
        survivalHud.AdvanceMenu(static_cast<unsigned>(frameTicks - menuTicks));
        menuTicks = frameTicks;
        
#if GB_ENABLE_CHEATS
        for (std::string cheat = window.TakeCheatCode(); !cheat.empty(); cheat = window.TakeCheatCode()) {
            CombatCheatResult result;
            if (!ApplyCombatCheat(cheat, scene, vitals, powerups, session, gameContext, result)) { return 1; }
            if (result.resume) { paused = false; shopOpen = false; itemChoice = false; }
            if (result.resetClock) {
                effects.SetPaused(paused || shopOpen);
                accumulator = 0;
                previous = window.GetTicksMs();
            }
        }
#endif

        int inputWidth = 0, inputHeight = 0;
        window.GetDrawableSize(inputWidth, inputHeight);
        float inputX = -1, inputY = -1;
        window.GetMousePosition(inputX, inputY);
        inputX *= 1024.0f / std::max(1, inputWidth);
        inputY *= 768.0f / std::max(1, inputHeight);
        const SurvivalHudState inputState = buildHudState();
        float menuScroll = window.TakeWheelDelta();
        int menuDragX = 0, menuDragY = 0;
        window.TakeDragDelta(menuDragX, menuDragY);
        survivalHud.ScrollMenuInput(inputState, menuScroll,
            float(menuDragX) * 1024 / inputWidth, float(menuDragY) * 768 / inputHeight);
        bool pointerDown = window.IsLeftMouseDown();
        
#if GB_ENABLE_TESTS
if (checkControls && controlFrame < controlClickCount) {
            // Bind and finish authored menu entrance before querying its live hitbox.
            if (!survivalHud.Draw(inputState)) { return 1; }
            survivalHud.AdvanceMenu(2000);
            if (!survivalHud.Draw(inputState)) { return 1; }
            MovieRegion target;
            if (!survivalHud.FindActionRegion(inputState, controlActions[controlFrame], target)) {
                std::printf("[combat-controls-check] missing original action=%d frame=%u\n", int(controlActions[controlFrame]), controlFrame);
                return 1;
            }
            inputX = target.x + target.width / 2;
            inputY = target.y + target.height / 2;
            survivalHud.Pointer(inputState, inputX, inputY, false);
            pointerDown = true;
        }
#endif

        const bool hudOwnsPointer = survivalHud.CapturesPointer(inputState, inputX, inputY);
        SurvivalHudAction action = survivalHud.Pointer(inputState, inputX, inputY, pointerDown);
        // Input-pad controls cannot interrupt the active powerup presentation.
        if (powerups.IsMovieActive()) { action = SurvivalHudAction::None; }
        if (action == SurvivalHudAction::Exit) {
            // Surrender leaves a paused menu; the same BGM continues into results.
            music.SetPaused(false);
            music.SetVolume(1.0f);
            break;
        }
        if (action == SurvivalHudAction::OpenShop) { shopOpen = true; itemChoice = false; accumulator = 0; }
        if (action == SurvivalHudAction::CloseShop) { shopOpen = false; itemChoice = false; }
        if (action == SurvivalHudAction::CancelItem) { itemChoice = false; }
        const StoreEntry *shopItem = survivalHud.SelectedItem();
        if (shopItem != nullptr && (action == SurvivalHudAction::BuyItem || action == SurvivalHudAction::SelectItem)) {
            const GameObjectRef &resource = shopItem->data.objects.front().object;
            if (action == SurvivalHudAction::BuyItem) {
                const PurchaseResult result = pickupProfile->AcquireItem(shopItem->data, progress.GetLevel());
                if (result == PurchaseResult::Purchased && gameContext != nullptr && !gameContext->SaveProfile()) { return 1; }
                // CPowerUpSelector::OnPurchase :184861 updates quantity in place.
                // An icon tap, not a purchase, enters the use/equip selection state.
                itemChoice = false;
                survivalHud.ReportSelectorPurchase(result, inputState);
            } else {
                itemChoice = pickupProfile->GetPowerupCount(resource) > 0;
            }
        }
        if (shopItem != nullptr && itemChoice && (action == SurvivalHudAction::EquipLeft || action == SurvivalHudAction::EquipRight || action == SurvivalHudAction::UseNow)) {
            const GameObjectRef &resource = shopItem->data.objects.front().object;
            if (action == SurvivalHudAction::EquipLeft || action == SurvivalHudAction::EquipRight) {
                unsigned slot = 0;
                if (action == SurvivalHudAction::EquipRight) { slot = 1; }
                if (powerups.Equip(slot, resource)) {
                    leftPowerup = powerups.GetEquipped(0);
                    rightPowerup = powerups.GetEquipped(1);
                    itemChoice = false;
                    if (gameContext != nullptr && !gameContext->SaveProfile()) { return 1; }
                }
            }
            if (action == SurvivalHudAction::UseNow) {
                if (powerups.SelectResource(resource) && powerups.Use(true)) { shopOpen = false; itemChoice = false; }
            }
        }
        if (action == SurvivalHudAction::UseLeft && !paused && !shopOpen && !session.IsTransitioning()) {
            if (powerups.SelectResource(leftPowerup)) { powerups.Use(); }
        }
        if (action == SurvivalHudAction::Sound || action == SurvivalHudAction::Music || action == SurvivalHudAction::DockedSticks) {
            if (action == SurvivalHudAction::Sound) { pickupProfile->soundEnabled = !pickupProfile->soundEnabled; }
            if (action == SurvivalHudAction::Music) { pickupProfile->musicEnabled = !pickupProfile->musicEnabled; }
            if (action == SurvivalHudAction::DockedSticks) { pickupProfile->options.ToggleDockedSticks(); }
            music.SetEnabled(pickupProfile->musicEnabled);
            CAudioPlayer::SetEffectsEnabled(pickupProfile->soundEnabled);
            if (gameContext != nullptr && !gameContext->SaveProfile()) { return 1; }
        }
        KeyCode pointerKey = KeyCode::None;
        if (action == SurvivalHudAction::Pause || action == SurvivalHudAction::Resume || action == SurvivalHudAction::Continue) { pointerKey = KeyCode::Space; }
        if (action == SurvivalHudAction::Retry) { pointerKey = KeyCode::R; }
        if (action == SurvivalHudAction::UseItem) { pointerKey = KeyCode::G; }
        if (action == SurvivalHudAction::NextItem) { pointerKey = KeyCode::F; }
        if (action == SurvivalHudAction::Weapon1) { pointerKey = KeyCode::Digit1; }
        if (action == SurvivalHudAction::Weapon2) { pointerKey = KeyCode::Digit2; }
        if (action == SurvivalHudAction::SwapWeapon) { pointerKey = KeyCode::Digit2; }
        std::vector<KeyCode> inputs;
        if (pointerKey != KeyCode::None) { inputs.push_back(pointerKey); }
        for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None; key = window.TakeKeyPress()) {
            if (HandleDebugKey(key, window, showCollisions)) { continue; }
#if GB_ENABLE_TESTS
            if (launch.debugSelection != nullptr && GameDebugKeys::OpensMapBrowser(key, window)) {
                music.SetPaused(true);
                const bool selected = ShowDebugMapPicker(toc, tables, window, *launch.debugSelection);
                music.SetPaused(paused);
                previous = window.GetTicksMs();
                menuTicks = previous;
                accumulator = 0;
                inputs.clear();
                if (selected) {
                    if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel(), accountedXplodium)) { return 1; }
                    return kDebugMapSessionChoice;
                }
                continue;
            }
            if (launch.debugMap != nullptr && key == GameDebugKeys::MapBack) { return 0; }
#endif
            AppendSurvivalShortcut(inputs, key);
        }
        const int controlsWaveBeforeInput = session.GetLevel().GetWave();
        
#if GB_ENABLE_TESTS
if (checkControls && controlFrame >= controlClickCount && controlFrame < controlClickCount + controlKeyCount) {
            const unsigned step = controlFrame - controlClickCount;
            if (step == 0 || step == 4) {
                vitals.invincible = true;
                // Wait through real intro/cooldown updates; do not bypass Use().
                for (unsigned tick = 0; tick < 250; ++tick) { session.Update(16, 0, 0, false); }
            }
            if (step == 0) {
                // Distinct slots catch accidental Q -> right-item routing.
                leftPowerup = controlsInitialLeft;
                // The isolated legacy test account starts without this item.
                pickupProfile->AddPowerup(leftPowerup, 1);
                controlsLeftBeforeKeys = pickupProfile->GetPowerupCount(leftPowerup);
                controlsBeforeKeys = pickupProfile->GetPowerupCount(rightPowerup);
                vitals.health = 1;
            }
            AppendSurvivalShortcut(inputs, controlKeys[step]);
        }
#endif

        for (KeyCode key : inputs) {
            if (powerups.IsMovieActive()) { continue; }
            // The original death script hides the input pad. Do not open an
            // invisible pause menu while the formal death animation is running.
            if (vitals.dead && gameContext != nullptr) { continue; }
            if (shopOpen) {
                if (key == KeyCode::Space || key == KeyCode::Escape) {
                    if (survivalHud.BackFromSelectorPrompt()) { continue; }
                    if (itemChoice) { itemChoice = false; }
                    else { shopOpen = false; }
                }
                continue;
            }
            if (key == KeyCode::Space || key == KeyCode::Escape) {
                if (!paused || !survivalHud.BackFromHelp()) { paused = !paused; }
            }
            if ((key == KeyCode::Q || key == KeyCode::E || key == KeyCode::G) &&
                !paused && !vitals.dead && !session.IsTransitioning()) {
                GameObjectRef item = rightPowerup;
                if (key == KeyCode::Q) { item = leftPowerup; }
                if (powerups.SelectResource(item)) { powerups.Use(); }
                continue;
            }
            if (key == KeyCode::Digit1 && gameContext != nullptr && !paused && !vitals.dead && !session.IsTransitioning()) {
                shopOpen = true;
                itemChoice = false;
                accumulator = 0;
                continue;
            }
            if (key == KeyCode::F) {
                powerups.SelectResource(rightPowerup);
                powerups.Cycle();
                if (powerups.GetSelected() != nullptr) { rightPowerup = powerups.GetSelected()->resource; }
            }
            if (key == KeyCode::R) {
                pendingWeapon = weapons.size();
                swapEventAccepted = false;
                if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel(), accountedXplodium)) { return 1; }
                session.Restart(startX, startY, startFacing);
                if (gameContext != nullptr) { gameContext->accountedKills = 0; gameContext->accountedWeaponExperience.clear(); }
                if (!session.HasOriginalHud()) { survivalHud.ResetNotices(); }
                savedDeath = false;
                paused = false;
                shopOpen = false;
                itemChoice = false;
            }
            std::size_t nextWeapon = weaponSlot;
            if (gameContext == nullptr) {
                KeyCode weaponKey = key;
                nextWeapon = SelectWeaponKey(weapons, weaponSlot, weaponKey);
            }
            else if (!paused && !vitals.dead && pendingWeapon >= weapons.size() &&
                (key == KeyCode::Digit2 || key == KeyCode::N || key == KeyCode::M)) {
                pendingEquippedSlot = 1 - equippedWeaponSlot;
                const GameObjectRef &ref = gameContext->profile.configuration.guns[pendingEquippedSlot];
                for (std::size_t index = 0; index < weapons.size(); ++index) {
                    if (weapons[index].packHash == ref.packHash && weapons[index].ordinal == ref.localIndex) { nextWeapon = index; break; }
                }
            }
            if (nextWeapon != weaponSlot && !vitals.dead) {
                if (gameContext != nullptr) {
                    // Load both authored guns before starting the lowering move.
                    // Keep the brother, torso controller, health and effects alive.
                    if (player.uiOtherWeapon == nullptr) {
                        primaryEquippedSlot = equippedWeaponSlot;
                        if (!PreparePlayerUIWeapon(tables, weapons[nextWeapon].data, weapons[nextWeapon].owner, player) ||
                            !CreatePlayerBuffers(player, program)) { return 1; }
                    }
                    pendingWeapon = nextWeapon;
                    swapEventAccepted = false;
                    
#if GB_ENABLE_TESTS
if (checkControls) {
                        std::printf("[combat-swap-check] queued old=%zu requested=%zu unchanged=1\n", weaponSlot, pendingWeapon);
                    }
#endif

                    continue;
                }
                if (!EquipControlledPlayer(tables, loaded, program, weapons[nextWeapon])) { return 1; }
                effects.RetireOwner(kPlayerCombatId);
                weaponSlot = nextWeapon;
            }
            if (gameContext != nullptr && !vitals.dead) { gameContext->profile.activeWeaponSlot = equippedWeaponSlot; }
        }
        
#if GB_ENABLE_TESTS
if (checkControls && (controlFrame == controlClickCount + 3 || controlFrame == controlClickCount + 5)) {
            // Throwing consumes inventory at the authored animation event.
            for (unsigned tick = 0; tick < 60; ++tick) { session.Update(16, 0, 0, false); }
        }
#endif

        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        loaded.players[0].x = scene.playerX;
        loaded.players[0].y = scene.playerY;
        const float baselineZoom = GameViewCameraZoom(width, height);
        camera.zoom = baselineZoom * loaded.map.GetCamera().GetScale() / kLevelCameraScale;
        session.SetViewSize(width / baselineZoom, height / baselineZoom);
        FollowPlayerCamera(loaded, width, height, camera);
        scene.SetViewCenter(camera.x + width / camera.zoom * 0.5f, camera.y + height / camera.zoom * 0.5f);
        scene.SetTextView(camera.x, camera.y, camera.zoom * 1024 / width, camera.zoom * 768 / height);
        float mouseX = 0, mouseY = 0;
        if (capturePath.empty() && window.GetMousePosition(mouseX, mouseY) && !vitals.dead && !powerups.IsMovieActive()) {
            scene.facing = std::atan2(camera.y + mouseY / camera.zoom - scene.playerY,
                camera.x + mouseX / camera.zoom - scene.playerX) * kRadiansToDegrees + 90;
        }
        float moveX = 0, moveY = 0;
        if (window.IsKeyDown(KeyCode::A)) { --moveX; }
        if (window.IsKeyDown(KeyCode::D)) { ++moveX; }
        if (window.IsKeyDown(KeyCode::W)) { --moveY; }
        if (window.IsKeyDown(KeyCode::S)) { ++moveY; }
        const std::uint64_t now = window.GetTicksMs();
        if (!paused && !shopOpen && capturePath.empty()) { accumulator += static_cast<int>(std::min<std::uint64_t>(now - previous, 100)); }
        previous = now;
        
#if GB_ENABLE_TESTS
if (performanceStudy) {
            accumulator = 16;
            performancePilot->Update(16, moveX, moveY);
        }
#endif

        
#if GB_ENABLE_TESTS
if (checkControls && !paused && !shopOpen) { accumulator = 960; }
#endif

        effects.SetPaused(paused || shopOpen);
        // CGunBros::OnSuspend :78263 lowers BGM to half without stopping it.
        // Gameplay and effects stay suspended; the music stream keeps advancing.
        float musicScale = 1.0f;
        if (paused || shopOpen) { musicScale = 0.5f; }
        music.SetVolume(musicScale);
        music.Update();
        
#if GB_ENABLE_TESTS
if (checkControls && controlFrame < controlClickCount + controlKeyCount) {
            const auto playback = music.GetPlaybackState();
            float expectedVolume = 0;
            if (pickupProfile->musicEnabled) {
                expectedVolume = 0.3f;
                if (paused || shopOpen) { expectedVolume *= 0.5f; }
            }
            if (playback.paused || std::abs(playback.volume - expectedVolume) > 0.001f) { ++checkFailures; }
            std::printf("[pause-bgm-check] frame=%u menu=%d paused-stream=%d gain=%.2f expected=%.2f failures=%u\n",
                controlFrame, paused || shopOpen, playback.paused, playback.volume, expectedVolume, checkFailures);
        }
#endif

        const std::size_t shotsBeforeSwap = effects.GetShotCount();
        const bool checkSwapFiring = checkControls && (controlFrame == 2 || controlFrame == controlClickCount + 2);
        while (accumulator >= 16) {
            if (powerups.IsMovieActive()) {
                session.Update(16, 0, 0, false);
                accumulator -= 16;
                continue;
            }
            if (!session.HasOriginalHud()) { survivalHud.Advance(16); }
            if (!vitals.dead && pendingWeapon < weapons.size() && !swapEventAccepted) {
                SetPlayerInput(player, false, false);
                swapEventAccepted = player.weapon->brother.OnSwapGun();
            }
            if (!vitals.dead) {
                const bool shoot = pendingWeapon >= weapons.size() &&
                    (performanceStudy || firePreview || checkSwapFiring || (window.IsLeftMouseDown() && !hudOwnsPointer));
                session.Update(16, moveX, moveY, shoot);
            }
            else {
                // CLevel::UpdateAfterDeath keeps an already-used powerup alive.
                session.UpdateAfterDeath(16);
            }
            if (pendingWeapon < weapons.size() && player.weapon->brother.TakeWeaponSwap()) {
                effects.RetireOwner(kPlayerCombatId);
                const auto &torso = player.weapon->brother.GetTorso().GetAnimation();
                const CMesh *outgoingMesh = torso.GetMesh();
                const int outgoingTime = torso.GetTimeMs();
                SelectPlayerUIWeapon(player, pendingEquippedSlot == primaryEquippedSlot);
                
#if GB_ENABLE_TESTS
if (checkControls && (torso.GetMesh() != outgoingMesh || torso.GetTimeMs() != outgoingTime)) { ++checkFailures; }
#endif

                weaponSlot = pendingWeapon;
                equippedWeaponSlot = pendingEquippedSlot;
                player.gunResource.packHash = weapons[weaponSlot].packHash;
                player.gunResource.localIndex = static_cast<std::uint8_t>(weapons[weaponSlot].ordinal);
                player.masteryExperience = gameContext->profile.GetWeaponExperience(player.gunResource);
                player.ActiveWeapon().gun.SetMasteryExperience(player.masteryExperience);
                gameContext->profile.activeWeaponSlot = equippedWeaponSlot;
                pendingWeapon = weapons.size();
                ++combatSwapEvents;
                
#if GB_ENABLE_TESTS
if (checkControls) {
                    std::printf("[combat-swap-check] native-event=%u slot=%u torso-preserved=1\n", combatSwapEvents, equippedWeaponSlot);
                }
#endif

            }
            const int worldDeltaMs = session.GetLevel().TransformWorldElapseMS(16);
            AdvanceProps(loaded.props, worldDeltaMs);
            AdvanceTileLayers(loaded.map, worldDeltaMs);
            accumulator -= 16;
        }
        
#if GB_ENABLE_TESTS
if (checkSwapFiring) {
            const std::size_t shots = effects.GetShotCount() - shotsBeforeSwap;
            if (shots == 0) { ++checkFailures; }
            std::printf("[combat-swap-check] fire-after-switch=%zu failures=%u\n", shots, checkFailures);
        }
#endif

        if (session.GetLevel().GetWave() != lastSavedWave || (session.IsDeathComplete() && !savedDeath) ||
            session.GetLevel().GetTutorialStep() != lastSavedTutorialStep) {
            if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel(), accountedXplodium, session.IsDeathComplete())) { return 1; }
            lastSavedWave = session.GetLevel().GetWave();
            savedDeath = session.IsDeathComplete();
            lastSavedTutorialStep = session.GetLevel().GetTutorialStep();
        }
        // Gameplay death opens the original postgame flow; research keeps its
        // death/restart controls so existing isolated checks remain available.
#if GB_ENABLE_TESTS
        // The archive browser owns preview wrap-up instead of the retail menu.
        if (launch.debugMap != nullptr && session.GetLevel().IsCleared()) { return kDebugMapSessionComplete; }
#endif
        if (session.IsFinished() && gameContext != nullptr && !check && capturePath.empty()) { break; }
        loaded.players[0].x = scene.playerX;
        loaded.players[0].y = scene.playerY;
        loaded.players[0].facingDegrees = scene.facing;
        camera.zoom = baselineZoom * loaded.map.GetCamera().GetScale() / kLevelCameraScale;
        FollowPlayerCamera(loaded, width, height, camera);
        const auto performanceUpdated = std::chrono::steady_clock::now();
        BuildGeometry(loaded, batch, true, true, false);
        const auto performanceGeometry = std::chrono::steady_clock::now();
        glViewport(0, 0, width, height);
        glClearColor(0.04f, 0.05f, 0.07f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        float mvp[kMatrix4dElements];
        Matrix4dOrthoTopLeft(width / camera.zoom, height / camera.zoom, kMapDepthRange, mvp);
        Matrix4dTranslate(mvp, -camera.x, -camera.y);
        batch.Draw(program, mvp);
        pickups.Draw(mvp, kLevelCameraScale);
        effects.Draw(mvp, nullptr, kLevelCameraScale, WeaponDrawPass::BehindPlayer);
        // Historical explanation of the old separate model pass:
        // The AI brother is a 3D model like the player and the enemies: with no
        // depth test his torso, legs and gun paint over each other in submission
        // order and the model's dark inside covers its front -- the black
        // speckles. DrawModels already cleared depth and drew the player, so this
        // shares the same depth buffer.
        // Correction: the shared queue now clears depth per model and sorts
        // BOTH brothers and enemies among props, preserving internal depth.
        PlayerModel *drawBrother = nullptr;
        if (withBrother) {
            drawBrother = &brotherModel;
        }
        DrawMapObjects(loaded, batch, program, mvp, true, &scene, drawBrother, brother.y, width);
        effects.Draw(mvp, nullptr, kLevelCameraScale, WeaponDrawPass::InFrontOfPlayer);
        
#if GB_ENABLE_TESTS
if (check) {
            GLint sourceBlend = 0, destinationBlend = 0;
            glGetIntegerv(GL_BLEND_SRC, &sourceBlend);
            glGetIntegerv(GL_BLEND_DST, &destinationBlend);
            if (sourceBlend != GL_SRC_ALPHA || destinationBlend != GL_ONE_MINUS_SRC_ALPHA) { ++checkFailures; }
            std::printf("[render-check] after-particles blend=%x/%x failures=%u\n", sourceBlend, destinationBlend, checkFailures);
        }
#endif

        if (showCollisions) {
            const CBrotherAI *collisionBrother = nullptr;
            if (withBrother) { collisionBrother = &brother; }
            DrawCollisionOverlay(markers, markerProgram, mvp, 1 / camera.zoom, &loaded, &scene, collisionBrother, &effects);
        }
        const auto performanceWorld = std::chrono::steady_clock::now();
        SurvivalHudState hudState = buildHudState();
        // CCamera constructor :64622: viewport factor is independent of zoom.
        const float healthBarViewportScale = std::min(width / 480.0f, height / 320.0f);
        hudState.enemyHealthBars = scene.EnemyHealthBars(healthBarViewportScale);
        ProjectEnemyHealthBars(hudState.enemyHealthBars, camera.x, camera.y, camera.zoom, width, height);
        hudState.indicators = session.GetLevel().GetIndicators();
        for (CLevelIndicator &indicator : hudState.indicators) {
            indicator.x = (indicator.x - camera.x) * camera.zoom * 1024 / width;
            indicator.y = (indicator.y - camera.y) * camera.zoom * 768 / height;
        }
        if (withBrother) {
            // CLevel::Bind :121881 leaves this empty for the default local
            // partner; only a selected friend or multiplayer peer supplies a name.
            // CLevel::DrawBrotherLabel :120351 anchors three collision radii
            // above the AI's world position and follows the level's alpha.
            hudState.brotherLabelX = (brother.x - camera.x) * camera.zoom * 1024 / width;
            hudState.brotherLabelY = (brother.y - scene.GetPlayerRadius() * 3 - camera.y) * camera.zoom * 768 / height;
            hudState.brotherLabelAlpha = session.GetLevel().GetBrotherLabelAlpha();
        }

        hudState.playerX = scene.playerX;
        hudState.playerY = scene.playerY;
        hudState.damageDealt = scene.damageDealt;
        const float moveLength = std::max(1.0f, std::hypot(moveX, moveY));
        hudState.moveX = moveX / moveLength;
        hudState.moveY = moveY / moveLength;
        if (window.IsLeftMouseDown() && !hudOwnsPointer) {
            hudState.aimX = std::sin(scene.facing / kRadiansToDegrees);
            hudState.aimY = -std::cos(scene.facing / kRadiansToDegrees);
        }
        if (!survivalHud.DrawExperienceTexts(scene.GetExperienceTexts(), horde) || !survivalHud.Draw(hudState)) { return 1; }
        if (!powerups.DrawMovies()) { return 1; }
        
#if GB_ENABLE_TESTS
if (checkControls && controlFrame < controlClickCount) {
            if (controlFrame == 0 && !GB_SAVE_FRAME(window, TestOutput::Path("combat-controls-shop.png"))) { return 1; }
            if (controlFrame == 3 && !GB_SAVE_FRAME(window, TestOutput::Path("combat-controls-pause.png"))) { return 1; }
            if (controlFrame + 1 == controlClickCount) {
                const bool inventoryUnchanged = pickupProfile->warbucks == controlsInitialBucks &&
                    pickupProfile->GetPowerupCount(rightPowerup) == controlsInitialGrenades;
                const bool controlsPassed = inventoryUnchanged && equippedWeaponSlot == 1 &&
                    pickupProfile->soundEnabled == controlsInitialSound && !paused && !shopOpen;
                std::printf("[combat-controls-check] original-hitboxes=5 inventory-unchanged=%d weapon=%u resume=%d failures=%d\n",
                    inventoryUnchanged, equippedWeaponSlot, !paused && !shopOpen, !controlsPassed);
                if (!controlsPassed) { ++checkFailures; }
            }
        }
#endif

        
#if GB_ENABLE_TESTS
if (checkControls && controlFrame >= controlClickCount && controlFrame < controlClickCount + controlKeyCount) {
            const unsigned step = controlFrame - controlClickCount;
            bool passed = true;
            if (step == 0) { passed = shopOpen; }
            if (step == 1) { passed = !shopOpen && !paused; }
            if (step == 2) { passed = equippedWeaponSlot == 0 && combatSwapEvents == 2; }
            if (step == 3) { passed = equippedWeaponSlot == 0 && controlsLeftBeforeKeys > 0 &&
                pickupProfile->GetPowerupCount(leftPowerup) == controlsLeftBeforeKeys - 1 &&
                pickupProfile->GetPowerupCount(rightPowerup) == controlsBeforeKeys; }
            if (step == 5) { passed = equippedWeaponSlot == 0 && pickupProfile->GetPowerupCount(rightPowerup) == controlsBeforeKeys - 1; }
            if (step == 6 || step == 7) { passed = inputs.empty() && equippedWeaponSlot == 0 &&
                session.GetLevel().GetWave() == controlsWaveBeforeInput && rightPowerup.localIndex == 13; }
            std::printf("[shortcut-check] step=%u key=%d passed=%d inventory=%u\n", step,
                static_cast<int>(controlKeys[step]), passed, pickupProfile->GetPowerupCount(rightPowerup));
            if (!passed) { ++checkFailures; }
        }
#endif

        
#if GB_ENABLE_TESTS
if (checkControls && controlFrame < controlClickCount + controlKeyCount) {
            ++controlFrame;
            if (controlFrame < controlClickCount + controlKeyCount) { window.Present(); continue; }
        }
#endif

        
#if GB_ENABLE_TESTS
if (!capturePath.empty()) {
            if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel(), accountedXplodium, check && horde)) { return 1; }
            const unsigned errors = glGetError();
            if (errors != 0 || !GB_SAVE_FRAME(window, capturePath)) { return 1; }
            std::printf("[survival] wave=%d alive=%d spawned=%u kills=%u hp=%.1f\n",
                session.GetLevel().GetWave(), session.CountEnemies(), scene.spawned, session.GetKills(), vitals.health);
            window.Present();
            if (check && horde) {
                CRefinementManager::Template refinement;
                if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
                CProfileManager hordeProfile;
                hordeProfile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
                SurvivalGameContext record{hordeProfile, TestOutput::Path("horde-progress-check.dat")};
                record.hordeStart = static_cast<int>(startWave);
                std::uint64_t credited = 0;
                if (!SaveSurvivalProgress(&record, progress, scene, session.GetLevel(), credited) ||
                    !SaveSurvivalProgress(&record, progress, scene, session.GetLevel(), credited)) { return 1; }
                CProfileManager restored = hordeProfile;
                if (!restored.LoadFromDisk(record.savePath) || restored.hordeBestScore[startWave] != scene.GetScore() ||
                    restored.hordeBestKills[startWave] != scene.GetTotalKills() || restored.clearedWaves[0] != 0 ||
                    restored.enemyKills[0] != 0) { ++checkFailures; }
                const unsigned points = scene.GetScore();
                CombatHit damage;
                damage.ownerType = 1;
                damage.damage = 1;
                scene.ApplyHit(kPlayerCombatId, damage);
                if (scene.GetKillStreak() != 0 || scene.GetScore() != points) { ++checkFailures; }
                session.Restart(startX, startY, startFacing);
                if (scene.GetScore() != 0 || scene.GetKillStreak() != 0 || session.GetKills() != 0 ||
                    session.GetLevel().GetStopwatchTime() != 0 || session.GetLevel().GetObjectTimeScale() != 1) { ++checkFailures; }
                std::printf("[horde-check] points=%u saved=1 damage-resets-streak=1 restart=1 failures=%u\n", points, checkFailures);
            }
            if (checkFailures != 0) { return 1; }
            return 0;
        }
#endif

        const auto performanceHud = std::chrono::steady_clock::now();
        window.Present();
        
#if GB_ENABLE_TESTS
if (performanceStudy) {
            const auto performanceEnd = std::chrono::steady_clock::now();
            const double updateMs = std::chrono::duration<double, std::milli>(performanceUpdated - performanceStart).count();
            const double geometryMs = std::chrono::duration<double, std::milli>(performanceGeometry - performanceUpdated).count();
            const double worldMs = std::chrono::duration<double, std::milli>(performanceWorld - performanceGeometry).count();
            const double hudMs = std::chrono::duration<double, std::milli>(performanceHud - performanceWorld).count();
            const double presentMs = std::chrono::duration<double, std::milli>(performanceEnd - performanceHud).count();
            performanceReport << performanceFrame << ',' << updateMs << ',' << geometryMs << ',' << worldMs << ',' << hudMs << ',' << presentMs
                << ',' << scene.AliveCount() << ',' << scene.spawned << '\n';
            performanceCpu.push_back(updateMs + geometryMs + worldMs + hudMs);
            if (++performanceFrame % 300 == 0) {
                std::printf("[performance] frame=%u update=%.2f geometry=%.2f world=%.2f hud=%.2f present=%.2f alive=%zu\n",
                    performanceFrame, updateMs, geometryMs, worldMs, hudMs, presentMs, scene.AliveCount());
            }
            if (performanceFrame >= 1200) {
                std::sort(performanceCpu.begin(), performanceCpu.end());
                std::printf("[performance] cpu-p50=%.3f cpu-p95=%.3f cpu-p99=%.3f max=%.3f frames=%u kills=%u\n",
                    performanceCpu[600], performanceCpu[1140], performanceCpu[1188], performanceCpu.back(), performanceFrame, scene.GetTotalKills());
                std::printf("[performance] enemy-assets hits=%u misses=%u templates=%zu\n", scene.GetEnemyModelCache().hits,
                    scene.GetEnemyModelCache().misses, scene.GetEnemyModelCache().entries.size());
                break;
            }
        }
#endif

    }
    if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel(), accountedXplodium)) { return 1; }
    return 0;
}
int RunSurvival(const SurvivalLaunch &launch) {
#if GB_ENABLE_TESTS
    return RunSurvivalSession(launch, nullptr);
#else
    return RunSurvivalSession(launch);
#endif
}
