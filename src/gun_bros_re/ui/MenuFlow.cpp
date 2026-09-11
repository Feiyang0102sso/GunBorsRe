#include "gun_bros_re/DebugKeys.h"
#include "gun_bros_re/ui/MenuInternal.h"
namespace MenuDetail {

/** Returns selected planet, -1 for quit, -2 after capture, -3 on failure. */
int ShowGameMenu(CResTOCManager &toc, PackTables &tables, CProfileManager &profile,
    const CPlayerProgress::Template &progressData, const CRefinementManager::Template &refinement,
    const std::vector<StoreEntry> &store, const std::vector<WeaponEntry> &weapons,
    const std::vector<ArmorEntry> &armors, MenuState &state, const std::filesystem::path &savePath,
    const std::string &capturePath, const std::vector<MenuTestClick> *testClicks , bool originalProfile , CWindow *sharedWindow , bool testTransitions , MenuTransitionTrace *transitionTrace , CBGM *sharedMusic ) {
    // CGunBros owns CBGM across loading, gameplay and postgame menus.
    GameMenu view(sharedWindow);
    if (!view.window.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return -3; }
    CBGM ownedMusic;
    if (sharedMusic == nullptr) { sharedMusic = &ownedMusic; }
    CBGM &music = *sharedMusic;
    music.SetEnabled(profile.musicEnabled);
    music.SetPaused(false);
    music.SetVolume(1.0f);
    if (!state.postGame.postGameMusic && !music.Play(0)) { return -3; }
    CAudioPlayer::SetEffectsEnabled(profile.soundEnabled);
    if (!view.Open(toc, tables, &profile, state.page == 14, &music)) { return -3; }
#if GB_ENABLE_TESTS
    view.animateNavigation = capturePath.empty() || testTransitions;
    view.scripted = testClicks != nullptr;
#else
    view.animateNavigation = true;
#endif
    // A new view has a new clock (including deterministic capture sessions).
    state.store.shopFilterBound = false;
    state.settings.optionsBound = false;
    state.social.socialBound = false;
    state.starMap.starBound = false;
    state.mode.modeBound = false;
    CPlayerProgress progress;
    progress.Bind(progressData);
    progress.SetExperience(profile.experience);
#if GB_ENABLE_TESTS
    unsigned testFrame = 0;
#endif
#if GB_ENABLE_TESTS
    std::uint64_t testClock = 0;
#endif
    MenuWipe wipe;
    std::uint64_t wipeLastTick = 0;
    std::uint64_t frameTicks = view.window.GetTicksMs();
    float smoothFrameMs = 16.7f;
    CDailyBonusTracking daily;
    if (!daily.Load(toc, tables)) { return -3; }
    if (profile.nativeArchive) {
        daily.RefreshUsageData(profile, static_cast<std::uint32_t>(CurrentSeconds()));
        if (!profile.SaveToDisk(savePath)) { return -3; }
    }
    std::printf("[menu] ready page=%u\n", state.page);
    while (view.window.PumpEvents()) {
        const auto ticks = view.window.GetTicksMs();
        smoothFrameMs = smoothFrameMs * 0.9f + static_cast<float>(ticks - frameTicks) * 0.1f;
        frameTicks = ticks;
#if GB_ENABLE_TESTS
        if (testClicks != nullptr && testFrame < testClicks->size()) { testClock += (*testClicks)[testFrame].advanceMs; }
#endif
        std::uint64_t menuClock = ticks;
#if GB_ENABLE_TESTS
        if (testClicks != nullptr) { menuClock = testClock; }
#endif
        view.clock = menuClock;
        unsigned wipeDelta = 0;
        if (wipeLastTick != 0) { wipeDelta = static_cast<unsigned>(menuClock - wipeLastTick); }
        wipeLastTick = menuClock;
        wipe.Update(wipeDelta);
        music.Update();
        bool activate = false;
        const bool wasPostGameMusic = state.postGame.postGameMusic;
        const unsigned previousPage = state.page;
        const unsigned previousCategory = state.store.shopCategory;
        
#if GB_ENABLE_CHEATS
for (std::string cheat = view.window.TakeCheatCode(); !cheat.empty(); cheat = view.window.TakeCheatCode()) {
            if (cheat == GameCheats::Money) { profile.coins += 5000; profile.warbucks += 500; state.message = "COINS +5000 / WARBUCKS +500"; }
            if (cheat == GameCheats::NextDay) {
                AdvanceDailyDebugDay(profile, daily, static_cast<std::uint32_t>(CurrentSeconds()));
                state.Navigate(24);
            }
            if (cheat == GameCheats::ToggleDebug) { GameHostSettings().debugMode = !GameHostSettings().debugMode; }
            if (cheat == GameCheats::ToggleConnection) { GameHostSettings().isConnected = !GameHostSettings().isConnected; }
            if (cheat == GameCheats::HealthOrGreeting) { state.Navigate(24); }
            if (cheat == GameCheats::UnlockWaves) { profile.clearedWaves.fill(500); state.message = "ALL WAVES UNLOCKED"; }
            if (!profile.SaveToDisk(savePath)) { return -3; }
            std::printf("[cheat] %s\n", cheat.c_str());
        }
#endif

        for (KeyCode key = view.window.TakeKeyPress(); key != KeyCode::None; key = view.window.TakeKeyPress()) {
            if (wipe.IsActive()) { continue; }
            if (state.page == 14) {
                if (key == KeyCode::Space || key == KeyCode::Enter) { activate = true; }
                continue;
            }
            if (state.currencyPending || state.refinery.refineryTransfer >= 0) { continue; }
            if (state.promotion.IsActive()) {
                if (key == KeyCode::Escape) { state.promotion.Dismiss(); }
                continue;
            }
            if (profile.nativeArchive && (state.page == 25 || state.page == 29)) { continue; }
            if (state.storePromptRequested || state.storePopup.IsActive()) {
                // Desktop Escape follows the modal's authored dismissal mode.
                if (key == KeyCode::Escape && (state.storePromptDismiss || state.storePromptButtons != nullptr) && state.storePopup.IsReady()) {
                    state.storePopup.Hide();
                }
                continue;
            }
            if (state.page >= 26 || state.refinementRequired) {
                if (key == KeyCode::Escape) {
                    if (state.page == 26) { state.masteryPopup.Hide(); }
                    else if (state.page == 27 || state.page == 28) {
                        if (profile.nativeArchive) { state.postGame.postGameClosing = true; state.postGame.postGameCloseTime = 0; }
                        else { state.page = 3; }
                    }
                }
                continue;
            }
            if (key == KeyCode::Space || key == KeyCode::Enter) { activate = true; }
            if (key == KeyCode::Escape) {
                if (profile.nativeArchive && state.page == 21 && state.missions.missionFocused >= 0) {
                    state.missions.missionClosing = true;
                    state.missions.missionFocusTime = 0;
                }
                else if (state.page != 0) { state.Back(); }
                else { return -1; } // Windows Escape closes through the normal save path.
            }
            if (state.page == 0 && key == KeyCode::Down) { state.planet = (state.planet + 1) % 4; }
            if (state.page == 0 && key == KeyCode::Up) { state.planet = (state.planet + 3) % 4; }
            if (key == KeyCode::P) { state.Navigate(0); }
            if (key == KeyCode::E) { state.Navigate(2); }
            if (key == KeyCode::B) { state.Navigate(2); }
            if (key == KeyCode::F) { state.Navigate(3); }
            if (key == KeyCode::Q && state.page == 2) { state.store.shopSwapKeyRequested = true; }
        }
        if (previousPage != state.page) { state.itemPage = 0; state.selectedItem = -1; state.message.clear(); }
        const std::int64_t now = CurrentSeconds();
        profile.refinery.UpdateRefinement(now);
        view.Begin(state.page);
        view.inputEnabled = !state.currencyPending && !state.storePromptRequested && !state.storePopup.IsActive() && !state.promotion.IsActive() && !wipe.IsActive();
#if GB_ENABLE_TESTS
        if (testClicks != nullptr && testFrame < testClicks->size()) { view.SetTestClick((*testClicks)[testFrame]); }
#endif
        if (wipe.IsActive()) {
            view.ExchangeClick(false);
            view.dragX = view.dragY = 0;
            view.window.TakeWheelDelta();
        }
        const bool masteryFrame = state.page == 26;
        bool masteryClick = false;
        if (masteryFrame) {
            // Preserve the invoking menu under the modal. Its input is consumed
            // by CMenuSystem::Update :96904; no purchase/navigation may leak through.
            state.page = 27;
            if (!state.history.empty()) { state.page = state.history.back(); }
            view.inputEnabled = false;
            masteryClick = view.ExchangeClick(false);
        }
        if (state.page == 27 || state.page == 28) {
            if (profile.nativeArchive) {
                if (!DrawOriginalPostGame(view, state, toc, tables, profile)) { return -3; }
            } else { return -3; }
        }
        if (state.page == 24 && profile.nativeArchive) {
            if (!DrawOriginalGreeting(view, state, toc, tables, profile, daily, store, progress, savePath, now)) { return -3; }
        }
        if (state.page == 0 || state.page == 22) {
            if (!profile.nativeArchive) {
                std::printf("[menu] star map requires native progression records\n");
                return -3;
            }
            if (!DrawOriginalStarMap(view, state, profile) || !DrawOriginalModeOverlay(view, state)) { return -3; }
        }
        if (state.page == 21) {
            if (!profile.nativeArchive) { return -3; }
            bool launch = false;
            if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return -3; }
            if (launch) { return static_cast<int>(state.planet); }
        }

        if (!CompleteOfflineIAP(menuClock, state, profile, store, savePath)) { return -3; }
        if (state.page == 2 || state.page == 17 || state.page == 18) {
            if (state.page != 2) { state.store.shopCategory = 3; }
            if (!DrawStore(view, toc, tables, profile, progress.GetLevel(), store, weapons, armors, state, savePath)) { return -3; }
        }
        if (state.page == 3) {
            if (!DrawRefinery(view, state, profile, refinement, savePath, now)) { return -3; }
        }
        bool saveChanged = false;
        if (state.page == 25 || state.page == 29) {
            bool launchTutorial = false;
            if (!DrawOriginalPlayerSelect(view, state, profile, savePath, launchTutorial)) { return -3; }
            if (launchTutorial) { return 5; }
        } else if (state.page == 4 || state.page == 5 || state.page == 11 || state.page == 13) {
            const bool credentials = std::filesystem::exists(savePath / "Credentials.dat");
            if (!DrawOriginalSocialOffline(view, state, credentials)) { return -3; }
        } else if (state.page == 6 || state.page == 8) {
            if (!DrawOptions(view, state, profile, saveChanged)) { return -3; }
        } else if (state.page == 14) {
            if (!view.TitleImage()) { return -3; }
            // User-requested desktop prompt, not an authored Movie/text.
            // The glyphs themselves come from the original BIG font11.
            if ((view.clock / 600) % 2 == 0) {
                view.CenterText("TAP TO CONTINUE", kMenuWidth * 0.5f, kMenuHeight * 0.88f, 11, 1.0f);
            }
            if (view.Hit(0, 0, kMenuWidth, kMenuHeight) || activate) {
                unsigned nextPage = 25;
                if (!profile.firstLaunch) { nextPage = 24; }
                state.Navigate(nextPage);
            }
        }
        if (saveChanged) {
            music.SetEnabled(profile.musicEnabled);
            CAudioPlayer::SetEffectsEnabled(profile.soundEnabled);
            if (!profile.SaveToDisk(savePath)) { return -3; }
        }
        if (masteryFrame) {
            state.page = 26;
            view.ExchangeClick(masteryClick);
            view.inputEnabled = !state.storePromptRequested && !state.storePopup.IsActive();
        }
        int navigation = -1;
        if (state.currencyPending || state.refinery.refineryTransfer >= 0) { view.Hit(0, 0, 1024, 768); }
        if (state.page != 14 && state.page != 26) {
            unsigned headerPage = state.page;
            if (state.page == 29) { headerPage = 4; }
            if (state.page == 17 || state.page == 18) { headerPage = 2; }
            if (state.refinementRequired) { headerPage = 25; }
            navigation = view.Header(profile, progress, headerPage);
            if (navigation == -3) { return -3; }
        }
        constexpr unsigned navigationPages[] = {0, 4, 5, 2, 3, 6, 7};
        if (navigation >= 7) {
            state.currencyTab = static_cast<unsigned>(navigation - 7);
            state.currencyPage = 0;
            state.Navigate(17);
            state.store.shopFilter = 1u << (state.currencyTab + 14);
        } else if (navigation >= 0) {
            if (navigation == 6) {
                // CMenuAction21 :93929 requests PlayHaven "more_games" only.
                // Without publisher offers it leaves the current menu intact.
                std::printf("[navigation] more_games unavailable: no publisher service\n");
            } else { state.Navigate(navigationPages[navigation], true); }
            state.itemPage = 0;
            state.selectedItem = -1;
            state.message.clear();
        }
        if (!state.message.empty()) { view.Text(450, 738, state.message, 1.45f, 0.93f, 0.74f, 0.33f); }
        if (state.page == 26 && !DrawMastery(view, state, profile, toc, tables, store, weapons, savePath, &progress)) { return -3; }
        if (!DrawStorePrompt(view, state)) { return -3; }
        if (state.promotion.IsActive()) {
            state.promotion.Update(static_cast<unsigned>(view.clock - state.promotionTick));
            state.promotionTick = view.clock;
            if (!state.promotion.Draw(view.movies)) { return -3; }
            // Consume every release inside the modal, including outside its buttons.
            const auto cursor = view.Cursor();
            if (view.ExchangeClick(false)) {
                const unsigned action = state.promotion.Click(cursor.first, cursor.second);
                if (action != 0 && action != 45) {
                    std::printf("[promotion] original action=%u unavailable on host; no account changes\n", action);
                }
            }
        }
#if GB_ENABLE_TESTS
        if (GameHostSettings().debugMode) {
            char debug[160];
            std::snprintf(debug, sizeof(debug), "FPS %.1f / %.1f MS / PAGE %u / NET %u", 1000.0f / std::max(0.1f, smoothFrameMs),
                smoothFrameMs, state.page, GameHostSettings().isConnected);
            view.movies.Rectangle(2, 135, 620, 22, 0, 0, 0, 0.8f);
            view.movies.Text(debug, 7, 138, 0, 0.65f);
        }
#endif

        // The pressed plate's burst plays above whatever the click opened.
        if (wasPostGameMusic && !state.postGame.postGameMusic && !music.Play(0)) { return -3; }
        view.DrawPress();
        if (view.animateNavigation) {
            // Only navigation between branches sweeps; see MenuBranchPage.
            const bool changed = MenuBranchPage(state.page) != MenuBranchPage(previousPage);
            const bool popup = state.page == 26 || previousPage == 26;
            // Deterministic reproduction of first-use resource work during a frame.
#if GB_ENABLE_TESTS
            if (testClicks && testFrame < testClicks->size()) { testClock += (*testClicks)[testFrame].renderDelayMs; }
#endif
            if (changed && !popup && !wipe.IsActive()) {
                if (!wipe.Begin(view.movies)) { return -3; }
#if GB_ENABLE_TESTS
                if (transitionTrace) { ++transitionTrace->starts; }
#endif
                // Start at the first presented wipe frame, after cold resources
                // for the destination have loaded. Frame-start time is stale here.
                wipeLastTick = view.window.GetTicksMs();
#if GB_ENABLE_TESTS
                if (testClicks) { wipeLastTick = testClock; }
#endif
                std::printf("[menu-wipe] from=%u/%u to=%u/%u duration=%u first-frame=0\n",
                    previousPage, previousCategory, state.page, state.store.shopCategory, wipe.Duration());
            }
            if (!wipe.Draw()) { return -3; }
            if (!wipe.IsActive() && !wipe.Remember()) { return -3; }
        }
#if GB_ENABLE_TESTS
        if (transitionTrace) { transitionTrace->active = wipe.IsActive(); transitionTrace->time = wipe.Time(); }
#endif
#if GB_ENABLE_TESTS
        ++testFrame;
#endif
#if GB_ENABLE_TESTS
        if (!capturePath.empty() && (testClicks == nullptr || testFrame > testClicks->size())) {
            if (glGetError() != 0 || !GB_SAVE_FRAME(view.window, capturePath)) { return -3; }
            view.window.Present();
            return -2;
        }
#endif

        view.window.Present();
    }
    return -1;
}
} // namespace MenuDetail
