#include "gun_bros_re/data/profile/CRefinementManager.h"
#include "gun_bros_re/data/profile/CPlayerProgress.h"
#include "gameplay/ProfilePlayDriver.h"
#include "gun_bros_re/debug/Capture.h"
#include "ui/GameMenuStudy.h"
#include "gameplay/SurvivalStudy.h"
#include "gun_bros_re/ui/host/ZMenuSession.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuMovieMultiplayerOverlay.h"
#include "gun_bros_re/ui/menus/CMenuMissionInfo.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/menus/CMenuGameResources.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/menus/CMenuList.h"
#include "gun_bros_re/ui/menus/CMenuGreeting.h"
#include "gun_bros_re/ui/host/ZLoadingScreen.h"
#include "gun_bros_re/ui/host/ZMenuWipe.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
#include "gun_bros_re/cheats/CheatActions.h"
#include "gun_bros_re/data/profile/CProfileManager.h"
#include "gun_bros_re/gameplay/powerup/CPowerup.h"
#include "gun_bros_re/startup/ZStartupSequence.h"
#include "engine/glu/sprite/CSpriteIterator.h"
#include "TestOutput.h"
#include "ui/MenuChecks.h"
#include "gun_bros_re/debug/DebugTutorial.h"
#include "gun_bros_re/debug/SurvivalDebug.h"
#include "gun_bros_re/debug/DebugConfig.h"
#include <SDL3/SDL.h>
using namespace MenuDetail;



int RunTutorialPlayCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    CGunBros tables(toc);
    CRefinementManager::Template refinement;
    if (!CRefinementManager::Template::Load(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    CGameFlow context{profile, TestOutput::Path("tutorial-profile-check.dat"), 0};
    context.tutorial = true;
    if (RunSurvivalStudy(bigDirectory, "pack2", 7, 0, -1, "", 0, false, false, true, 2, 0, &context, true) != 0) { return 1; }
    CProfileManager restored;
    restored.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!restored.LoadFromDisk(context.savePath) || !restored.tutorialCompleted || restored.tutorialSteps != 255 ||
        restored.configuration.guns[1].localIndex != 4) { return 1; }
    std::printf("[tutorial-profile-check] completed=1 steps=255 rifle=4 restored=1\n");
    // Repeat with a genuinely absent original save source. The GUI's first
    // brother choice was checked separately; now verify its native play seam.
    const auto nativePath = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) / ("tutorial-native-" + std::to_string(GetTickCount64()));
    CProfileManager native;
    native.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!(native).LoadNative(toc, tables, nativePath, nativePath / "absent-source")) { return 1; }
    native.firstLaunch = false;
    if (!native.SaveToDisk(nativePath)) { return 1; }
    const auto &level = native.nativeArchive->survivalLevels[0];
    std::vector<std::uint8_t> payload;
    if (!tables.ReadSectionResource(level.packHash, ZGameSection::Level, level.localIndex, payload)) { return 1; }
    CArrayInputStream input(payload);
    CLevel::Template data;
    if (!data.Init(input) || input.Available() != 0) { return 1; }
    CGameFlow nativeContext{native, nativePath, 0};
    nativeContext.tutorial = true;
    if (RunSurvivalStudy(bigDirectory, tables.GetPackName(data.mapRef.packHash), data.mapRef.localIndex,
        0, -1, "", 0, false, false, true, 2, 0, &nativeContext, true) != 0) { return 1; }
    if (!native.tutorialCompleted || native.tutorialSteps != 255) { return 1; }
    const auto earnedRifle = native.configuration.guns[1];
    if (!(native).LoadNative(toc, tables, nativePath, nativePath / "absent-source") || native.firstLaunch ||
        !SameObject(native.configuration.guns[1], earnedRifle) || !native.Owns(6, earnedRifle)) { return 1; }
    // Tutorial steps are a host execution trace, not an invented original flag.
    std::printf("[tutorial-profile-check] native created-without-source original-HUD completed=1 steps=255 earned-rifle-restored=1\n");
    CProfileManager debugProfile;
    debugProfile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    CGameFlow debugContext{debugProfile, {}};
    CGame::Launch debugLaunch;
    debugLaunch.bigDirectory = bigDirectory;
    if (!PrepareDebugTutorial(toc, tables, debugContext, debugLaunch) ||
        !debugContext.tutorial || !debugContext.debugTutorial || debugContext.persistProgress ||
        debugProfile.experience != 0) { return 1; }
    // A non-empty sentinel catches accidental writes through either save path.
    debugContext.savePath = TestOutput::Path("tutorial-must-not-save");
    if (std::filesystem::exists(debugContext.savePath)) { return 1; }
    if (RunSurvivalStudy(bigDirectory, debugLaunch.packShortName, debugLaunch.mapIndex,
        0, -1, "", 0, false, false, true, 2, 0, &debugContext, true) != 0 ||
        !debugContext.SaveProfile() || std::filesystem::exists(debugContext.savePath)) { return 1; }
    std::printf("[tutorial-profile-check] debug full-tutorial=1 no-save=1\n");

    // Exercise ESC through the production key dispatcher with a fresh replay.
    if (!PrepareDebugTutorial(toc, tables, debugContext, debugLaunch)) { return 1; }
    debugContext.savePath = TestOutput::Path("tutorial-must-not-save");
    ZWindow debugWindow;
    if (!debugWindow.Open("Debug tutorial checks", 1024, 768)) { return 1; }
    debugWindow.SetEscapeCloses(false);
    SDL_Event escape{};
    escape.type = SDL_EVENT_KEY_DOWN;
    escape.key.key = SDLK_ESCAPE;
    if (!SDL_PushEvent(&escape)) { return 1; }
    escape.type = SDL_EVENT_KEY_UP;
    if (!SDL_PushEvent(&escape)) { return 1; }
    debugLaunch.window = &debugWindow;
    if (CGame::Run(debugLaunch) != 0 || !debugWindow.IsOpen() ||
        std::filesystem::exists(debugContext.savePath)) { return 1; }
    ZMovieRenderer debugMovies;
    auto &core = *toc.GetPack(toc.GetCorePackIndex());
    if (!debugMovies.Init(core, core)) { return 1; }
    for (unsigned phase = 0; phase < 2; ++phase) {
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        DrawTutorialDebugNotice(debugMovies, phase * DebugConfig::Tutorial::BlinkMs);
        if (debugMovies.Failures() != 0 || !Capture::SaveFrame(debugWindow,
            TestOutput::Path("tutorial-notice-" + std::to_string(phase) + ".png"))) { return 1; }
    }
    std::printf("[tutorial-profile-check] debug escape=1 window-open=1 no-save=1 notice-phases=2\n");
    CPlayerProgress::Template progressData;
    std::vector<CStoreItem::Entry> store;
    std::vector<CGun::Entry> weapons;
    std::vector<CArmor::Entry> armor;
    if (!CPlayerProgress::Template::Load(toc, tables, progressData) || !CStoreItem::LoadEntries(toc, tables, store) ||
        !CGun::LoadEntries(toc, tables, weapons) || !CArmor::LoadEntries(toc, tables, armor)) { return 1; }
    CMenuSystem menuState;
    menuState.resumeAfterDebugTutorial = true;
    SDL_Event replay{};
    replay.type = SDL_EVENT_KEY_DOWN;
    replay.key.key = SDLK_T;
    replay.key.mod = SDL_KMOD_LSHIFT;
    if (!SDL_PushEvent(&replay)) { return 1; }
    replay.type = SDL_EVENT_KEY_UP;
    replay.key.mod = SDL_KMOD_NONE;
    if (!SDL_PushEvent(&replay)) { return 1; }
    // This debug shortcut requires the host gate; isolated test configs default it off.
    const bool previousDebugMode = GameHostSettings().debugMode;
    GameHostSettings().debugMode = true;
    const int menuResult = ShowGameMenu(toc, tables, debugProfile, progressData, refinement,
        store, weapons, armor, menuState, debugContext.savePath, "", nullptr, false, &debugWindow);
    GameHostSettings().debugMode = previousDebugMode;
    if (menuResult != kDebugTutorialMenuChoice || std::filesystem::exists(debugContext.savePath)) { return 1; }
    std::printf("[tutorial-profile-check] menu shift-T=1 return-checkpoint-skipped=1 no-save=1\n");
    return 0;
}

int RunProfilePlayCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    CGunBros tables(toc);
    CRefinementManager::Template refinement;
    std::vector<CGun::Entry> weapons;
    if (!CRefinementManager::Template::Load(toc, tables, refinement) || !CGun::LoadEntries(toc, tables, weapons)) { return 1; }
    CProfileManager profile;
    const unsigned core = toc.GetPack(toc.GetCorePackIndex())->GetPackHash();
    profile.Reset(core, refinement);
    // Only this isolated test account gets a high-damage original gun.
    GameObjectRef gun;
    gun.packHash = weapons[65].packHash;
    gun.localIndex = static_cast<std::uint8_t>(weapons[65].ordinal);
    profile.Grant(6, gun);
    profile.configuration.guns[0] = gun;
    CGameFlow context{profile, TestOutput::Path("game-profile-check.dat"), 0};
    profile.warbucks = 50;
    ProfilePlayDriver controls;
    // This regression exercises real SDL transport at zero device gain.
    // Keep the host alive across both sessions, as the formal front end does.
    ZWindow window;
    if (!window.Open("Profile and pause audio verification", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
    CBGM music;
    music.EnableSilentValidation();
    context.music = &music;
    if (RunSurvivalStudy(bigDirectory, "pack2", 7, 0, -1, "", 0, false, false, true, 2, 0,
        &context, false, false, nullptr, false, &window, false, false, false, &controls) != 0) { return 1; }
    const std::uint64_t firstExperience = profile.experience;
    const std::uint64_t firstXplodium = profile.xplodium;
    CProfileManager restored;
    restored.Reset(core, refinement);
    if (!restored.LoadFromDisk(context.savePath) || restored.experience == 0 || restored.xplodium == 0 ||
        restored.clearedWaves[0] != 2 || restored.configuration.guns[0].packHash != gun.packHash) { return 1; }
    CGameFlow continued{restored, context.savePath, 0};
    continued.music = &music;
    if (RunSurvivalStudy(bigDirectory, "pack2", 7, 0, -1, "", 0, false, false, true, 2, 2,
        &continued, false, false, nullptr, false, &window) != 0) { return 1; }
    if (restored.experience <= firstExperience || restored.xplodium <= firstXplodium || restored.clearedWaves[0] != 4) { return 1; }
    const auto playback = music.GetPlaybackState();
    if (playback.paused || playback.voices != 1 || playback.queuedBytes <= 0 ||
        std::abs(playback.volume - 0.3f) > 0.001f) { return 1; }
    std::printf("[profile-play-check] resumed=2 completed=4 xp=%llu xplodium=%llu failures=0\n",
        restored.experience, restored.xplodium);
    return 0;
}

int RunPlayerSelectCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    CGunBros tables(toc);
    CRefinementManager::Template refinement;
    if (!CRefinementManager::Template::Load(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) / ("select-check-" + std::to_string(GetTickCount64()));
    // Missing source exercises native constructor defaults as requested.
    if (!(profile).LoadNative(toc, tables, path, {})) { return 1; }
    ZMenuSurface view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_PLAYER_SELECT");
    auto *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, start, end)) { return 1; }
    const auto coins = profile.coins, warbucks = profile.warbucks;
    for (unsigned mode = 0; mode < 2; ++mode) {
        for (unsigned brother = 0; brother < 2; ++brother) {
            CMenuSystem state;
            state.stack.page = 25;
            if (mode != 0) { state.stack.page = 29; }
            profile.firstLaunch = mode == 0;
            const auto initialBrother = profile.playerBrother;
            ZMovieRegion area;
            if (!view.movies.Region(ordinal, brother + 1, 0, area)) { return 1; }
            const ZMenuInputFrame click{area.x + area.width / 2, area.y + area.height / 2};
            bool launch = false;
            view.Begin();
            view.inputEnabled = true;
            view.InjectTap(click);
            if (!FinishMenuFrame(state.selection.Draw(view, state, profile, path, launch), state) || launch ||
                state.selection.playerSelection != -1 || profile.playerBrother != initialBrother) { return 1; }
            view.clock += end + 1;
            view.Begin();
            view.inputEnabled = true;
            if (!FinishMenuFrame(state.selection.Draw(view, state, profile, path, launch), state)) { return 1; }
            if (mode == 0 && brother == 0 && !Capture::SaveFrame(view.window, TestOutput::Path("ui-original-2026-09-09/player-select-original-ready.png"))) { return 1; }
            view.inputEnabled = true;
            view.InjectTap(click);
            if (!FinishMenuFrame(state.selection.Draw(view, state, profile, path, launch), state) || launch ||
                state.selection.playerSelection != static_cast<int>(brother) || profile.playerBrother != brother || profile.firstLaunch) { return 1; }
            unsigned selectedStart = 0, selectedEnd = 0;
            if (!movie->GetChapterRange(state.selection.playerSelectChapter, selectedStart, selectedEnd) || state.selection.playerSelectTime != selectedStart) { return 1; }
            view.clock += (selectedEnd - selectedStart) / 2;
            view.Begin();
            view.inputEnabled = true;
            if (!FinishMenuFrame(state.selection.Draw(view, state, profile, path, launch), state) || launch) { return 1; }
            if (mode == 0 && !Capture::SaveFrame(view.window, TestOutput::Path("ui-original-2026-09-09/player-select-original-") + std::to_string(brother) + ".png")) { return 1; }
            view.clock += selectedEnd - selectedStart + 1;
            view.Begin();
            if (!FinishMenuFrame(state.selection.Draw(view, state, profile, path, launch), state) || launch != (mode == 0)) { return 1; }
            if (mode != 0 && state.stack.page != 6) { return 1; }
            CProfileManager reloaded;
            reloaded.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
            if (!(reloaded).LoadNative(toc, tables, path, {}) || reloaded.playerBrother != brother ||
                reloaded.firstLaunch || reloaded.coins != coins || reloaded.warbucks != warbucks) { return 1; }
        }
    }
    std::printf("[player-select-check] native-new-profile=1 brothers=2 original-hit=4 opening-gate=4 chapter-gate=4 native-reload=4 failures=0\n");
    return view.movies.Failures() != 0 || glGetError() != 0;
}

/** Replay input through the same PLAY callbacks as the GUI, with native saves. */
int RunPlayInteractionCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    CGunBros tables(toc);
    CProfileManager profile;
    const auto path = std::filesystem::path(TestOutput::Path("play-interaction-check")) / std::to_string(GetTickCount64());
    if (!(profile).LoadNative(toc, tables, path, TestOutput::Fixtures())) { return 1; }
    ZMenuSurface view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = true;
    CMenuSystem state;
    unsigned failures = 0;
    for (unsigned frame = 0; frame < 150; ++frame) {
        view.clock += 16; view.Begin(); view.InjectTap({-1, -1});
        if (!FinishMenuFrame(state.starMap.Draw(view, state, profile), state) || !state.mode.Draw(view, state)) { return 1; }
    }
    const bool autoSelected = state.starMap.starSelectedSlot == 1 && state.starMap.starLocked;
    if (!autoSelected) { ++failures; }
    std::printf("[play-interaction] auto-selected=%d slot=%d locked=%d failures=%u\n", autoSelected, state.starMap.starSelectedSlot, state.starMap.starLocked, failures);
    const unsigned modeOrdinal = view.movies.Ordinal("GLU_MOVIE_MULTIPLAYER_AND_VERSUS_MAP");
    ZMovieRegion mode;
    if (!view.movies.Region(modeOrdinal, 1, state.mode.modeTime, mode)) { return 1; }
    view.Begin(); view.InjectTap({mode.x + mode.width / 2, mode.y + mode.height / 2});
    if (!state.mode.Draw(view, state)) { return 1; }
    const unsigned before = state.mode.modeTime;
    view.clock += 80; view.Begin(); view.InjectTap({-1, -1});
    if (!state.mode.Draw(view, state)) { return 1; }
    const bool animated = state.mode.modeTime != before && state.mode.modePhase == 1;
    if (!animated || view.modeEffects.ModeParticleCount() == 0) { ++failures; }
    if (!Capture::SaveFrame(view.window, (path / "mode-select.png").string())) { return 1; }
    std::printf("[play-interaction] mode-intermediate=%d time=%u..%u failures=%u\n", animated, before, state.mode.modeTime, failures);
    view.clock += 3000; view.Begin(); view.InjectTap({-1, -1});
    if (!state.mode.Draw(view, state)) { return 1; }
    state.Navigate(21);
    state.UpdateNavigation();
    view.animateNavigation = false;
    bool launch = false;
    view.Begin(); view.InjectTap({-1, -1});
    if (!FinishMenuFrame(state.missions.Draw(view, state, profile, launch), state)) { return 1; }
    const unsigned mainOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_MENU");
    const unsigned backOrdinal = view.movies.Ordinal("GLU_MOVIE_BACK_BUTTON");
    unsigned start = 0, end = 0;
    ZMovieRegion planet, back;
    if (!view.movies.GetMovie(mainOrdinal)->GetChapterRange(0, start, end) ||
        !view.movies.Region(mainOrdinal, 0, end, planet) ||
        !view.movies.GetMovie(backOrdinal)->GetChapterRange(0, start, end)) { return 1; }
    for (const auto &region : view.movies.Regions(backOrdinal, end, planet.x + planet.width / 2, planet.y + planet.height / 2, true)) {
        if (region.index == 0) { back = region; }
    }
    view.Begin(); view.InjectTap({std::max(1.0f, back.x + back.width / 2), back.y + back.height / 2});
    std::printf("[play-interaction] back-bounds=%.1f,%.1f %.1fx%.1f frame=%u mission=%u\n", back.x, back.y, back.width, back.height, end, state.missions.missionTime);
    if (!FinishMenuFrame(state.missions.Draw(view, state, profile, launch), state)) { return 1; }
    if (state.stack.page != 0) { ++failures; }
    std::printf("[play-interaction] back-page=%u failures=%u\n", state.stack.page, failures);
    state.Navigate(21);
    state.UpdateNavigation(); state.missions.missionBound = false;
    view.Begin(); view.InjectTap({-1, -1});
    if (!FinishMenuFrame(state.missions.Draw(view, state, profile, launch), state)) { return 1; }
    const unsigned listOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_LIST");
    ZMovieRegion viewport, first, second;
    if (!view.movies.GetMovie(listOrdinal)->GetChapterRange(1, start, end) ||
        !view.movies.Region(listOrdinal, 0, start, viewport) || !view.movies.Region(listOrdinal, 1, start, first) ||
        !view.movies.Region(listOrdinal, 2, start, second)) { return 1; }
    view.clock += 16; view.Begin();
    view.InjectTap({viewport.x + viewport.width / 2, viewport.y + viewport.height / 2});
    view.ExchangeClick(false); view.dragX = -(second.x - first.x) / 8;
    if (!FinishMenuFrame(state.missions.Draw(view, state, profile, launch), state)) { return 1; }
    const bool followsDrag = state.missions.missionListTime != start || state.missions.missionFirst != 0;
    if (!followsDrag) { ++failures; }
    std::printf("[play-interaction] list-follows-small-drag=%d time=%u rest=%u failures=%u\n", followsDrag, state.missions.missionListTime, start, failures);
    const float stride = second.x - first.x;
    // Equal flicks must travel equally at 30/60-ish desktop frame cadences.
    float referenceDistance = 0;
    for (unsigned frameMs : {16u, 32u}) {
        ZMenuScrollMotion motion;
        float position = 0;
        motion.Update(position, 16, -stride / 2, 0, true, true, true, stride * 100, stride, end - start + 1);
        unsigned clock = 16;
        while (clock < 1616) {
            clock += frameMs;
            motion.Update(position, clock, 0, 0, false, false, true, stride * 100, stride, end - start + 1);
        }
        if (frameMs == 16) { referenceDistance = position; }
        else if (std::abs(position - referenceDistance) > 0.1f) { ++failures; }
    }
    for (unsigned frame = 0; frame < 6; ++frame) {
        view.clock += 16; view.Begin();
        view.InjectTap({viewport.x + viewport.width / 2, viewport.y + viewport.height / 2});
        view.ExchangeClick(false); view.dragX = -stride / 2;
        if (!FinishMenuFrame(state.missions.Draw(view, state, profile, launch), state)) { return 1; }
    }
    const float released = state.missions.missionPosition;
    view.clock += 16; view.Begin(); view.InjectTap({-1, -1});
    if (!FinishMenuFrame(state.missions.Draw(view, state, profile, launch), state)) { return 1; }
    if (state.missions.missionPosition <= released || released < stride * 3) { ++failures; }
    for (unsigned frame = 0; frame < 180; ++frame) {
        view.clock += 16; view.Begin(); view.InjectTap({-1, -1});
        if (!FinishMenuFrame(state.missions.Draw(view, state, profile, launch), state)) { return 1; }
    }
    const float maximum = (view.planets.planetEntries[state.planet].missions.size() - 3) * stride;
    if (state.missions.missionPosition > maximum || state.missions.missionMotion.velocity != 0) { ++failures; }
    if (!Capture::SaveFrame(view.window, (path / "revolutions-after-flick.png").string())) { return 1; }
    std::printf("[play-interaction] list-release=%.1f coast=%.1f maximum=%.1f failures=%u\n", released, state.missions.missionPosition, maximum, failures);
    // Focus a real visible REV and swipe its wave selector continuously.
    ZMovieRegion card;
    if (!view.movies.Region(listOrdinal, 1, state.missions.missionListTime, card)) { return 1; }
    view.Begin(); view.InjectTap({card.x + card.width / 2, card.y + card.height / 2});
    if (!FinishMenuFrame(state.missions.Draw(view, state, profile, launch), state) || state.missions.missionFocused < 0) { return 1; }
    for (unsigned frame = 0; frame < 65; ++frame) {
        view.clock += 16; view.Begin(); view.InjectTap({-1, -1});
        if (!FinishMenuFrame(state.missions.Draw(view, state, profile, launch), state)) { return 1; }
    }
    const unsigned boxOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_BOX");
    ZMovieRegion box, waves;
    if (!view.movies.Region(boxOrdinal, 0, state.missions.missionCardTime, box)) { return 1; }
    for (const auto &region : view.movies.Regions(boxOrdinal, state.missions.missionCardTime,
        kMenuWidth / 2 - box.width / 2, kMenuHeight / 2 - box.height / 2)) {
        if (region.index == 8) { waves = region; }
    }
    const float waveBefore = state.missions.wavePosition;
    view.clock += 16; view.Begin();
    view.InjectTap({waves.x + waves.width / 2, waves.y + waves.height / 2});
    view.ExchangeClick(false); view.dragX = waves.width * 3;
    if (!FinishMenuFrame(state.missions.Draw(view, state, profile, launch), state)) { return 1; }
    const float waveLeft = state.missions.wavePosition;
    view.clock += 16; view.Begin();
    view.InjectTap({waves.x + waves.width / 2, waves.y + waves.height / 2});
    view.ExchangeClick(false); view.dragX = -waves.width * 5;
    if (!FinishMenuFrame(state.missions.Draw(view, state, profile, launch), state)) { return 1; }
    if (state.missions.wavePosition <= waveLeft || state.missions.wavePage < 2 || launch) { ++failures; }
    if (!Capture::SaveFrame(view.window, (path / "waves-after-drag.png").string())) { return 1; }
    std::printf("[play-interaction] waves-before=%.1f left=%.1f right=%.1f page=%u failures=%u\n", waveBefore, waveLeft, state.missions.wavePosition, state.missions.wavePage, failures);
    // The same radial back action must close an expanded card, then the planet.
    for (unsigned press = 0; press < 2; ++press) {
        view.Begin(); view.InjectTap({std::max(1.0f, back.x + back.width / 2), back.y + back.height / 2});
        if (!FinishMenuFrame(state.missions.Draw(view, state, profile, launch), state)) { return 1; }
        if (press == 0 && !state.missions.missionClosing) { ++failures; }
        for (unsigned frame = 0; frame < 40 && state.stack.page == 21; ++frame) {
            view.clock += 16; view.Begin(); view.InjectTap({-1, -1});
            if (!FinishMenuFrame(state.missions.Draw(view, state, profile, launch), state)) { return 1; }
        }
    }
    if (state.stack.page != 0) { ++failures; }
    std::printf("[play-interaction] nested-back-page=%u failures=%u\n", state.stack.page, failures);
    return failures != 0;
}

/** Actual menu preview and scene handoffs, with isolated save data. */
int RunAudioTransitionsCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    CGunBros tables(toc);
    CPlayerProgress::Template progress;
    CRefinementManager::Template refinement;
    std::vector<CStoreItem::Entry> store;
    std::vector<CGun::Entry> weapons;
    std::vector<CArmor::Entry> armor;
    if (!CPlayerProgress::Template::Load(toc, tables, progress) || !CRefinementManager::Template::Load(toc, tables, refinement) ||
        !CStoreItem::LoadEntries(toc, tables, store) || !CGun::LoadEntries(toc, tables, weapons) ||
        !CArmor::LoadEntries(toc, tables, armor)) { return 1; }
    const auto savePath = std::filesystem::path(TestOutput::Path("audio-transitions")) / std::to_string(GetTickCount64());
    CProfileManager profile;
    // Match the front end: native saves restore slot state but do not bind
    // the BIG refinery template needed after the postgame -> refinery transition.
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!(profile).LoadNative(toc, tables, savePath, TestOutput::Fixtures())) { return 1; }
    ZWindow window;
    if (!window.Open("Audio transition verification", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
    unsigned failures = 0;
    {
        ZMenuSurface view(&window);
        if (!view.Open(toc, tables, &profile)) { return 1; }
        view.playerPreview.EnableSilentPreviewAudio();
        ZMovieRegion panel;
        if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_STORE_MENU"), 2, 0, panel)) { return 1; }
        for (unsigned actor = 0; actor < 2; ++actor) {
            profile.playerBrother = actor;
            if (!view.playerPreview.Draw(view, toc, tables, profile, weapons, armor, 0, nullptr, &panel)) { return 1; }
            for (unsigned tick = 0; tick < 300; ++tick) { view.playerPreview.AdvancePlayerPreview(16); }
            for (unsigned exchange = 0; exchange < 2; ++exchange) {
                const unsigned slot = 1 - view.playerPreview.GetPlayerPreviewSlot();
                const auto before = view.playerPreview.PreviewSoundCount();
                if (!view.playerPreview.Draw(view, toc, tables, profile, weapons, armor, slot, nullptr, &panel)) { return 1; }
                for (unsigned tick = 0; tick < 300; ++tick) { view.playerPreview.AdvancePlayerPreview(16); }
                const auto sounds = view.playerPreview.PreviewSoundCount() - before;
                if (sounds == 0 || view.playerPreview.GetPlayerPreviewSlot() != slot || view.playerPreview.PreviewAudioState().voices == 0) { ++failures; }
                std::printf("[audio-transition-check] store brother=%u slot=%u sounds=%zu failures=%u\n", actor, slot, sounds, failures);
            }
        }
    }
    CBGM music;
    music.EnableSilentValidation();
    if (!music.Play(0)) { return 1; }
    CMenuSystem state;
    state.stack.page = 2;
    unsigned starts = CBGM::GetPlaybackStarts();
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state,
        savePath, TestOutput::Path("audio-transition-menu.png"), nullptr, false, &window, false, nullptr, &music) != -2) { return 1; }
    if (CBGM::GetPlaybackStarts() != starts || music.GetTrack() != 0) { ++failures; }
    std::printf("[audio-transition-check] menu retained=%d extra-starts=%u failures=%u\n",
        music.GetTrack() == 0, CBGM::GetPlaybackStarts() - starts, failures);
    CGameFlow context{profile, savePath, 0};
    context.music = &music;
    if (RunSurvivalStudy(bigDirectory, "pack2", 7, 0, -1, "", 0, false, false, true, 2, 0,
        &context, false, false, nullptr, false, &window) != 0) { return 1; }
    const int battleTrack = music.GetTrack();
    if (battleTrack <= 0) { ++failures; }
    starts = CBGM::GetPlaybackStarts();
    music.SetPaused(true); // Re-entering menus must resume the retained battle track.
    music.SetVolume(0.5f); // Leaving pause for results must restore the scene gain.
    state.postGame.Refresh(state, context, weapons);
            state.UpdateNavigation();
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state,
        savePath, TestOutput::Path("audio-transition-postgame.png"), nullptr, false, &window, false, nullptr, &music) != -2) { return 1; }
    if (music.GetTrack() != battleTrack || CBGM::GetPlaybackStarts() != starts ||
        std::abs(music.GetPlaybackState().volume - 0.3f) > 0.001f) { ++failures; }
    std::printf("[audio-transition-check] postgame track=%d expected=%d extra-starts=%u failures=%u\n",
        music.GetTrack(), battleTrack, CBGM::GetPlaybackStarts() - starts, failures);
    // Drive the real authored close timer; existing postgame checks cover hitboxes.
    state.stack.page = 27;
    state.postGame.postGameClosing = true;
    state.postGame.postGameCloseTime = 100000;
    profile.xplodium = 1; // Explicit test fixture chooses the refinery branch.
    const std::vector<ZMenuInputFrame> closeTicks{{-100, -100, 16}, {-100, -100, 16}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state,
        savePath, TestOutput::Path("audio-transition-refinery.png"), &closeTicks, false, &window, false, nullptr, &music) != -2) { return 1; }
    if (state.stack.page != 3 || music.GetTrack() != 0) { ++failures; }
    std::printf("[audio-transition-check] refinery page=%u track=%d failures=%u\n", state.stack.page, music.GetTrack(), failures);
    const auto playback = music.GetPlaybackState();
    if (playback.voices != 1 || playback.devicesOpened != 1 || playback.streamsCreated != 1 ||
        playback.queuedBytes <= 0 || playback.paused) { ++failures; }
    // Muting is a gain change; switching tracks must not silently enable music.
    music.SetEnabled(false);
    if (!music.NextTrack() || music.GetPlaybackState().volume != 0) { ++failures; }
    music.SetEnabled(true);
    if (std::abs(music.GetPlaybackState().volume - 0.3f) > 0.001f) { ++failures; }
    music.SetVolume(0.5f);
    music.SetEnabled(false);
    if (music.GetPlaybackState().volume != 0) { ++failures; }
    music.SetEnabled(true);
    if (std::abs(music.GetPlaybackState().volume - 0.15f) > 0.001f || music.GetPlaybackState().paused) { ++failures; }
    music.SetVolume(1.0f);
    std::printf("[audio-transition-check] real-SDL voices=%u devices=%u streams=%u queued=%lld pause=%d settings-preserved failures=%u\n",
        playback.voices, playback.devicesOpened, playback.streamsCreated, playback.queuedBytes, playback.paused, failures);
    return failures != 0;
}

int RunSceneTransitionCheck(const std::string &bigDirectory) {
    ZWindow window;
    if (!window.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
    const unsigned generation = window.GetSurfaceGeneration();
    const unsigned surface = window.GetSurfaceId();
    int initialX = 0, initialY = 0, initialWidth = 0, initialHeight = 0;
    window.GetPosition(initialX, initialY);
    window.GetDrawableSize(initialWidth, initialHeight);
    ZPNGImage pixel;
    pixel.width = 1; pixel.height = 1; pixel.pixels = {23, 45, 67, 255};
    ZTexture witness;
    if (!witness.Create(pixel)) { return 1; }
    unsigned failures = 0;
    for (unsigned scene = 0; scene < 4; ++scene) {
        int result = 0;
        if (scene == 0) {
            result = RunStartupSequence(TestOutput::Path("scene-transition-logo.png"), 2000, &window);
        } else if (scene == 2) {
            result = RunSurvivalStudy(bigDirectory, "pack2", 7, 0, -1, TestOutput::Path("scene-transition-game.png"),
                0, false, false, false, 2, 0, nullptr, false, false, nullptr, false, &window);
        } else {
            result = RunGameMenuStudy(bigDirectory, TestOutput::Path("scene-transition-menu.png"), 2, false,
                TestOutput::Path("scene-transition-profile"), &window);
        }
        int x = 0, y = 0, width = 0, height = 0;
        window.GetPosition(x, y);
        window.GetDrawableSize(width, height);
        const bool retained = window.GetSurfaceId() == surface && window.GetSurfaceGeneration() == generation &&
            x == initialX && y == initialY && width == initialWidth && height == initialHeight && glIsTexture(witness.GetHandle());
        std::printf("[scene-transition-check] scene=%u result=%d generation=%u retained=%d\n",
            scene, result, window.GetSurfaceGeneration(), retained);
        if (result != 0 || !retained) { ++failures; }
    }
    return failures != 0;
}
