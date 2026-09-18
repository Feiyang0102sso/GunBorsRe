#include "gun_bros_re/debug/FrameRateOverlay.h"
#include "gun_bros_re/debug/DebugTutorial.h"
#include "gun_bros_re/ui/ZGameFrontEndInternal.h"
#include "engine/core/ZPaths.h"
#include "gun_bros_re/ui/ZMenuInternal.h"
#include "gun_bros_re/gameplay/CMPMatch.h"
using namespace MenuDetail;

namespace {
ZLocalBotFriend *MatchBot(ZMenuState &state) {
    if (!state.rematchingBot && state.botRoster != nullptr) { state.matchedBot = state.botRoster->MatchSelected(); }
    state.rematchingBot = false;
    return state.matchedBot;
}
int RunFrontEndSurvival(CGame::Launch launch, ZMenuState &state) {
    launch.debugSelection = &state.debugMap;
    launch.localBot = state.online.IsConnected() && state.social.selectedLocalFriend != 0;
    launch.botFriend = nullptr;
    if (launch.localBot) { launch.botFriend = state.botFriend; }
    if (state.botRoster != nullptr && launch.localBot) { launch.botFriend = state.botRoster->At(state.social.selectedLocalFriend - 1); }
    // Original action23 also matches unlocked MissionType2 (BOKOR) in Live.
    if (state.gameMode == 1 && launch.archiveMission != nullptr) {
        launch.localLive = true;
        launch.withBrother = true;
        if (state.botRoster != nullptr) { launch.botFriend = MatchBot(state); }
        if (launch.botFriend == nullptr) { return 1; }
    }
    return RunSurvival(launch);
}
}

int RunGameMenuSession(const std::string &bigDirectory, const std::string &screenshotPath, unsigned page, bool originalProfile,
    const std::string &profilePath, ZWindow *sharedWindow) {
    ZWindow ownedWindow;
    ZWindow &window = sharedWindow ? *sharedWindow : ownedWindow;
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    if (!tables.HasLatestBigVersion()) {
        std::printf("[game] BigVersion 1 required; older formats are supported for resource viewing only\n");
        return 1;
    }
    CPlayerProgress::Template progress;
    CRefinementManager::Template refinement;
    std::vector<ZStoreEntry> store;
    std::vector<ZWeaponEntry> weapons;
    std::vector<ZArmorEntry> armor;
    {
        ZWindow &loadingWindow = window;
        if (!loadingWindow.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
        if (!SetDebugFPS(loadingWindow, GameHostSettings().drawFPS, bigDirectory)) { return 1; }
        ZMovieRenderer loadingMovies;
        CResPackTOC *core = toc.GetPack(toc.GetCorePackIndex());
        if (!loadingMovies.Init(*core, *core)) { return 1; }
        ZLoadingScreen loading(loadingWindow, loadingMovies, tables, nullptr, false, screenshotPath.empty() || page == 14);
        if (!loading.IsValid()) { return 1; }
        if (!LoadPlayerProgress(toc, tables, progress) || !LoadRefinementTemplate(toc, tables, refinement) ||
            !LoadStoreCatalog(toc, tables, store) || !LoadWeaponCatalog(toc, tables, weapons) ||
            !LoadArmorCatalog(toc, tables, armor)) { return 1; }
        if (!loading.IsValid()) { return 1; }
        if (loading.Cancelled()) { return 0; }
    }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    std::filesystem::path savePath = Paths::Root() / Paths::SaveDirectory;
    if (!profilePath.empty()) { savePath = std::filesystem::u8path(profilePath); }
    // Explicit .dat paths keep old research fixtures usable. Default game and
    // --original-profile both use the original numbered records exclusively.
    if (!profilePath.empty() && savePath.extension() == ".dat") {
        std::printf("[game] explicit legacy research profile: %s\n", savePath.u8string().c_str());
        if (!profile.LoadFromDisk(savePath)) { return 1; }
    } else {
        if (!LoadProfile(toc, tables, profile, savePath)) {
            std::printf("[game] native profile cannot be loaded; files preserved: %s\n", savePath.u8string().c_str());
            return 1;
        }
    }
    CBGM music; // Lifetime includes every menu, loading screen and game session.
    ZMenuState state;
    ZLocalBotRoster botRoster;
    if (!botRoster.Load(toc, tables, savePath, profile)) { return 1; }
    state.botRoster = &botRoster;
    state.social.selectedLocalFriend = botRoster.Selected();
    state.botFriend = botRoster.At(0);
    if (botRoster.Selected() != 0) { state.botFriend = botRoster.At(botRoster.Selected() - 1); }
    state.store.shopGunSlot = profile.activeWeaponSlot;
    state.page = std::min(page, 29u);
    if (page == 0 && screenshotPath.empty()) {
        // Desktop splash requested by the user precedes EnterShell's original
        // first-launch player selection / returning-player greeting.
        state.page = 14;
    }
    if (state.page == 1) { state.page = 2; }
    if (state.page == 7 || state.page == 20 || state.page == 16) { state.page = 0; }
    if (state.page == 9 || state.page == 10) { state.page = 4; }
    if (state.page == 12) { state.page = 6; state.settings.optionsFocus = 9; }
    if (state.page == 15) { state.page = 0; }
    if (state.page == 19 || state.page == 23) { state.page = 21; }
    while (true) {
        if (state.debugMap.ready) {
            // Research sessions own their BGM; suspend the shared menu track.
            music.SetPaused(true);
            RunDebugMaps(bigDirectory, window, state.debugMap, profile);
            music.SetPaused(false);
        }
        const int choice = ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state, savePath, screenshotPath, nullptr, originalProfile, &window, false, nullptr, &music);
        if (choice == kDebugTutorialMenuChoice) {
            CProfileManager debugProfile;
            debugProfile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
            ZSurvivalGameContext context{debugProfile, {}};
            context.music = &music;
            CGame::Launch launch;
            launch.bigDirectory = bigDirectory;
            launch.window = &window;
            if (!PrepareDebugTutorial(toc, tables, context, launch) || RunSurvival(launch) != 0) { return 1; }
            std::printf("[debug-tutorial] return to menu no-save=1\n");
            state.resumeAfterDebugTutorial = true;
            continue;
        }
        if (choice == kDebugMapMenuChoice) { continue; }
        if (choice == -3) { return 1; }
        if (choice == -2) { return 0; }
        if (choice < 0) { return !profile.SaveToDisk(savePath); }
        if (state.gameMode == 2 && choice < 5) {
            std::vector<ZPlanetEntry> planets;
            std::vector<CMPMatch::Entry> matches;
            if (!LoadPlanetCatalog(toc, tables, planets) || static_cast<unsigned>(choice) >= planets.size() || !LoadMPMatches(toc, tables, matches)) { return 1; }
            ZMissionEntry mission;
            mission.resource = planets[choice].data.object12;
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(mission.resource.packHash, ZGameSection::Mission, mission.resource.localIndex, bytes)) { return 1; }
            CArrayInputStream missionInput(bytes);
            if (!mission.data.Init(missionInput) || mission.data.type != 3 || missionInput.Available() != 0) { return 1; }
            if (!tables.ReadSectionResource(mission.data.level.packHash, ZGameSection::Level, mission.data.level.localIndex, bytes)) { return 1; }
            CArrayInputStream levelInput(bytes);
            CLevel::Template level;
            if (!level.Init(levelInput) || levelInput.Available() != 0) { return 1; }
            // CGunBros::GetRandomMpMatchId :78901 picks the match before entry.
            static std::mt19937 matchRandom(std::random_device{}());
            const unsigned tier = std::uniform_int_distribution<unsigned>(0, static_cast<unsigned>(matches.size() - 1))(matchRandom);
            std::array<unsigned, 2> selection{0, 1};
            ZSurvivalGameContext context{profile, savePath, static_cast<unsigned>(choice)};
            context.music = &music;
            context.mission = mission.resource; context.missionLevel = mission.data.level;
            CGame::Launch launch;
            launch.bigDirectory = bigDirectory;
            launch.packShortName = tables.GetPackName(level.mapRef.packHash);
            launch.mapIndex = level.mapRef.localIndex;
            launch.gameContext = &context; launch.window = &window;
            launch.deathmatch = true; launch.matchIndex = tier;
            launch.loadout[0] = selection[0]; launch.loadout[1] = selection[1];
            launch.archiveMission = &mission; launch.botFriend = MatchBot(state);
            if (launch.botFriend == nullptr || RunSurvival(launch) != 0) { return 1; }
            state.online.CancelMatch();
            BeginPostGame(state, context, weapons);
            std::printf("[deathmatch] entered results\n");
            continue;
        }
        if (state.gameMode == 1 && choice < 4) {
            // Live uses the real local account; the peer has its own archive.
            ZSurvivalGameContext context{profile, savePath, static_cast<unsigned>(choice)};
            context.music = &music;
            const auto &level = profile.nativeArchive->survivalLevels[choice];
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(level.packHash, ZGameSection::Level, level.localIndex, bytes)) { return 1; }
            CArrayInputStream input(bytes);
            CLevel::Template data;
            if (!data.Init(input) || input.Available() != 0) { return 1; }
            CGame::Launch launch;
            launch.bigDirectory = bigDirectory;
            launch.packShortName = tables.GetPackName(data.mapRef.packHash);
            launch.mapIndex = data.mapRef.localIndex;
            launch.gameContext = &context;
            launch.window = &window;
            launch.localLive = true;
            launch.botFriend = MatchBot(state);
            if (launch.botFriend == nullptr) { return 1; }
            launch.withBrother = true;
            if (state.starMap.startingWave >= 0) { launch.startWave = static_cast<unsigned>(state.starMap.startingWave); }
            const int result = RunSurvival(launch);
            if (result != 0) { return result; }
            state.online.CancelMatch();
            BeginPostGame(state, context, weapons);
            std::printf("[local-live] entered multiplayer results\n");
            continue;
        }
        if (choice == 5) {
            ZSurvivalGameContext context{profile, savePath, 0};
            context.music = &music;
            context.tutorial = true;
            std::string tutorialPack = "pack2";
            unsigned tutorialMap = 7;
            if (profile.nativeArchive) {
                const auto &level = profile.nativeArchive->survivalLevels[0];
                std::vector<std::uint8_t> bytes;
                if (!tables.ReadSectionResource(level.packHash, ZGameSection::Level, level.localIndex, bytes)) { return 1; }
                CArrayInputStream input(bytes);
                CLevel::Template data;
                if (!data.Init(input) || input.Available() != 0) { return 1; }
                tutorialPack = tables.GetPackName(data.mapRef.packHash);
                tutorialMap = data.mapRef.localIndex;
            }
            const int result = RunFrontEndSurvival(CGame::Launch{bigDirectory, tutorialPack, tutorialMap, 0, -1, 0, &context, true, nullptr, &window}, state);
            if (result == kDebugMapSessionChoice) { continue; }
            if (result != 0) { return 1; }
            if (profile.tutorialCompleted) { BeginPostGame(state, context, weapons); }
            else { state.Navigate(25, true); }
            continue;
        }
        if (choice == 4) {
            if (profile.nativeArchive) {
                std::vector<ZPlanetEntry> planets;
                if (!LoadPlanetCatalog(toc, tables, planets) || state.planet >= planets.size()) { return 1; }
                const auto &planet = planets[state.planet];
                if (state.hordeStart >= planet.missions.size() ||
                    !SameObject(state.selectedMission, planet.data.missions[state.hordeStart])) { return 1; }
                ZMissionEntry selected;
                selected.resource = state.selectedMission;
                selected.data = planet.missions[state.hordeStart];
                selected.title = planet.missionInfo[state.hordeStart].title;
                if (selected.data.type != 2 || IsMissionLocked(profile, selected.data, planet.missionInfo[state.hordeStart])) { return 1; }
                const auto &map = planet.missionInfo[state.hordeStart].map;
                ZSurvivalGameContext context{profile, savePath};
                context.music = &music;
                context.hordeStart = static_cast<int>(state.hordeStart);
                const int result = RunFrontEndSurvival(CGame::Launch{bigDirectory, tables.GetPackName(map.packHash), map.localIndex, 0, -1, selected.data.value64, &context, profile.brotherEnabled, &selected, &window}, state);
                if (result == kDebugMapSessionChoice) { continue; }
                if (result != 0) { return 1; }
                BeginPostGame(state, context, weapons);
                continue;
            }
            // Explicit legacy .dat study path retains its historical fixture.
            std::vector<ZMissionEntry> missions;
            if (!LoadMissionCatalog(toc, tables, missions)) { return 1; }
            const unsigned packHash = toc.GetPack(toc.GetPackIndexFromName("pack11"))->GetPackHash();
            const ZMissionEntry *selected = nullptr;
            for (const ZMissionEntry &mission : missions) {
                if (mission.resource.packHash == packHash && mission.resource.localIndex == state.hordeStart && mission.data.type == 2) { selected = &mission; break; }
            }
            if (selected == nullptr) { return 1; }
            ZSurvivalGameContext context{profile, savePath};
            context.music = &music;
            context.hordeStart = static_cast<int>(state.hordeStart);
            const int result = RunFrontEndSurvival(CGame::Launch{bigDirectory, "pack11", 0, 0, -1, selected->data.value64, &context, profile.brotherEnabled, selected, &window}, state);
            if (result == kDebugMapSessionChoice) { continue; }
            if (result != 0) { return 1; }
            BeginPostGame(state, context, weapons);
            continue;
        }
        unsigned wave = profile.clearedWaves[choice];
        if (wave >= 500) { wave = 0; }
        if (state.starMap.startingWave >= 0) { wave = static_cast<unsigned>(state.starMap.startingWave); }
        ZSurvivalGameContext context{profile, savePath, static_cast<unsigned>(choice)};
        context.music = &music;
        std::string mapPack;
        unsigned mapIndex = 0;
        if (profile.nativeArchive) {
            const auto &level = profile.nativeArchive->survivalLevels[choice];
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(level.packHash, ZGameSection::Level, level.localIndex, bytes)) { return 1; }
            CArrayInputStream input(bytes);
            CLevel::Template data;
            if (!data.Init(input) || input.Available() != 0) { return 1; }
            mapPack = tables.GetPackName(data.mapRef.packHash);
            mapIndex = data.mapRef.localIndex;
        } else {
            // Explicit legacy research profiles retain their historical map fixture.
            mapPack = kPlanetPacks[choice];
            mapIndex = kPlanetMaps[choice];
        }
        const int result = RunFrontEndSurvival(CGame::Launch{bigDirectory, mapPack, mapIndex, 0, -1, wave, &context, profile.brotherEnabled, nullptr, &window}, state);
        if (result == kDebugMapSessionChoice) { continue; }
        if (result != 0) { return 1; }
        BeginPostGame(state, context, weapons);
    }
}

int RunGameFrontEnd(const std::string &bigDirectory, bool originalProfile,
    const std::string &profilePath, ZWindow *sharedWindow) {
    return RunGameMenuSession(bigDirectory, "", 0, originalProfile, profilePath, sharedWindow);
}
