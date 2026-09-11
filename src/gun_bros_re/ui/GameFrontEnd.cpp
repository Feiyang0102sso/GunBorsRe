#include "gun_bros_re/ui/GameFrontEndInternal.h"
#include "engine/core/Paths.h"
#include "gun_bros_re/ui/MenuInternal.h"
using namespace MenuDetail;

namespace {
int RunFrontEndSurvival(SurvivalLaunch launch, MenuState &state) {
#if GB_ENABLE_TESTS
    launch.debugSelection = &state.debugMap;
#endif
    return RunSurvival(launch);
}
}

int RunGameMenuSession(const std::string &bigDirectory, const std::string &screenshotPath, unsigned page, bool originalProfile,
    const std::string &profilePath, CWindow *sharedWindow) {
    CWindow ownedWindow;
    CWindow &window = sharedWindow ? *sharedWindow : ownedWindow;
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    if (!tables.HasLatestBigVersion()) {
        std::printf("[game] BigVersion 1 required; older formats are supported for resource viewing only\n");
        return 1;
    }
    CPlayerProgress::Template progress;
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    {
        CWindow &loadingWindow = window;
        if (!loadingWindow.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
        MovieRenderer loadingMovies;
        CResPackTOC *core = toc.GetPack(toc.GetCorePackIndex());
        if (!loadingMovies.Init(*core, *core)) { return 1; }
        LoadingScreen loading(loadingWindow, loadingMovies, tables, nullptr, false, screenshotPath.empty() || page == 14);
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
        if (!LoadNativeProfile(toc, tables, profile, savePath)) {
            std::printf("[game] native profile cannot be loaded; files preserved: %s\n", savePath.u8string().c_str());
            return 1;
        }
    }
    CBGM music; // Lifetime includes every menu, loading screen and game session.
    MenuState state;
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
#if GB_ENABLE_TESTS
        if (state.debugMap.ready) {
            // Research sessions own their BGM; suspend the shared menu track.
            music.SetPaused(true);
            RunDebugMaps(bigDirectory, window, state.debugMap, profile);
            music.SetPaused(false);
        }
#endif
        const int choice = ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state, savePath, screenshotPath, nullptr, originalProfile, &window, false, nullptr, &music);
#if GB_ENABLE_TESTS
        if (choice == kDebugMapMenuChoice) { continue; }
#endif
        if (choice == -3) { return 1; }
        if (choice == -2) { return 0; }
        if (choice < 0) { return !profile.SaveToDisk(savePath); }
        if (choice == 5) {
            SurvivalGameContext context{profile, savePath, 0};
            context.music = &music;
            context.tutorial = true;
            std::string tutorialPack = "pack2";
            unsigned tutorialMap = 7;
            if (profile.nativeArchive) {
                const auto &level = profile.nativeArchive->survivalLevels[0];
                std::vector<std::uint8_t> bytes;
                if (!tables.ReadSectionResource(level.packHash, GameSection::Level, level.localIndex, bytes)) { return 1; }
                CArrayInputStream input(bytes);
                CLevel::Template data;
                if (!data.Init(input) || input.Available() != 0) { return 1; }
                tutorialPack = tables.GetPackName(data.mapRef.packHash);
                tutorialMap = data.mapRef.localIndex;
            }
            const int result = RunFrontEndSurvival(SurvivalLaunch{bigDirectory, tutorialPack, tutorialMap, 0, -1, 0, &context, true, nullptr, &window}, state);
#if GB_ENABLE_TESTS
            if (result == kDebugMapSessionChoice) { continue; }
#endif
            if (result != 0) { return 1; }
            if (profile.tutorialCompleted) { BeginPostGame(state, context, weapons); }
            else { state.Navigate(25, true); }
            continue;
        }
        if (choice == 4) {
            if (profile.nativeArchive) {
                std::vector<PlanetEntry> planets;
                if (!LoadPlanetCatalog(toc, tables, planets) || state.planet >= planets.size()) { return 1; }
                const auto &planet = planets[state.planet];
                if (state.hordeStart >= planet.missions.size() ||
                    !SameObject(state.selectedMission, planet.data.missions[state.hordeStart])) { return 1; }
                MissionEntry selected;
                selected.resource = state.selectedMission;
                selected.data = planet.missions[state.hordeStart];
                selected.title = planet.missionInfo[state.hordeStart].title;
                if (selected.data.type != 2 || OriginalMissionLocked(profile, selected.data, planet.missionInfo[state.hordeStart])) { return 1; }
                const auto &map = planet.missionInfo[state.hordeStart].map;
                SurvivalGameContext context{profile, savePath};
                context.music = &music;
                context.hordeStart = static_cast<int>(state.hordeStart);
                const int result = RunFrontEndSurvival(SurvivalLaunch{bigDirectory, tables.GetPackName(map.packHash), map.localIndex, 0, -1, selected.data.value64, &context, profile.brotherEnabled, &selected, &window}, state);
#if GB_ENABLE_TESTS
                if (result == kDebugMapSessionChoice) { continue; }
#endif
                if (result != 0) { return 1; }
                BeginPostGame(state, context, weapons);
                continue;
            }
            // Explicit legacy .dat study path retains its historical fixture.
            std::vector<MissionEntry> missions;
            if (!LoadMissionCatalog(toc, tables, missions)) { return 1; }
            const unsigned packHash = toc.GetPack(toc.GetPackIndexFromName("pack11"))->GetPackHash();
            const MissionEntry *selected = nullptr;
            for (const MissionEntry &mission : missions) {
                if (mission.resource.packHash == packHash && mission.resource.localIndex == state.hordeStart && mission.data.type == 2) { selected = &mission; break; }
            }
            if (selected == nullptr) { return 1; }
            SurvivalGameContext context{profile, savePath};
            context.music = &music;
            context.hordeStart = static_cast<int>(state.hordeStart);
            const int result = RunFrontEndSurvival(SurvivalLaunch{bigDirectory, "pack11", 0, 0, -1, selected->data.value64, &context, profile.brotherEnabled, selected, &window}, state);
#if GB_ENABLE_TESTS
            if (result == kDebugMapSessionChoice) { continue; }
#endif
            if (result != 0) { return 1; }
            BeginPostGame(state, context, weapons);
            continue;
        }
        unsigned wave = profile.clearedWaves[choice];
        if (wave >= 500) { wave = 0; }
        if (state.starMap.startingWave >= 0) { wave = static_cast<unsigned>(state.starMap.startingWave); }
        SurvivalGameContext context{profile, savePath, static_cast<unsigned>(choice)};
        context.music = &music;
        std::string mapPack;
        unsigned mapIndex = 0;
        if (profile.nativeArchive) {
            const auto &level = profile.nativeArchive->survivalLevels[choice];
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(level.packHash, GameSection::Level, level.localIndex, bytes)) { return 1; }
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
        const int result = RunFrontEndSurvival(SurvivalLaunch{bigDirectory, mapPack, mapIndex, 0, -1, wave, &context, profile.brotherEnabled, nullptr, &window}, state);
#if GB_ENABLE_TESTS
        if (result == kDebugMapSessionChoice) { continue; }
#endif
        if (result != 0) { return 1; }
        BeginPostGame(state, context, weapons);
    }
}

int RunGameFrontEnd(const std::string &bigDirectory, bool originalProfile,
    const std::string &profilePath, CWindow *sharedWindow) {
    return RunGameMenuSession(bigDirectory, "", 0, originalProfile, profilePath, sharedWindow);
}
