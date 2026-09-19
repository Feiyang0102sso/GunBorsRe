/** CGame session host implementation; original ownership follows game.cpp.
 * SDL/GL submission and borrowed desktop resources are host adaptations.
 */
#include "gun_bros_re/gameplay/game/CGameRuntime.h"
#include "gun_bros_re/debug/DebugMaps.h"
#include "gun_bros_re/debug/FrameRateOverlay.h"
#include "gun_bros_re/ui/ZLoadingScreen.h"
#include "gun_bros_re/data/ZProfileStorage.h"
#include "gun_bros_re/data/ZArmorCatalog.h"
#include "gun_bros_re/gameplay/map/CLayerObjectPlayers.h"
#include "engine/core/CStringToKey.h"
#include <ctime>

using namespace MapDetail;

int CGame::Run(const Launch &launch) {
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
    ZGameObserver::Options options;
    if (launch.observer != nullptr) { launch.observer->Configure(launch, options); }
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, kArtSetXga) || !toc.Bind()) { return 1; }
    const int packIndex = toc.GetPackIndexFromName(packShortName.c_str());
    if (packIndex < 0) { return 1; }
    ZPackTables tables(toc);
    if (!tables.HasLatestBigVersion()) {
        std::printf("[survival] BigVersion 1 required; older formats are supported for resource viewing only\n");
        return 1;
    }
    std::vector<ZWeaponEntry> weapons;
    std::vector<CMPMatch::Entry> matches;
    CMPMatch match;
    CPlayerConfiguration matchConfiguration;
    std::vector<CEnemy::Template> enemies;
    ZPlayerVitals vitals;
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
    ZWindow ownedWindow;
    ZWindow *activeWindow = &ownedWindow;
    if (sharedWindow != nullptr) { activeWindow = sharedWindow; }
    ZWindow &window = *activeWindow;
    if (!window.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
    if (!SetDebugFPS(window, GameHostSettings().drawFPS, bigDirectory)) { return 1; }
    window.SetEscapeCloses(false);
    // Both the retail frontend and standalone survival research use shortcuts.
    window.EnableCheats(true);
    CBGM ownedMusic;
    CBGM *activeMusic = &ownedMusic;
    if (gameContext != nullptr && gameContext->music != nullptr) { activeMusic = gameContext->music; }
    CBGM &music = *activeMusic;
    if (gameContext != nullptr) { music.SetEnabled(gameContext->profile.musicEnabled); }
    ZMovieRenderer loadingMovies;
    CResPackTOC *loadingCore = toc.GetPack(toc.GetCorePackIndex());
    if (!loadingMovies.Init(*loadingCore, *loadingCore)) { return 1; }
    const CProfileManager *loadingProfile = nullptr;
    if (gameContext != nullptr) { loadingProfile = &gameContext->profile; }
    ZLoadingScreen loading(window, loadingMovies, tables, loadingProfile, true, false, &music, launch.localLive,
                           launch.deathmatch);
    if (!loading.IsValid()) { return 1; }
    if (!LoadWeaponCatalog(toc, tables, weapons) || !CEnemy::Template::LoadCatalog(toc, tables, enemies) ||
        !LoadInitialPlayerHealth(toc, tables, vitals.maximum)) {
        return 1;
    }
    unsigned matchSeed = static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count());
    if (options.matchSeed.has_value()) { matchSeed = *options.matchSeed; }
    if (launch.deathmatch) {
        if (!LoadMPMatches(toc, tables, matches) || launch.matchIndex >= matches.size()) { return 1; }
        const auto &entry = matches[launch.matchIndex];
        match.Bind(entry.data, matchSeed);
        match.SetBotLevel(static_cast<CMPMatch::BotLevel>(GameHostSettings().dmBotLevel));
        for (unsigned slot = 0; slot < 2; ++slot) {
            if (launch.loadout[slot] >= entry.guns.size()) { return 1; }
            matchConfiguration.guns[slot] = entry.guns[launch.loadout[slot]];
        }
    }
    CInputPad survivalHud;
    if (!survivalHud.Init(toc, tables)) { return 1; }
    ZShaderProgram program, markerProgram;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0") ||
        !markerProgram.Load(kShaderDirectory, "ogles_vs_mvp_constcolor", "ogles_ps_constcolor")) {
        return 1;
    }
    ZQuadBatch batch;
    ZMarkerBatch markers;
    if (!batch.Create(program) || !markers.Create(markerProgram)) { return 1; }
    CMap loaded;
    if (!loaded.Load(toc, packIndex, mapIndex)) { return 1; }
    LoadPlacedPlayers(toc, program, loaded);
    if (loaded.GetResources().players.empty()) { return 1; }
    // The second brother will be driven by the partner system, not a stationary clone.
    loaded.GetResources().players.resize(1);
    CBrother &player = *loaded.GetResources().players[0].model;
    player.SetCooperative(launch.localLive);
    player.SetDeathmatch(launch.deathmatch);
    player.SetVitals(&vitals);
    if (gameContext != nullptr) {
        for (const auto &entry : gameContext->profile.weaponMastery) {
            const std::uint64_t key =
                (static_cast<std::uint64_t>(entry.resource.packHash) << 8) | entry.resource.localIndex;
            player.masteryByWeapon[key] = entry.experience;
        }
    }
    std::size_t weaponSlot = weaponIndex % weapons.size();
    unsigned equippedWeaponSlot = 0;
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
            if (weapons[index].packHash == matchConfiguration.guns[0].packHash &&
                weapons[index].ordinal == matchConfiguration.guns[0].localIndex) {
                weaponSlot = index;
                break;
            }
        }
    }
    if (!EquipControlledPlayer(tables, loaded, program, weapons[weaponSlot])) { return 1; }
    if (armorIndex >= 0) {
        std::vector<ZArmorEntry> armors;
        if (!LoadArmorCatalog(toc, tables, armors) || armorIndex >= static_cast<int>(armors.size()) ||
            !player.EquipArmor(tables, armors[armorIndex].data, program)) {
            return 1;
        }
    }
    if (gameContext != nullptr) {
        std::vector<ZArmorEntry> armors;
        if (!LoadArmorCatalog(toc, tables, armors)) { return 1; }
        for (const GameObjectRef &ref : gameContext->profile.configuration.armor) {
            if (ref.IsNull()) { continue; }
            bool found = false;
            for (const ZArmorEntry &entry : armors) {
                if (entry.packHash == ref.packHash && entry.ordinal == ref.localIndex) {
                    if (!player.EquipArmor(tables, entry.data, program)) { return 1; }
                    found = true;
                    break;
                }
            }
            if (!found) { return 1; }
        }
    }
    CLevel scene(toc, tables, program, loaded.GetResources().particlePool, loaded.GetResources().particleSystem);
    if (launch.observer != nullptr) {
        const int result = launch.observer->OnResources(
            {toc, tables, enemies, vitals, progressData, window, survivalHud, program, loaded, player, scene});
        if (result >= 0) { return result; }
    }
    scene.BindCombat(enemies, player, vitals, loaded.GetResources().playerTemplate->GetGameScale());
    if (gameContext != nullptr) {
        music.SetEnabled(gameContext->profile.musicEnabled);
        ZAudioPlayer::SetEffectsEnabled(gameContext->profile.soundEnabled);
        player.brotherIndex = gameContext->profile.playerBrother;
    }
    CBrotherAI defaultBrother;
    std::unique_ptr<ZLocalCoopBot> localBot;
    std::unique_ptr<ZLocalPVPBot> deathmatchBot;
    CBrotherAI *partner = &defaultBrother;
    // A selected friend's equipment does not change the original Solo policy.
    // Construct the host multiplayer input policy only for a Live session.
    if (launch.localLive) {
        localBot = std::make_unique<ZLocalCoopBot>();
        partner = localBot.get();
    }
    if (launch.deathmatch) {
        deathmatchBot = std::make_unique<ZLocalPVPBot>();
        partner = deathmatchBot.get();
    }
    CBrotherAI &brother = *partner;
    CBrother brotherModel;
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
        const auto selection = ZLocalPVPBot::ChooseLoadout(matches[launch.matchIndex], weapons, matchSeed);
        const ZWeaponEntry *chosen[2]{};
        for (unsigned slot = 0; slot < 2; ++slot) {
            brotherConfiguration.guns[slot] = matches[launch.matchIndex].guns[selection[slot]];
            for (const auto &weapon : weapons) {
                if (weapon.packHash == brotherConfiguration.guns[slot].packHash &&
                    weapon.ordinal == brotherConfiguration.guns[slot].localIndex) {
                    chosen[slot] = &weapon;
                    break;
                }
            }
            if (chosen[slot] == nullptr) { return 1; }
        }
        deathmatchBot->Configure(matchSeed, *chosen[0], *chosen[1],
                                 static_cast<CMPMatch::BotLevel>(GameHostSettings().dmBotLevel));
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
        brotherModel.SetVitals(&brother.vitals);
        brotherModel.SetHuman(false);
        brotherModel.SetCooperative(launch.localLive);
        brotherModel.SetDeathmatch(launch.deathmatch);
        if (launch.deathmatch) { brotherModel.SetHuman(true); }
        if (launch.localLive) { brotherModel.SetHuman(true); }
        brotherModel.brotherIndex = 1;
        if (gameContext != nullptr) { brotherModel.brotherIndex = 1 - gameContext->profile.playerBrother; }
        if ((launch.localLive || launch.localBot) && launch.botFriend != nullptr) {
            brotherModel.brotherIndex = launch.botFriend->profile.playerBrother;
        }
        std::size_t brotherWeaponSlot = 0;
        for (std::size_t index = 0; index < weapons.size(); ++index) {
            if (weapons[index].packHash == brotherConfiguration.guns[0].packHash &&
                weapons[index].ordinal == brotherConfiguration.guns[0].localIndex) {
                brotherWeaponSlot = index;
                break;
            }
        }
        if (!brotherModel.BuildBody(tables, player.moveSet) ||
            !brotherModel.EquipWeapon(tables, loaded.GetResources().playerTemplate->GetScript(),
                                      weapons[brotherWeaponSlot].data, "AI brother") ||
            !brotherModel.CreateBuffers(program)) {
            return 1;
        }
        for (const GameObjectRef &ref : brotherConfiguration.armor) {
            if (ref.IsNull()) { continue; }
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(ref.packHash, ZGameSection::Armor, ref.localIndex, payload)) { return 1; }
            CArrayInputStream input(payload);
            CArmor::Template armor;
            if (!armor.Init(input) || !brotherModel.EquipArmor(tables, armor, program)) { return 1; }
        }
        scene.SetBrother(&brotherModel, &brother);
        const ZWeaponEntry *rifle = nullptr;
        for (const ZWeaponEntry &entry : weapons) {
            if (entry.packHash == brotherConfiguration.guns[1].packHash &&
                entry.ordinal == brotherConfiguration.guns[1].localIndex) {
                rifle = &entry;
                break;
            }
        }
        if (rifle == nullptr) { return 1; }
        scene.SetBrotherWeapons(loaded.GetResources().playerTemplate->GetScript(), weapons[brotherWeaponSlot].data,
                                rifle->data);
    }
    scene.SetPlayerProgress(&progress);
    scene.SetLocalLive(launch.localLive);
    if (launch.localLive && !scene.SetReviveResources(loaded.GetResources().playerTemplate->GetScript())) { return 1; }
    scene.SetLocalBot(launch.localLive || launch.localBot || launch.deathmatch);
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
    CGame session(scene, loaded, enemies);
    if (launch.deathmatch) { session.SetDeathmatch(&match); }
    session.GetLevel().SetCooperative(launch.localLive);
    CChallengeManager challenges;
    if (gameContext != nullptr && gameContext->profile.nativeArchive && GameHostSettings().isConnected &&
        !gameContext->tutorial && !launch.deathmatch) {
        if (!challenges.InitProgressData(toc, tables, gameContext->profile,
                                         static_cast<unsigned>(std::time(nullptr)))) {
            return 1;
        }
        session.SetChallenges(&challenges, &gameContext->profile, &weapons);
        survivalHud.SetChallenges(&challenges);
        if (!gameContext->SaveProfile()) { return 1; }
    }
    CProfileManager researchProfile;
    CProfileManager *pickupProfile = nullptr;
    if (gameContext != nullptr) {
        pickupProfile = &gameContext->profile;
    } else {
        pickupProfile = &researchProfile;
    }
    CPowerUpSelector &powerups = survivalHud.PowerupSelector();
    powerups.BindPowerups(toc, tables, player, vitals, scene, *pickupProfile);
    if (!powerups.InitPowerups()) { return 1; }
    CProfileManager peerResearchProfile = *pickupProfile;
    CProfileManager *peerProfile = &peerResearchProfile;
    if (launch.botFriend != nullptr) { peerProfile = &launch.botFriend->profile; }
    scene.SetPeerProfile(peerProfile);
    player.friendCount = pickupProfile->friendCount;
    brotherModel.friendCount = peerProfile->friendCount;
    if (launch.deathmatch) {
        // CBrother::HandleDamage :136705 explicitly excludes BRO BUFF in DM.
        player.friendCount = 0;
        brotherModel.friendCount = 0;
        for (const auto &entry : peerProfile->weaponMastery) {
            const std::uint64_t key =
                (static_cast<std::uint64_t>(entry.resource.packHash) << 8) | entry.resource.localIndex;
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
    CPowerUpSelector peerPowerups(toc, tables, brotherModel, brother.vitals, scene, *peerProfile, kBrotherCombatId);
    if ((launch.localLive || launch.deathmatch) && !peerPowerups.InitPowerups()) { return 1; }
    if (launch.localLive || launch.deathmatch) { scene.SetPowerup(&peerPowerups.GetPowerup(), kBrotherCombatId); }
    if (launch.deathmatch) {
        powerups.SetDeathmatch(&match);
        peerPowerups.SetDeathmatch(&match);
    }
    if (launch.observer != nullptr) {
        const int result = launch.observer->OnInventory(toc, researchProfile);
        if (result >= 0) { return result; }
    }

    scene.SetPowerup(&powerups.GetPowerup());
    if (!scene.InitPickups(toc, tables, program, pickupProfile)) { return 1; }
    if (launch.deathmatch) { scene.SetDeathmatch(&match, &weapons); }
    const GameObjectRef *archiveLevel = nullptr;
    if (archiveMission != nullptr) {
        archiveLevel = &archiveMission->data.level;
    } else if (gameContext != nullptr && gameContext->profile.nativeArchive && !gameContext->tutorial &&
               gameContext->planet < 4) {
        archiveLevel = &gameContext->profile.nativeArchive->survivalLevels[gameContext->planet];
    }
    session.SetDialogHud(&survivalHud);
    if (launch.debugMap != nullptr) { archiveLevel = &launch.debugMap->level; }
    if (!session.Load(toc, tables, toc.GetPack(packIndex)->GetPackHash(), mapIndex, archiveLevel,
                      archiveMission != nullptr)) {
        return 1;
    }
    const bool horde = archiveMission != nullptr && archiveMission->data.type == 2;
    if (horde && gameContext != nullptr) {
        gameContext->mission = archiveMission->resource;
        gameContext->missionLevel = archiveMission->data.level;
    }
    if (horde) { session.SetHorde(true); }
    session.SetHud(&survivalHud);
    const float startX = loaded.GetResources().players[0].x;
    const float startY = loaded.GetResources().players[0].y;
    // The map PLAYER object's spawn angle; CBrother::Spawn :135887 writes the
    // same value to both brothers.
    const float startFacing = loaded.GetResources().players[0].facingDegrees;
    session.SetStartWave(static_cast<int>(startWave));
    // The original seeds its one CRandGen from the clock (:370383), so a level
    // script's rolls differ every session. Real play does the same; research
    // runs keep the fixed default stream so their results stay comparable.
    if (options.seedLevel) {
        std::uint32_t seed = static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        if (options.levelSeed.has_value()) { seed = *options.levelSeed; }
        session.SetScriptRandomSeed(seed);
    }
    const bool tutorial = gameContext != nullptr && gameContext->tutorial;
    session.GetLevel().EnableTutorial(tutorial);
    scene.SetMap(loaded, loaded.GetResources().collisionScene, loaded.GetResources().weaponCollision, kLevelCameraScale,
                 kPlayerCollisionRadius);
    session.Restart(startX, startY, startFacing);
    std::uint64_t accountedXplodium = 0;
    // CMap::SetObjectLayer (:91935) activates one layer. Preview may combine
    // layers, but survival must not inherit deathmatch/campaign obstacles.
    // Correction: preload all layers, then OnStart activates only authored
    // layers in script order; previously spawned props survive layer switches.
    loaded.LoadProps(toc);
    loaded.BuildCollisionScene();
    CLevel::Props props(loaded, scene, session.GetLevel());
    session.SetProps(&props);
    scene.SetProps(&props);
    session.Restart(startX, startY, startFacing);
    if (!session.SubmitChallenges(false)) { return 1; }
    loading.Finish();
    CGame::Session state{launch,
                         vitals,
                         window,
                         program,
                         batch,
                         loaded,
                         player,
                         scene,
                         brother,
                         brotherModel,
                         session,
                         startX,
                         startY,
                         startFacing,
                         toc,
                         tables,
                         enemies,
                         weapons,
                         survivalHud,
                         props,
                         withBrother,
                         progress,
                         weaponSlot,
                         equippedWeaponSlot,
                         powerups,
                         pickupProfile,
                         tutorial,
                         accountedXplodium,
                         packIndex,
                         archiveLevel,
                         horde,
                         match,
                         peerPowerups,
                         peerProfile,
                         brotherConfiguration,
                         matches,
                         peerProgress,
                         challenges,
                         markers,
                         markerProgram,
                         localBot.get(),
                         deathmatchBot.get(),
                         music,
                         progressData,
                         options};
    state.brotherName = brotherName;
    if (launch.observer != nullptr) {
        const int result = launch.observer->OnStage(ZGameObserver::Stage::Bound, state);
        if (result >= 0) { return result; }
    }

    if (!loading.IsValid()) { return 1; }
    if (loading.Cancelled()) { return 0; }
    // CGunBros::OnLoaded :79321: battle music begins only after game binding.
    music.SetPaused(false);
    music.SetVolume(1.0f);
    if (!music.NextTrack()) { return 1; }
    if (launch.observer != nullptr) {
        const int result = launch.observer->OnStage(ZGameObserver::Stage::Ready, state);
        if (result >= 0) { return result; }
    }

    return state.Run();
}
