#include "gun_bros_re/debug/DebugKeys.h"
#include "gun_bros_re/cheats/CheatActions.h"
#include "gun_bros_re/debug/DebugTutorial.h"
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
    // CMenuMeshPlayer::BindPlayer copies the current equipment when binding a new menu.
    // Combat may have changed the active gun while the previous menu state survived.
    state.store.shopGunSlot = profile.activeWeaponSlot;
    // A new view has a new clock (including deterministic capture sessions).
    state.store.shopFilterBound = false;
    state.settings.optionsBound = false;
    state.social.socialBound = false;
    state.social.contentBound = false;
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
    CDailyBonusTracking daily;
    if (!daily.Load(toc, tables)) { return -3; }
    if (profile.nativeArchive && !state.resumeAfterDebugTutorial) {
        daily.RefreshUsageData(profile, static_cast<std::uint32_t>(CurrentSeconds()));
        if (!profile.SaveToDisk(savePath)) { return -3; }
    }
    state.resumeAfterDebugTutorial = false;
    std::printf("[menu] ready page=%u\n", state.page);
    unsigned challengeSecond = UINT32_MAX;
    bool challengeConnected = false;
    while (view.window.PumpEvents()) {
        const auto ticks = view.window.GetTicksMs();
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
        // CMenuGameResources::Update :173392 resumes only after the header's
        // authored entrance. Switch before drawing, so WIPE captures the store
        // on its first frame instead of capturing the refinery a second time.
        if (state.page == 3 && state.refinery.refineryExitPending && view.IsNavigationReady() && !wipe.IsActive()) {
            state.refinery.refineryExitPending = false;
            // CMenuStore::OnShow reinitializes category content. Postgame returns to GUNS.
            state.store = StoreMenuState{};
            state.store.shopGunSlot = profile.activeWeaponSlot;
            state.selectedItem = -1;
            state.Navigate(2, true);
        }
        
        if (!ProcessMenuCheats(view.window, profile, state, daily, savePath, progressData, progress)) { return -3; }
        UpdateLocalConnection(state);
        const unsigned challengeNow = static_cast<unsigned>(CurrentSeconds());
        if (state.online.IsConnected() && profile.nativeArchive &&
            (challengeNow != challengeSecond || !challengeConnected) && !state.storePromptRequested && !state.storePopup.IsActive()) {
            challengeSecond = challengeNow;
            auto &challenges = state.social.challenges;
            const auto previousRecord = profile.nativeArchive->records[17].payload;
            const unsigned day = static_cast<unsigned>((std::uint64_t(challengeNow) + 36000) / 86400);
            if (challenges.current.empty() || day > challenges.cycleDay || !challengeConnected) {
                if (!challenges.InitProgressData(toc, tables, profile, challengeNow)) { return -3; }
            }
            challenges.UpdateChallengeStatusData(profile, false);
            unsigned selected = 0;
            for (; selected < challenges.current.size(); ++selected) {
                const auto &challenge = challenges.current[selected];
                const auto &definition = challenges.templates[challenge.templateIndex];
                if (challenge.progress == 100 && challenge.rewardStatus < 3 &&
                    challenge.completedFriends >= definition.participationRequired[challenge.rewardStatus]) { break; }
            }
            unsigned awarded = 0;
            if (!challenges.AwardAvailableRewards(profile, store, awarded)) { return -3; }
            if (previousRecord != profile.nativeArchive->records[17].payload || awarded) {
                if (!profile.SaveToDisk(savePath)) { return -3; }
            }
            if (awarded) {
                // CMenuAction 110 and CreateRewardDescString :240332.
                const auto &challenge = challenges.current[selected];
                state.ShowStorePrompt("MDS_CHALLENGE_MENU", false, true);
                state.challengeRewardTitle = view.movies.NamedString("IDS_CHALLENGES_PROMPT_COMPLETE_TITLE");
                const char *description = "IDS_CHALLENGES_PROMPT_COMPLETE_DESC1";
                if (challenge.rewardStatus > 1) { description = "IDS_CHALLENGES_PROMPT_COMPLETE_DESC2"; }
                state.challengeRewardBody = view.movies.NamedString(description);
                const auto marker = state.challengeRewardBody.find("%s");
                if (marker != std::string::npos) { state.challengeRewardBody.replace(marker, 2, challenge.name); }
                if (challenge.rewardStatus < 3) { state.challengeRewardBody += view.movies.NamedString("IDS_CHALLENGES_PROMPT_COMPLETE_DESC3"); }
                progress.SetExperience(profile.experience);
            }
        }
        challengeConnected = state.online.IsConnected();

        for (KeyCode key = view.window.TakeKeyPress(); key != KeyCode::None; key = view.window.TakeKeyPress()) {
#if GB_ENABLE_CHEATS
            if (GameDebugKeys::StartsTutorial(key, view.window)) { return kDebugTutorialMenuChoice; }
#endif
            if (GameDebugKeys::OpensMapBrowser(key, view.window)) {
                music.SetPaused(true);
                const bool selected = ShowDebugMapPicker(toc, tables, view.window, state.debugMap);
                music.SetPaused(false);
                if (selected) { return kDebugMapMenuChoice; }
                frameTicks = view.window.GetTicksMs();
                wipeLastTick = frameTicks;
                continue;
            }
            if (wipe.IsActive()) { continue; }
            if (state.page == 14) {
                if (key == KeyCode::Space || key == KeyCode::Enter) { activate = true; }
                continue;
            }
            if (state.currencyPending || state.refinery.refineryTransfer >= 0 || state.refinery.refineryExitPending) { continue; }
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
        view.inputEnabled = !state.currencyPending && !state.storePromptRequested && !state.storePopup.IsActive() &&
            !state.promotion.IsActive() && !wipe.IsActive() && !state.refinery.refineryExitPending;
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
            if (launch) {
                if (state.gameMode == 0) { return static_cast<int>(state.planet); }
                if (!BeginLocalMatch(state)) { state.ShowStorePrompt("MDS_PROMPT_MP_UNAVAILABLE", false, true, 0); }
            }
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
            if (!DrawOriginalSocialMenu(view, state, profile, credentials)) { return -3; }
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
        if (state.page == 3 && !DrawRefineryOverlay(view, state)) { return -3; }
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
        if (transitionTrace) {
            transitionTrace->active = wipe.IsActive();
            transitionTrace->time = wipe.Time();
            transitionTrace->frames.push_back({state.page, view.HeaderTime(), wipe.Time(),
                view.IsNavigationReady(), state.refinery.refineryExitPending, wipe.IsActive()});
        }
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
