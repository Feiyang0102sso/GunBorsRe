#include "gun_bros_re/debug/SurvivalDebug.h"
#include "gun_bros_re/debug/FrameRateOverlay.h"
#include "gun_bros_re/debug/DebugKeys.h"
#include "gun_bros_re/cheats/CheatActions.h"
#include "gun_bros_re/debug/DebugMaps.h"
#include "gun_bros_re/gameplay/SurvivalRuntime.h"
#include "gun_bros_re/gameplay/BroAIDeathmatch.h"
#include "gun_bros_re/gameplay/DeathmatchBot.h"
#include "gun_bros_re/data/LocalBotFriend.h"
#include "gun_bros_re/gameplay/LiveShopSession.h"
#include "gun_bros_re/LocalOnlineServices.h"
#if GB_ENABLE_TESTS
#include "gameplay/SurvivalStudy.h"
#include "gameplay/PerformanceProbe.h"
#endif
#if GB_ENABLE_TESTS
#include "gameplay/SurvivalChecks.h"
#include "gameplay/CampaignDoorChecks.h"
#include "gameplay/DebugMapChecks.h"
#endif
#include "gun_bros_re/gameplay/MapWorldInternal.h"
#include <ctime>
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
    const bool withBrother = launch.withBrother || launch.localLive || launch.localBot || launch.deathmatch;
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
    std::vector<CMPMatch::Entry> matches;
    CMPMatch match;
    CPlayerConfiguration matchConfiguration;
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
        if (launch.deathmatch) {
            gameContext->accountedKills = 0;
            gameContext->accountedPeerKills = 0;
            gameContext->accountedPeerXplodium = 0;
        }
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
    LoadingScreen loading(window, loadingMovies, tables, loadingProfile, true, false, &music, launch.localLive, launch.deathmatch);
    if (!loading.IsValid()) { return 1; }
    if (!LoadWeaponCatalog(toc, tables, weapons) || !LoadEnemyCatalog(toc, tables, enemies) ||
        !LoadInitialPlayerHealth(toc, tables, vitals.maximum)) { return 1; }
    unsigned matchSeed = static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count());
#if GB_ENABLE_TESTS
    if (development->deathmatchCheck || !capturePath.empty()) { matchSeed = 42 + launch.matchIndex; }
#endif
    if (launch.deathmatch) {
        if (!LoadMPMatches(toc, tables, matches) || launch.matchIndex >= matches.size()) { return 1; }
        const auto &entry = matches[launch.matchIndex];
        match.Bind(entry.data, matchSeed);
        for (unsigned slot = 0; slot < 2; ++slot) {
            if (launch.loadout[slot] >= entry.guns.size()) { return 1; }
            matchConfiguration.guns[slot] = entry.guns[launch.loadout[slot]];
        }
    }
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
    player.cooperative = launch.localLive;
    player.deathmatch = launch.deathmatch;
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
        if (launch.deathmatch) { equippedWeaponSlot = 0; }
        GameObjectRef ref = gameContext->profile.configuration.guns[equippedWeaponSlot];
        if (launch.deathmatch) { ref = matchConfiguration.guns[0]; }
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
    if (launch.deathmatch && gameContext == nullptr) {
        for (std::size_t index = 0; index < weapons.size(); ++index) {
            if (weapons[index].packHash == matchConfiguration.guns[0].packHash && weapons[index].ordinal == matchConfiguration.guns[0].localIndex) { weaponSlot = index; break; }
        }
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
    CBrotherAI defaultBrother;
    std::unique_ptr<BroAIDeathmatch> localBot;
    std::unique_ptr<DeathmatchBot> deathmatchBot;
    CBrotherAI *partner = &defaultBrother;
    // A selected friend's equipment does not change the original Solo policy.
    // Construct the host multiplayer input policy only for a Live session.
    if (launch.localLive) {
        localBot = std::make_unique<BroAIDeathmatch>();
        partner = localBot.get();
    }
    if (launch.deathmatch) {
        deathmatchBot = std::make_unique<DeathmatchBot>();
        partner = deathmatchBot.get();
    }
    CBrotherAI &brother = *partner;
    PlayerModel brotherModel;
    CPlayerConfiguration brotherConfiguration;
    brotherConfiguration.SetDefaults(toc.GetPack(toc.GetCorePackIndex())->GetPackHash());
    // Local default partner: Whippersnappers and the free ER97E Elite rifle.
    // These are core gun 0 and pack5 gun 4 in the original store catalogue.
    brotherConfiguration.guns[1].packHash = toc.GetPack(toc.GetPackIndexFromName("pack5"))->GetPackHash();
    brotherConfiguration.guns[1].localIndex = 4;
    if ((launch.localLive || launch.localBot) && launch.botFriend != nullptr) {
        brotherConfiguration = launch.botFriend->profile.configuration;
    }
    if (launch.deathmatch) {
        if (launch.botFriend != nullptr) { brotherConfiguration = launch.botFriend->profile.configuration; }
        const auto selection = DeathmatchBot::ChooseLoadout(matches[launch.matchIndex], weapons, matchSeed);
        const WeaponEntry *chosen[2]{};
        for (unsigned slot = 0; slot < 2; ++slot) {
            brotherConfiguration.guns[slot] = matches[launch.matchIndex].guns[selection[slot]];
            for (const auto &weapon : weapons) {
                if (weapon.packHash == brotherConfiguration.guns[slot].packHash && weapon.ordinal == brotherConfiguration.guns[slot].localIndex) { chosen[slot] = &weapon; break; }
            }
            if (chosen[slot] == nullptr) { return 1; }
        }
        deathmatchBot->Configure(matchSeed, *chosen[0], *chosen[1]);
    }
    if (withBrother) {
        brother.vitals.maximum = progress.GetHealth();
        if ((launch.localLive || launch.localBot) && launch.botFriend != nullptr) {
            CPlayerProgress botProgress;
            botProgress.Bind(progressData);
            botProgress.SetExperience(launch.botFriend->profile.experience);
            brother.vitals.maximum = botProgress.GetHealth();
        }
        brother.vitals.invincible = false;
        brotherModel.vitals = &brother.vitals;
        brotherModel.human = false;
        brotherModel.cooperative = launch.localLive;
        brotherModel.deathmatch = launch.deathmatch;
        if (launch.deathmatch) { brotherModel.human = true; }
        if (launch.localLive) { brotherModel.human = true; }
        brotherModel.brotherIndex = 1;
        if (gameContext != nullptr) { brotherModel.brotherIndex = 1 - gameContext->profile.playerBrother; }
        if ((launch.localLive || launch.localBot) && launch.botFriend != nullptr) { brotherModel.brotherIndex = launch.botFriend->profile.playerBrother; }
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
    scene.SetLocalLive(launch.localLive);
    if (launch.localLive && !scene.SetReviveResources(loaded.playerTemplate->script)) { return 1; }
    scene.SetTestBot(launch.localLive || launch.localBot || launch.deathmatch);
    survivalHud.SetLiveBrotherIndex(player.brotherIndex);
    survivalHud.SetLivePeerIndex(brotherModel.brotherIndex);
    std::string brotherName = survivalHud.DefaultBrotherName(brotherModel.brotherIndex);
    if (launch.botFriend != nullptr) { brotherName = launch.botFriend->name; }
    survivalHud.SetLivePeerName(brotherName);
    CPlayerProgress peerProgress;
    peerProgress.Bind(progressData);
    if (launch.botFriend != nullptr) { peerProgress.SetExperience(launch.botFriend->profile.experience); }
    scene.SetPeerProgress(&peerProgress);
    if (gameContext != nullptr) { gameContext->botFriend = launch.botFriend; }
    SurvivalSession session(scene, loaded.map, enemies);
    if (launch.deathmatch) { session.SetDeathmatch(&match); }
    session.GetLevel().SetCooperative(launch.localLive);
    CChallengeManager challenges;
    if (gameContext != nullptr && gameContext->profile.nativeArchive && GameHostSettings().isConnected && !gameContext->tutorial && !launch.deathmatch) {
        if (!challenges.InitProgressData(toc, tables, gameContext->profile, static_cast<unsigned>(std::time(nullptr)))) { return 1; }
        session.SetChallenges(&challenges, &gameContext->profile, &weapons);
        survivalHud.SetChallenges(&challenges);
        if (!gameContext->SaveProfile()) { return 1; }
    }
    CProfileManager researchProfile;
    CProfileManager *pickupProfile = nullptr;
    if (gameContext != nullptr) { pickupProfile = &gameContext->profile; }
    else { pickupProfile = &researchProfile; }
    PowerupScene powerups(toc, tables, player, vitals, scene, effects, *pickupProfile);
    if (!powerups.Init()) { return 1; }
    CProfileManager peerResearchProfile = *pickupProfile;
    CProfileManager *peerProfile = &peerResearchProfile;
    if (launch.botFriend != nullptr) { peerProfile = &launch.botFriend->profile; }
    scene.SetPeerProfile(peerProfile);
    player.friendCount = pickupProfile->friendCount;
    brotherModel.friendCount = peerProfile->friendCount;
    if (launch.deathmatch) {
        // CBrother::HandleDamage :136705 explicitly excludes BRO BUFF in DM.
        player.friendCount = 0; brotherModel.friendCount = 0;
        for (const auto &entry : peerProfile->weaponMastery) {
            const std::uint64_t key = (static_cast<std::uint64_t>(entry.resource.packHash) << 8) | entry.resource.localIndex;
            brotherModel.masteryByWeapon[key] = entry.experience;
        }
    }
    for (unsigned peer = 0; peer < 2; ++peer) {
        const CPlayerConfiguration *configuration = &pickupProfile->configuration;
        if (launch.deathmatch) { configuration = &matchConfiguration; }
        if (peer == 1) { configuration = &brotherConfiguration; }
        for (unsigned slot = 0; slot < 2; ++slot) {
            for (const auto &weapon : weapons) {
                const auto &ref = configuration->guns[slot];
                if (ref.packHash == weapon.packHash && ref.localIndex == weapon.ordinal) {
                    scene.SetGunConfiguration(peer, slot, ref, weapon.data.GetMasteryLimit());
                }
            }
        }
    }
    player.gunSlot = equippedWeaponSlot;
    PowerupScene peerPowerups(toc, tables, brotherModel, brother.vitals, scene, effects, *peerProfile, kBrotherCombatId);
    if ((launch.localLive || launch.deathmatch) && !peerPowerups.Init()) { return 1; }
    if (launch.localLive || launch.deathmatch) { session.SetPeerPowerups(&peerPowerups); }
    if (launch.deathmatch) { powerups.SetDeathmatch(&match); peerPowerups.SetDeathmatch(&match); }
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
    if (launch.localLive || launch.deathmatch) { pickups.SetPeerProfile(peerProfile); }
    if (!pickups.Init()) { return 1; }
    session.SetPickups(&pickups, &effects);
    if (launch.deathmatch) { scene.SetDeathmatch(&match, &weapons, &pickups); }
    const GameObjectRef *archiveLevel = nullptr;
    if (archiveMission != nullptr) { archiveLevel = &archiveMission->data.level; }
    else if (gameContext != nullptr && gameContext->profile.nativeArchive && !gameContext->tutorial && gameContext->planet < 4) {
        archiveLevel = &gameContext->profile.nativeArchive->survivalLevels[gameContext->planet];
    }
    session.SetDialogHud(&survivalHud);
    if (launch.debugMap != nullptr) { archiveLevel = &launch.debugMap->level; }
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
    if (!check && !bossStudy && !performanceStudy && capturePath.empty()) {
        session.SetScriptRandomSeed(static_cast<std::uint32_t>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    }
#if GB_ENABLE_TESTS
    // DM checks fix both the Bot stream and the independent LEVEL script stream.
    if (development->deathmatchCheck) { session.SetScriptRandomSeed(matchSeed); }
#endif
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
    if (!session.SubmitChallenges(false)) { return 1; }
    loading.Finish();
#if GB_ENABLE_TESTS
    if (development->deathmatchCheck) {
        int CheckDeathmatchCombat(SurvivalDeathFixture, CMPMatch &, PickupScene &, PowerupScene &, CProfileManager &, SurvivalGameContext &);
        SurvivalDeathFixture fixture{checkFailures, packShortName, false, vitals, window, program, batch,
            loaded, player, effects, scene, brother, brotherModel, session, startX, startY, startFacing};
        if (development->deathmatchFeedbackCheck) {
            int CheckDeathmatchFeedback(SurvivalDeathFixture, CMPMatch &, PowerupScene &, CProfileManager &);
            return CheckDeathmatchFeedback(fixture, match, powerups, gameContext->profile);
        }
        return CheckDeathmatchCombat(fixture, match, pickups, peerPowerups, *peerProfile, *gameContext);
    }
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
    if (development->localLiveCheck) {
        SurvivalDeathFixture fixture{checkFailures, packShortName, false, vitals, window, program, batch,
            loaded, player, effects, scene, brother, brotherModel, session, startX, startY, startFacing};
        if (CheckLocalLive(fixture, &survivalHud) != 0) { return 1; }
        session.Restart(startX, startY, startFacing);
        return CheckLivePeerActions(fixture, toc, tables, powerups, peerPowerups, *peerProfile);
    }
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
    int accumulator = 0;
    LiveShopSession liveShop;
    LiveShopSession botShop;
    unsigned deathShopSerial = UINT32_MAX;
    bool matchShopCounted = false, matchShopPurchased = false;
    std::vector<StoreEntry> matchStore;
    if (launch.deathmatch && !LoadStoreCatalog(toc, tables, matchStore)) { return 1; }
    bool deathShop = false;
    const auto openShop = [&](unsigned peer) {
        if (launch.deathmatch && !match.CanShop(peer)) { return; }
        if (launch.deathmatch) {
            if (peer == 1) {
                if (botShop.Request(1, window.GetTicksMs(), 0)) { matchShopCounted = false; matchShopPurchased = false; }
            } else if (liveShop.Request(0, window.GetTicksMs(), 0, UINT32_MAX)) { survivalHud.ResetSelector(scene.IsMatchSpawnPending(0)); }
            itemChoice = false;
            return;
        }
        if (launch.localLive || launch.deathmatch) {
            if (liveShop.Request(peer, window.GetTicksMs())) {
                itemChoice = false; accumulator = 0; matchShopCounted = false; matchShopPurchased = false;
            }
        } else if (peer == 0) { shopOpen = true; itemChoice = false; accumulator = 0; }
    };
    const auto closeShop = [&]() {
        // ResumeActionCallback :94386 completes both first entry and respawn.
        if (launch.deathmatch && match.GetResult() == CMPMatch::Result::Playing) {
            if (scene.IsMatchSpawnPending(0)) {
                if (!scene.RespawnDeathmatch(0, true)) { ++checkFailures; }
            } else if (vitals.dead && vitals.deathAnimationComplete) {
                if (!scene.RespawnDeathmatch(0, false, true)) { ++checkFailures; }
            }
        }
        if (deathShop) { scene.FinishDeathChoice(0); deathShop = false; }
        if (launch.localLive || launch.deathmatch) { liveShop.Close(0); }
        survivalHud.ResetSelector();
        shopOpen = false; itemChoice = false;
    };
    if (launch.deathmatch) {
        if (!survivalHud.ConfigureDeathmatch(matches[launch.matchIndex].data.stores)) { return 1; }
    }
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
        state.xplodiumMultiplier = static_cast<int>(std::ceil(session.GetLevel().GetXplodiumMultiplierPercent() *
            PlayerArmorMultiplier(player, 4) * CFriendPowerManager::Multiplier(player.friendCount, 6)));
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
        if (launch.deathmatch) {
            state.guns[0] = scene.MatchGun(0, 0);
            state.guns[1] = scene.MatchGun(0, 1);
        }
        state.shopOpen = shopOpen;
        state.localLive = launch.localLive;
        state.deathmatch = launch.deathmatch;
        state.remoteShop = shopOpen && (launch.localLive || launch.deathmatch) && liveShop.Owner() == 1;
        state.afterDeathShop = deathShop;
        state.shopRemainingMs = liveShop.Remaining(window.GetTicksMs());
        if (!liveShop.IsTimed()) { state.shopRemainingMs = 0; }
        state.brotherName = brotherName;
        state.itemChoice = itemChoice;
        state.leftPowerup = leftPowerup;
        state.rightPowerup = rightPowerup;
        state.leftCount = pickupProfile->GetPowerupCount(leftPowerup);
        state.rightCount = pickupProfile->GetPowerupCount(rightPowerup);
        state.powerupCooldowns = powerups.Cooldowns();
        state.inventory = pickupProfile->powerups;
        state.coins = pickupProfile->coins;
        state.warbucks = pickupProfile->warbucks;
        if (state.remoteShop) {
            state.inventory = peerProfile->powerups;
            state.coins = peerProfile->coins;
            state.warbucks = peerProfile->warbucks;
        }
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
        if (launch.deathmatch) { state.inputHidden = false; }
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
    std::uint64_t previous = window.GetTicksMs();
    Camera camera;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    std::printf("[survival] WASD move, mouse aim/fire, Q/E powerups, 1 shop, 2 swap weapon, Esc/space pause\n");
    auto menuTicks = window.GetTicksMs();

#if GB_ENABLE_TESTS
    // Deterministic 1,200 rendered frames, with real waves, AI and projectiles.
    if (development != nullptr && development->flockCheck) {
        vitals.invincible = true;
        return CheckFlockMovement(scene);
    }
    std::unique_ptr<ISurvivalInputDriver> performancePilot;
    std::ofstream performanceReport;
    std::vector<double> performanceCpu;
    unsigned performanceFrame = 0;
    unsigned performanceSteps = 0;
    std::size_t performancePeakAlive = 0;
    bool performancePassed = true;
#endif
    
#if GB_ENABLE_TESTS
if (performanceStudy) {
        if (development->performanceSpawnStudy) {
            // User screenshot coordinates are a test input, not map resource data.
            scene.playerX = 454.8f;
            scene.playerY = 688.7f;
            brother.vitals.invincible = true;
            if (development->performanceFlockStudy) {
                scene.playerX = 733.6f;
                scene.playerY = 284.1f;
            }
        } else {
            performancePilot = CreateSurvivalInputDriver(scene, loaded.map.GetVisibleBounds());
            if (!performancePilot) { return 1; }
        }
        PerformanceProbe::enabled = true;
        PerformanceProbe::uncachedPaths = development->performanceUncachedPaths;
        vitals.invincible = true;
        if (!window.SetVSync(false)) { return 1; }
        std::printf("[performance] vsync=0 update-step=16ms realtime=%d uncached-paths=%d target=1200\n",
            development->performanceRealtimeStudy, development->performanceUncachedPaths);
        performanceReport.open(TestOutput::Path("performance-frames.csv"));
        performanceReport << "frame,update_ms,geometry_ms,world_ms,hud_ms,present_ms,alive,spawned,spawn_ms,brother_ms,navigation_ms,enemy_ms,effects_ms,new_spawns,brother_hp,player_x,player_y,path_ms,path_calls,path_nodes,update_steps,flock_ms,nearest_mean,minimum_gap,close_pairs\n";
    }
#endif

    while (window.PumpEvents()) {
#if GB_ENABLE_TESTS
        PerformanceProbe::counters = {};
#endif
        const auto performanceStart = std::chrono::steady_clock::now();
        const auto frameTicks = window.GetTicksMs();
        const unsigned menuElapsed = static_cast<unsigned>(frameTicks - menuTicks);
        if (launch.deathmatch && !session.IsFinished()) {
            // DeathMatchIntroSequenceCallback :90035 opens the selector after the banner.
            if (survivalHud.TakeDeathmatchIntroCompletion() && !session.IsFinished()) {
                survivalHud.ResetSelector(true);
                liveShop.Request(0, frameTicks, 0, match.Data().respawnSeconds * 1000);
                // The local Bot has already chosen its two guns. Each peer
                // enters independently; an unfinished player loadout stays absent.
                if (scene.IsMatchSpawnPending(1) && !scene.RespawnDeathmatch(1, true)) { ++checkFailures; }
            }
            const bool wasShopping = liveShop.Active();
            liveShop.Update(frameTicks);
            if (wasShopping && !liveShop.Active()) { closeShop(); }
            botShop.Update(frameTicks);
            const auto &life = match.GetLife(0);
            // OnPlayerKilled opens the normal DM selector after the burst.
            if (life.dead && vitals.deathAnimationComplete && deathShopSerial != life.serial &&
                match.GetResult() == CMPMatch::Result::Playing) {
                deathShopSerial = life.serial;
                liveShop.Close(0);
                survivalHud.ResetSelector();
                unsigned selectorMs = 1;
                if (life.respawnMs > 1000) { selectorMs = life.respawnMs - 1000; }
                liveShop.Request(0, frameTicks, 0, selectorMs);
                itemChoice = false;
            }
            shopOpen = liveShop.Visible(frameTicks);
            if (botShop.Visible(frameTicks) && !matchShopCounted) {
                if (!match.EnterShop(1)) { botShop.Close(1); }
                else { matchShopCounted = true; }
            }
            if (botShop.Visible(frameTicks) && !matchShopPurchased) {
                // Shop decisions use the same catalog, price, balance and level checks as the player.
                while (const auto *item = DeathmatchBot::ChoosePurchase(matchStore, *peerProfile, peerProgress.GetLevel(), match.GetLife(1))) {
                    if (peerProfile->AcquireItem(item->data, peerProgress.GetLevel()) != PurchaseResult::Purchased) { break; }
                    std::printf("[deathmatch] bot purchased powerup=%u\n", item->data.objects.front().object.localIndex);
                }
                matchShopPurchased = true;
                if (launch.botFriend != nullptr && gameContext != nullptr && gameContext->persistProgress && !launch.botFriend->Save()) { return 1; }
            }
            if (botShop.Visible(frameTicks) && botShop.Remaining(frameTicks) < 7500) { botShop.Close(1); }
            if (!paused && !botShop.Active() && !brother.vitals.dead && !session.IsFinished()) {
                if (deathmatchBot->WantsHealth()) { peerPowerups.UseMatchConsumable(false); }
                if (deathmatchBot->WantsGrenade()) { peerPowerups.UseMatchConsumable(true); }
                if (deathmatchBot->WantsShop() && match.CanShop(1) &&
                    DeathmatchBot::ChoosePurchase(matchStore, *peerProfile, peerProgress.GetLevel(), match.GetLife(1)) != nullptr) {
                    openShop(1); deathmatchBot->OnShopAttempt();
                }
            }
        }
        if (launch.localLive) {
            scene.SetAfterDeathAvailability(powerups.HasAfterDeathPowerup(), peerPowerups.HasAfterDeathPowerup());
            const bool wasActive = liveShop.Active();
            liveShop.Update(frameTicks);
            if (wasActive && !liveShop.Active() && deathShop) { scene.FinishDeathChoice(liveShop.Owner()); deathShop = false; }
            if (!liveShop.Active() && !powerups.IsMovieActive() && !peerPowerups.IsMovieActive()) {
                for (unsigned peer = 0; peer < 2; ++peer) {
                    const PlayerVitals *down = &vitals;
                    if (peer == 1) { down = &brother.vitals; }
                    if (scene.NeedsDeathChoice(peer) && down->deathAnimationComplete) {
                        openShop(peer); deathShop = true; break;
                    }
                }
            }
            shopOpen = liveShop.Visible(frameTicks);
            if (shopOpen && liveShop.Owner() == 1) {
                const unsigned browsingMs = LiveShopSession::LimitMs - liveShop.Remaining(frameTicks);
                survivalHud.BrowseRemoteShop(localBot->ShopSelection(browsingMs));
                const auto *item = survivalHud.SelectedItem();
                if (!deathShop && item != nullptr && localBot->ShouldBuyShopItem(browsingMs,
                    peerProfile->GetPowerupCount(item->data.objects.front().object))) {
                    const auto purchase = peerProfile->AcquireItem(item->data, peerProgress.GetLevel());
                    if (purchase == PurchaseResult::Purchased) {
                        std::printf("[local-live] peer purchased powerup=%u\n", item->data.objects.front().object.localIndex);
                        if (launch.botFriend != nullptr && !launch.botFriend->Save()) { return 1; }
                    }
                }
                if (deathShop && liveShop.Remaining(frameTicks) < 8000 && peerPowerups.UseAfterDeathPowerup()) {
                    scene.FinishDeathChoice(1); liveShop.Close(1); shopOpen = false; deathShop = false;
                }
            }
            if (!paused && !liveShop.Active() && !session.IsTransitioning() && !brother.vitals.dead &&
                !powerups.IsMovieActive() && !peerPowerups.IsMovieActive()) {
                localBot->AdvanceActions(menuElapsed);
                if (localBot->TakePowerupRequest()) { peerPowerups.UseAny(); }
                if (localBot->TakeShopRequest()) { openShop(1); }
            }
        }
        survivalHud.AdvanceMenu(static_cast<unsigned>(frameTicks - menuTicks));
        menuTicks = frameTicks;
        
#if GB_ENABLE_CHEATS
        for (std::string cheat = window.TakeCheatCode(); !cheat.empty(); cheat = window.TakeCheatCode()) {
            CombatCheatResult result;
            if (!ApplyCombatCheat(cheat, scene, vitals, powerups, session, gameContext, result, progressData, progress)) { return 1; }
            if (result.botShop && !brother.vitals.dead && !powerups.IsMovieActive() && !peerPowerups.IsMovieActive()) { openShop(1); }
            if (result.botPowerup && !liveShop.Active() && !powerups.IsMovieActive()) { peerPowerups.UseAny(true); }
            if (result.challengesUpdated && !gameContext->tutorial && !launch.deathmatch) {
                // Discard the old day's pending wave deltas before binding the new list.
                scene.TakeChallengeKills();
                scene.TakeChallengePowerups();
                if (!challenges.Bind(toc, tables, gameContext->profile, static_cast<unsigned>(std::time(nullptr)))) { return 1; }
                session.SetChallenges(&challenges, &gameContext->profile, &weapons);
                survivalHud.SetChallenges(&challenges);
                if (!session.SubmitChallenges(false)) { return 1; }
            }
            if (result.resume) { paused = false; shopOpen = false; itemChoice = false; liveShop = {}; deathShop = false; }
            if (result.resetClock) {
                effects.SetPaused(paused || shopOpen);
                accumulator = 0;
                previous = window.GetTicksMs();
            }
        }
#endif

        if (gameContext && gameContext->profile.nativeArchive && !gameContext->tutorial && !launch.deathmatch &&
            GameHostSettings().isConnected && challenges.current.empty()) {
            // Enabling the connection during combat establishes the same local clock.
            scene.TakeChallengeKills();
            scene.TakeChallengePowerups();
            if (!challenges.InitProgressData(toc, tables, gameContext->profile, static_cast<unsigned>(std::time(nullptr)))) { return 1; }
            session.SetChallenges(&challenges, &gameContext->profile, &weapons);
            survivalHud.SetChallenges(&challenges);
            if (!session.SubmitChallenges(false) || !gameContext->SaveProfile()) { return 1; }
        }

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
        if (powerups.IsMovieActive() || peerPowerups.IsMovieActive()) { action = SurvivalHudAction::None; }
        if (launch.deathmatch && session.IsFinished()) { action = SurvivalHudAction::None; }
        if (action == SurvivalHudAction::Exit) {
            // Surrender leaves a paused menu; the same BGM continues into results.
            music.SetPaused(false);
            music.SetVolume(1.0f);
            break;
        }
        if (action == SurvivalHudAction::OpenShop) { openShop(0); }
        if (action == SurvivalHudAction::CloseShop) { closeShop(); }
        if (action == SurvivalHudAction::CancelItem) { itemChoice = false; }
        const StoreEntry *shopItem = survivalHud.SelectedItem();
        if (launch.deathmatch && shopItem != nullptr && action == SurvivalHudAction::SelectMatchGun) {
            const auto &gun = shopItem->data.objects.front().object;
            if (!scene.SelectMatchGun(0, survivalHud.MatchSelectionSlot(), gun)) { return 1; }
            survivalHud.AdvanceMatchSelection();
        }
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
                if (powerups.SelectResource(resource) && powerups.Use(true)) { closeShop(); }
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
            if (launch.deathmatch && session.IsFinished()) { continue; }
            // Replay escape bypasses pause, dialogs, death and powerup movies.
            if (gameContext != nullptr && gameContext->debugTutorial && key == KeyCode::Escape) {
                std::printf("[debug-tutorial] escape no-save=1\n");
                return 0;
            }
            if (HandleDebugKey(key, window, showCollisions)) { continue; }
            if (launch.debugSelection != nullptr && GameDebugKeys::OpensMapBrowser(key, window)) {
                music.SetPaused(true);
                const bool selected = ShowDebugMapPicker(toc, tables, window, *launch.debugSelection);
                music.SetPaused(paused);
                previous = window.GetTicksMs();
                menuTicks = previous;
                accumulator = 0;
                inputs.clear();
                if (selected) {
                    if (!session.SubmitChallenges(true)) { return 1; }
                    if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel(), accountedXplodium)) { return 1; }
                    return kDebugMapSessionChoice;
                }
                continue;
            }
            if (launch.debugMap != nullptr && key == GameDebugKeys::MapBack) { return 0; }
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
            if ((launch.localLive || launch.deathmatch) && liveShop.Active() && liveShop.Owner() == 1) { continue; }
            if (powerups.IsMovieActive() || peerPowerups.IsMovieActive()) { continue; }
            // The original death script hides the input pad. Do not open an
            // invisible pause menu while the formal death animation is running.
            if (vitals.dead && gameContext != nullptr && !shopOpen && !launch.deathmatch) { continue; }
            if (shopOpen) {
                if (key == KeyCode::Space || key == KeyCode::Escape) {
                    if (survivalHud.BackFromSelectorPrompt()) { continue; }
                    if (itemChoice) { itemChoice = false; }
                    else { closeShop(); }
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
                openShop(0);
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
                if (!session.SubmitChallenges(true)) { return 1; }
                if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel(), accountedXplodium)) { return 1; }
                session.Restart(startX, startY, startFacing);
                accountedXplodium = 0;
                if (launch.deathmatch && gameContext != nullptr) {
                    gameContext->accountedPeerKills = 0;
                    gameContext->accountedPeerXplodium = 0;
                }
                if (gameContext != nullptr) { gameContext->accountedKills = 0; gameContext->accountedWeaponExperience.clear(); }
                liveShop = {};
                botShop = {};
                deathShopSerial = UINT32_MAX;
                deathShop = false;
                if (!session.HasOriginalHud()) { survivalHud.ResetNotices(); }
                savedDeath = false;
                paused = false;
                shopOpen = false;
                itemChoice = false;
            }
            std::size_t nextWeapon = weaponSlot;
            if (launch.deathmatch) {
                if (!paused && !vitals.dead && (key == KeyCode::Digit2 || key == KeyCode::N || key == KeyCode::M)) { scene.RequestMatchWeaponSwap(0); }
                continue;
            }
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
        if (capturePath.empty() && window.GetMousePosition(mouseX, mouseY) && !vitals.dead && !powerups.IsMovieActive() && !peerPowerups.IsMovieActive()) {
            scene.facing = std::atan2(camera.y + mouseY / camera.zoom - scene.playerY,
                camera.x + mouseX / camera.zoom - scene.playerX) * kRadiansToDegrees + 90;
        }
        float moveX = 0, moveY = 0;
        if (window.IsKeyDown(KeyCode::A)) { --moveX; }
        if (window.IsKeyDown(KeyCode::D)) { ++moveX; }
        if (window.IsKeyDown(KeyCode::W)) { --moveY; }
        if (window.IsKeyDown(KeyCode::S)) { ++moveY; }
        const std::uint64_t now = window.GetTicksMs();
        // Re-evaluate after input and cheats. No leftover simulation tick may
        // move either actor while either peer owns a visible selector.
        if (launch.localLive || launch.deathmatch) { shopOpen = liveShop.Visible(now); }
        // CPowerUpSelector::Show only suspends non-DM worlds (:1864xx).
        const bool worldPaused = paused || (shopOpen && !launch.deathmatch);
        scene.SetMatchShopping(0, shopOpen);
        scene.SetMatchShopping(1, botShop.Active());
        session.SetSuspended(worldPaused);
        if (worldPaused) { accumulator = 0; }
        if (!worldPaused && capturePath.empty()) { accumulator += static_cast<int>(std::min<std::uint64_t>(now - previous, 100)); }
        previous = now;
        
#if GB_ENABLE_TESTS
if (performanceStudy) {
            if (!development->performanceRealtimeStudy) { accumulator = 16; }
            if (performancePilot) { performancePilot->Update(16, moveX, moveY); }
            if (development->performanceSpawnStudy) {
                moveX = 0;
                moveY = 0;
                if (development->performanceFlockStudy) {
                    // Repeatable square movement through the reported map.
                    switch ((performanceFrame / 300) % 4) {
                    case 0: moveY = 1; break;
                    case 1: moveX = -1; break;
                    case 2: moveY = -1; break;
                    case 3: moveX = 1; break;
                    }
                }
            }
        }
#endif

        
#if GB_ENABLE_TESTS
if (checkControls && !paused && !shopOpen) { accumulator = 960; }
#endif

        effects.SetPaused(worldPaused || (launch.deathmatch && session.IsFinished()));
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
#if GB_ENABLE_TESTS
        unsigned performanceUpdateSteps = 0;
#endif
        const bool checkSwapFiring = checkControls && (controlFrame == 2 || controlFrame == controlClickCount + 2);
        while (accumulator >= 16) {
#if GB_ENABLE_TESTS
            ++performanceUpdateSteps;
#endif
            if (powerups.IsMovieActive() || peerPowerups.IsMovieActive()) {
                session.Update(16, 0, 0, false);
                accumulator -= 16;
                continue;
            }
            if (!session.HasOriginalHud()) { survivalHud.Advance(16); }
            if (!vitals.dead && pendingWeapon < weapons.size() && !swapEventAccepted) {
                SetPlayerInput(player, false, false);
                swapEventAccepted = player.weapon->brother.OnSwapGun();
            }
            if (!vitals.dead || launch.localLive || launch.deathmatch) {
                bool shoot = pendingWeapon >= weapons.size() &&
                    (performanceStudy || firePreview || checkSwapFiring || (window.IsLeftMouseDown() && !hudOwnsPointer));
#if GB_ENABLE_TESTS
                if (development->performanceSpawnStudy) { shoot = false; }
#endif
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
                player.gunSlot = equippedWeaponSlot;
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
            if (launch.deathmatch) {
                const auto active = scene.ActiveMatchGun(0);
                for (std::size_t index = 0; index < weapons.size(); ++index) {
                    if (weapons[index].packHash == active.packHash && weapons[index].ordinal == active.localIndex) { weaponSlot = index; break; }
                }
                equippedWeaponSlot = player.gunSlot;
            }
            const int worldDeltaMs = session.GetLevel().TransformWorldElapseMS(16);
            if (!launch.deathmatch || !session.IsFinished()) {
                AdvanceProps(loaded.props, worldDeltaMs);
                AdvanceTileLayers(loaded.map, worldDeltaMs);
            }
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
        // The archive browser owns preview wrap-up instead of the retail menu.
        if (launch.debugMap != nullptr && session.GetLevel().IsCleared()) { return kDebugMapSessionComplete; }
        if (gameContext != nullptr && gameContext->debugTutorial && !check && capturePath.empty() &&
            session.GetLevel().GetTutorialStep() == -1) {
            std::printf("[debug-tutorial] completed no-save=1\n");
            return 0;
        }
        if (session.IsReadyForResults() && gameContext != nullptr && !check && capturePath.empty()) { break; }
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
        scene.UpdatePeerIndicator(menuElapsed, camera.x, camera.y, width / camera.zoom, height / camera.zoom);
        if (scene.PeerIndicator() != nullptr) { hudState.indicators.push_back(*scene.PeerIndicator()); }
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
            if (launch.localLive || launch.localBot || launch.deathmatch) {
                hudState.brotherName = brotherName;
                hudState.brotherLabelAlpha = 1;
            }
        }
        hudState.localLive = launch.localLive;
        hudState.deathmatch = launch.deathmatch;
        if (launch.deathmatch) {
            hudState.inputHidden = false;
            hudState.guns[0] = scene.MatchGun(0, 0);
            hudState.guns[1] = scene.MatchGun(0, 1);
            hudState.matchScore[0] = match.Score(0); hudState.matchScore[1] = match.Score(1);
            hudState.matchLimit = match.Data().killLimit;
            hudState.respawnMs = match.GetLife(0).respawnMs;
        }
        hudState.reviveProgress = scene.GetReviveProgress();
        if (launch.localLive && hudState.reviveProgress > 0) {
            float x = brother.x, y = brother.y;
            if (vitals.dead) { x = scene.playerX; y = scene.playerY; }
            // CBrother::GetBounds :134186 is a native 100x100 box at
            // (trunc(x)-50, trunc(y)-50), independent of mesh and weapon.
            const int barWidth = static_cast<int>(30 * healthBarViewportScale);
            const int barHeight = static_cast<int>(4 * healthBarViewportScale);
            const int padding = static_cast<int>(healthBarViewportScale);
            hudState.reviveBar.width = float(barWidth) * 1024 / width;
            hudState.reviveBar.height = float(barHeight) * 768 / height;
            hudState.revivePaddingX = float(padding) * 1024 / width;
            hudState.revivePaddingY = float(padding) * 768 / height;
            hudState.reviveBar.x = (static_cast<int>(x) - barWidth / 2 - camera.x) * camera.zoom * 1024 / width;
            hudState.reviveBar.y = (static_cast<int>(y) - 50 - camera.y) * camera.zoom * 768 / height;
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
        if (!powerups.DrawMovies() || (launch.localLive && !peerPowerups.DrawMovies())) { return 1; }
        if (gameContext != nullptr && gameContext->debugTutorial &&
            !survivalHud.DrawTutorialDebugNotice(window.GetTicksMs())) { return 1; }
        
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
#if GB_ENABLE_TESTS
        if (performanceStudy && development->performanceFlockStudy && (performanceFrame + 1) % 300 == 0) {
            if (!GB_SAVE_FRAME(window, TestOutput::Path("flock-" + std::to_string(performanceFrame + 1) + ".png"))) { return 1; }
        }
#endif
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
                << ',' << scene.AliveCount() << ',' << scene.spawned
                << ',' << PerformanceProbe::counters.spawnMs << ',' << PerformanceProbe::counters.brotherMs
                << ',' << PerformanceProbe::counters.navigationMs << ',' << PerformanceProbe::counters.enemyMs
                << ',' << PerformanceProbe::counters.effectsMs << ',' << PerformanceProbe::counters.spawns
                << ',' << brother.vitals.health << ',' << scene.playerX << ',' << scene.playerY
                << ',' << PerformanceProbe::counters.pathSearchMs << ',' << PerformanceProbe::counters.pathSearches
                << ',' << PerformanceProbe::counters.pathNodes << ',' << performanceUpdateSteps;
            const auto flockMetrics = MeasureFlock(scene);
            performanceReport << ',' << PerformanceProbe::counters.flockMs << ',' << flockMetrics.nearestMean
                << ',' << flockMetrics.minimum << ',' << flockMetrics.closePairs << '\n';
            performanceCpu.push_back(updateMs + geometryMs + worldMs + hudMs);
            performanceSteps += performanceUpdateSteps;
            performancePeakAlive = std::max(performancePeakAlive, scene.AliveCount());
            if (++performanceFrame % 300 == 0) {
                std::printf("[performance] frame=%u update=%.2f geometry=%.2f world=%.2f hud=%.2f present=%.2f alive=%zu\n",
                    performanceFrame, updateMs, geometryMs, worldMs, hudMs, presentMs, scene.AliveCount());
            }
            bool finished = performanceFrame >= 1200;
            if (development->performanceRealtimeStudy) { finished = performanceSteps >= 1200; }
            if (finished) {
                std::sort(performanceCpu.begin(), performanceCpu.end());
                const std::size_t count = performanceCpu.size();
                std::printf("[performance] cpu-p50=%.3f cpu-p95=%.3f cpu-p99=%.3f max=%.3f frames=%u kills=%u\n",
                    performanceCpu[count / 2], performanceCpu[count * 95 / 100], performanceCpu[count * 99 / 100],
                    performanceCpu.back(), performanceFrame, scene.GetTotalKills());
                std::printf("[performance] enemy-assets hits=%u misses=%u templates=%zu\n", scene.GetEnemyModelCache().hits,
                    scene.GetEnemyModelCache().misses, scene.GetEnemyModelCache().entries.size());
                if (development->performanceSpawnStudy) {
                    // Host acceptance budget: 60 Hz CPU work, with the reported crowd present.
                    const unsigned enemyLimit = session.GetLevel().GetEnemyLimit();
                    performancePassed = performancePeakAlive >= enemyLimit && performanceCpu[count * 95 / 100] <= 1000.0 / 60;
                    std::printf("[spawn-performance] updates=%u peak-alive=%zu pool-limit=%u cpu-p95-budget=16.667 passed=%d\n",
                        performanceSteps, performancePeakAlive, enemyLimit, performancePassed);
                }
                break;
            }
        }
#endif

    }
#if GB_ENABLE_TESTS
    PerformanceProbe::enabled = false;
    PerformanceProbe::uncachedPaths = false;
    if (!performancePassed) { return 1; }
#endif
    if (!session.SubmitChallenges(true)) { return 1; }
    if (launch.deathmatch && gameContext != nullptr) {
        if (match.GetResult() == CMPMatch::Result::Playing) { match.Surrender(0); }
        auto &result = gameContext->result;
        result.live = true; result.deathmatch = true;
        result.matchResult = static_cast<unsigned>(match.GetResult());
        result.matchKillLimit = match.Data().killLimit;
        result.matchTimeLimitSeconds = match.Data().seconds;
        result.score = scene.GetScore();
        result.bestKillStreak = scene.GetBestKillStreak();
        result.peerName = brotherName;
        for (unsigned peer = 0; peer < 2; ++peer) {
            result.peers[peer] = scene.GetMultiplayerStatistics(peer).total;
            result.peers[peer].kills = match.Score(peer);
            result.peers[peer].deaths = match.GetLife(peer).serial;
            if (match.GetLife(peer).dead) { ++result.peers[peer].deaths; }
        }
        if (launch.botFriend != nullptr && gameContext->persistProgress && !launch.botFriend->Save()) { return 1; }
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
