#include "gun_bros_re/ui/PostGameCardCallbacks.h"
#include "gameplay/SurvivalStudy.h"
#include "gun_bros_re/ui/MenuInternal.h"
#include "TestOutput.h"
#include "ui/MenuChecks.h"
using namespace MenuDetail;

/** Compare the real button callback with the native unselected artwork. */
static bool CheckUnselectedTabArtwork(GameMenu &view, const char *table) {
    for (unsigned index = 0; index < 2; ++index) {
        const auto *entry = OriginalMenuData(table, index);
        if (entry == nullptr) { return false; }
        MovieRegion origin;
        origin.x = 320;
        origin.y = 300;
        origin.alpha = 1;
        bool pressed = false;
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        std::vector<std::uint8_t> actual(viewport[2] * viewport[3] * 4);
        std::vector<std::uint8_t> expected(actual.size());
        view.Begin(3);
        if (!DrawOriginalMovieButton(view, *entry, origin, {}, 5, false, pressed, 2, 1000, UINT32_MAX, true)) { return false; }
        glReadPixels(0, 0, viewport[2], viewport[3], GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
        OriginalMenuEntry reference = *entry;
        reference.sprites[0] = entry->sprites[1];
        view.Begin(3);
        if (!DrawOriginalMovieButton(view, reference, origin, {}, 5, false, pressed, 2, 1000)) { return false; }
        glReadPixels(0, 0, viewport[2], viewport[3], GL_RGBA, GL_UNSIGNED_BYTE, expected.data());
        if (actual != expected) {
            std::printf("[tab-artwork-check] table=%s index=%u expected unselected sprite=%u failures=1\n", table, index, entry->sprites[1]);
            return false;
        }
        view.Begin(3);
        if (!DrawOriginalMovieButton(view, *entry, origin, {}, 5, false, pressed, 3, 1000, UINT32_MAX, true)) { return false; }
        glReadPixels(0, 0, viewport[2], viewport[3], GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
        if (actual == expected) {
            std::printf("[tab-artwork-check] table=%s index=%u missing selected color/glow\n", table, index);
            return false;
        }
        view.Begin(3);
        if (!DrawOriginalMovieButton(view, *entry, origin, {}, 5, false, pressed, 3, 1000)) { return false; }
        glReadPixels(0, 0, viewport[2], viewport[3], GL_RGBA, GL_UNSIGNED_BYTE, expected.data());
        if (actual != expected) { return false; }
    }
    return true;
}

/** Native save fixtures and real greeting callbacks; no original saves change. */
int RunPostGameMenuCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    std::vector<PlanetEntry> planets;
    std::vector<WeaponEntry> weapons;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadPlanetCatalog(toc, tables, planets) ||
        !LoadWeaponCatalog(toc, tables, weapons)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) / ("postgame-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
    unsigned tested = 0;
    for (unsigned type : {1u, 2u}) {
        for (unsigned index = 0; index < planets.size(); ++index) {
            const auto &planet = planets[index];
            if (planet.missions.empty() || planet.missions[0].type != type) { continue; }
            MissionEntry mission;
            mission.resource = planet.data.missions[0];
            mission.data = planet.missions[0];
            mission.title = planet.missionInfo[0].title;
            SurvivalGameContext context{profile, path, index};
            context.mission = mission.resource;
            context.missionLevel = mission.data.level;
            if (type == 2) { context.hordeStart = 0; }
            const MissionEntry *archiveMission = nullptr;
            if (type == 2) { archiveMission = &mission; }
            const auto &map = planet.missionInfo[0].map;
            if (RunSurvivalStudy(bigDirectory, tables.GetPackName(map.packHash), map.localIndex, 0, -1, {}, 0,
                false, false, true, 2, 0, &context, true, false, archiveMission) != 0) { return 1; }
            if (context.result.kills == 0 || context.result.casualties.empty() || context.result.waves != 2) { return 1; }
            const auto coins = profile.coins, warbucks = profile.warbucks, ore = profile.xplodium;
            MenuState state;
            BeginPostGame(state, context, weapons);
            const bool popupExpected = state.postGame.postGameUpgradePending;
            if (state.page != 27) { return 1; }
            GameMenu view;
            if (!view.Open(toc, tables)) { return 1; }
            view.scripted = true;
            if (!CheckUnselectedTabArtwork(view, "MDS_BUTTON_POSTGAME_INFO")) { return 1; }
            const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_WRAPUP_SCREEN");
            const auto *movie = view.movies.GetMovie(ordinal);
            unsigned start = 0, end = 0;
            if (movie == nullptr || !movie->GetChapterRange(1, start, end)) { return 1; }
            MovieRegion tabs;
            if (!view.movies.Region(ordinal, 1, start, tabs)) { return 1; }
            const auto *tab = OriginalMenuData("MDS_BUTTON_POSTGAME_INFO", 1);
            MovieRegion tabBounds;
            if (tab == nullptr || !view.movies.Region(view.movies.Ordinal(tab->movies[0]), 0, 0, tabBounds)) { return 1; }
            const float tabX = tabs.x + static_cast<int>(tabs.width) / 2;
            MenuTestClick casualtyClick{};
            for (const auto &area : view.movies.Regions(view.movies.Ordinal(tab->movies[0]), 0, tabX, tabs.y)) {
                if (area.index == 0) { casualtyClick = {area.x + area.width / 2, area.y + area.height / 2}; }
            }
            view.Begin(27);
            view.SetTestClick(casualtyClick);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile) || state.page != 27) { return 1; }
            view.clock += start;
            view.Begin(27);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile)) { return 1; }
            if (popupExpected) {
                if (state.page != 26) { return 1; }
                CloseMastery(state);
                if (state.page != 27) { return 1; }
            }
            view.clock += 1000;
            view.Begin(27);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile)) { return 1; }
            const std::string prefix = TestOutput::Path("ui-original-2026-09-09/postgame-original-") + std::to_string(type);
            if (!GB_SAVE_FRAME(view.window, prefix + "-overview.png")) { return 1; }
            view.Begin(27);
            view.SetTestClick(casualtyClick);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile) || state.page != 28) { return 1; }
            view.Begin(28);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile)) { return 1; }
            if (!GB_SAVE_FRAME(view.window, prefix + "-casualties.png")) { return 1; }
            // Every actual killed enemy must resolve its own original menu model.
            for (unsigned enemy = 0; enemy < state.result.casualties.size(); ++enemy) {
                state.postGame.postGameGalleryPosition = static_cast<float>(enemy) - 1;
                view.clock += 100;
                view.Begin(28);
                if (!DrawOriginalPostGame(view, state, toc, tables, profile)) { return 1; }
            }
            MovieRegion backArea;
            const auto *back = OriginalMenuData("MDS_BUTTON_POSTGAME_BACK", 0);
            if (back == nullptr || !view.movies.Region(ordinal, 0, start, backArea)) { return 1; }
            MenuTestClick backClick{};
            for (const auto &area : view.movies.Regions(view.movies.Ordinal(back->movies[0]), 0,
                backArea.x + backArea.width / 2, backArea.y + backArea.height / 2, true)) {
                if (area.index == 0) { backClick = {area.x + area.width / 2, area.y + area.height / 2}; }
            }
            view.Begin(28);
            view.SetTestClick(backClick);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile) || !state.postGame.postGameClosing || state.page != 28) { return 1; }
            const auto *backMovie = view.movies.GetMovie(view.movies.Ordinal(back->movies[0]));
            unsigned exitStart = 0, exitEnd = 0;
            if (backMovie == nullptr || !backMovie->GetChapterRange(1, exitStart, exitEnd)) { return 1; }
            unsigned hideStart = 0, hideEnd = 0;
            if (!backMovie->GetChapterRange(0, hideStart, hideEnd)) { return 1; }
            view.clock += exitEnd - exitStart + 1;
            view.Begin(28);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile) || state.page != 28) { return 1; }
            view.clock += hideEnd - hideStart + 1;
            view.Begin(28);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile) || state.page != 3) { return 1; }
            if (profile.coins != coins || profile.warbucks != warbucks || profile.xplodium != ore ||
                !profile.LoadFromDisk(path) || profile.xplodium != ore) { return 1; }
            // Zero-ore routing is isolated from the actual persisted reward.
            profile.xplodium = 0;
            state.page = 28;
            state.postGame.postGameClosing = false;
            view.Begin(28);
            view.SetTestClick(backClick);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile) || !state.postGame.postGameClosing) { return 1; }
            view.clock += exitEnd - exitStart + hideEnd - hideStart + 2;
            view.Begin(28);
            if (!DrawOriginalPostGame(view, state, toc, tables, profile) || state.page != 0 ||
                !profile.LoadFromDisk(path) || profile.xplodium != ore) { return 1; }
            std::printf("[postgame-original-check] type=%u waves=%u kills=%u casualties=%zu score=%u best=%u time=%u opening=1 tabs=1 exit=1 no-duplicate-reward=1 failures=0\n",
                type, context.result.waves, context.result.kills, context.result.casualties.size(), context.result.score,
                context.result.bestKillStreak, context.result.stopwatchMs);
            ++tested;
            break;
        }
    }
    return tested == 2 ? 0 : 1;
}

int RunGreetingCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    CDailyBonusTracking daily;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadStoreCatalog(toc, tables, store) ||
        !daily.Load(toc, tables)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) / ("greeting-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
    const auto first = static_cast<std::uint32_t>(CurrentSeconds());
    // Explicit fixture: a new daily cycle, preserving the real inventory/wallet.
    profile.dailyLastLaunchSeconds = first;
    profile.dailyConsecutiveSeconds = 0;
    profile.dailyConsecutiveDays = 1;
    profile.dailyLastCommit = 0;
    CPlayerProgress progress;
    progress.Bind(profile.nativeArchive->progression);
    progress.SetExperience(profile.experience);
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = true;
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_WELCOME_NEW");
    auto *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, start, end)) { return 1; }
    for (unsigned day = 0; day < 7; ++day) {
        const auto seconds = first + day * 86400;
        MenuState state;
        state.page = 24;
        const auto coins = profile.coins;
        const auto warbucks = profile.warbucks;
        const auto xp = profile.experience;
        const auto awarded = profile.statistics[32];
        const auto &prize = daily.prizes[day % daily.prizes.size()];
        view.Begin(24);
        if (!DrawOriginalGreeting(view, state, toc, tables, profile, daily, store, progress, path, seconds) ||
            profile.coins != coins || profile.warbucks != warbucks || profile.statistics[32] != awarded) { return 1; }
        view.clock += start;
        view.Begin(24);
        view.inputEnabled = true; // ShowGameMenu resets the frame input gate.
        if (!DrawOriginalGreeting(view, state, toc, tables, profile, daily, store, progress, path, seconds) ||
            profile.coins != coins || state.greeting.greetingClosing) { return 1; }
        if (day == 0 && !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/greeting-original-open.png"))) { return 1; }
        MovieRegion button;
        if (!view.movies.Region(ordinal, 3 + day % 2, state.greeting.greetingTime, button)) { return 1; }
        view.SetTestClick({button.x + button.width / 2, button.y + button.height / 2});
        if (!DrawOriginalGreeting(view, state, toc, tables, profile, daily, store, progress, path, seconds) ||
            !state.greeting.greetingExitRequested || profile.coins != coins) { return 1; }
        view.Begin(24);
        if (!DrawOriginalGreeting(view, state, toc, tables, profile, daily, store, progress, path, seconds) ||
            !state.greeting.greetingClosing || state.greeting.greetingTime != start || profile.coins != coins + prize.coins ||
            profile.warbucks != warbucks + prize.warbucks || profile.experience != xp + prize.experience ||
            profile.statistics[32] != awarded + 1) { return 1; }
        view.clock += start / 2;
        view.Begin(24);
        if (!DrawOriginalGreeting(view, state, toc, tables, profile, daily, store, progress, path, seconds) ||
            state.greeting.greetingTime != start - start / 2 || profile.statistics[32] != awarded + 1) { return 1; }
        if (day == 0 && !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/greeting-original-exit.png"))) { return 1; }
        view.clock += start;
        view.Begin(24);
        unsigned target = 5;
        if (day % 2 != 0) { target = 4; }
        if (!DrawOriginalGreeting(view, state, toc, tables, profile, daily, store, progress, path, seconds) ||
            state.page != target || daily.CommitBonus(profile, seconds, store)) { return 1; }
    }
    CProfileManager reloaded;
    reloaded.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!LoadNativeProfile(toc, tables, reloaded, path, {}) || reloaded.dailyLastCommit != 7 ||
        reloaded.coins != profile.coins || reloaded.warbucks != profile.warbucks) { return 1; }
    const auto seconds = first + 9 * 86400;
    daily.RefreshUsageData(reloaded, seconds);
    if (reloaded.dailyConsecutiveDays != 1 || !daily.CommitBonus(reloaded, seconds, store)) { return 1; }
    AdvanceDailyDebugDay(reloaded, daily, seconds);
    if (reloaded.dailyLastLaunchSeconds != seconds || reloaded.dailyConsecutiveDays != 2 ||
        !daily.CommitBonus(reloaded, seconds, store) || !reloaded.SaveToDisk(path)) { return 1; }
    CProfileManager finalProfile;
    finalProfile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!LoadNativeProfile(toc, tables, finalProfile, path, {}) || finalProfile.dailyLastCommit != 2 ||
        daily.IsBonusAvailable(finalProfile, seconds) || view.movies.Failures() != 0 || glGetError() != 0) { return 1; }
    std::printf("[greeting-check] original-buttons=7 no-award-on-show=7 exit-only=7 reverse=7 duplicate=7 cycle=7 gap=1 cht=1 native-reload=2 failures=0\n");
    return 0;
}

/** Run the actual shell through collection, header entrance and one cold store wipe. */
int CheckRefineryStoreTransition(CResTOCManager &toc, PackTables &tables, GameMenu &probe,
    const CRefinementManager::Template &refinement, unsigned previousCategory) {
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path(TestOutput::Path("refinery-store-transition/profile-") + std::to_string(previousCategory));
    if (!LoadNativeProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    if (!LoadStoreCatalog(toc, tables, store) || !LoadWeaponCatalog(toc, tables, weapons) ||
        !LoadArmorCatalog(toc, tables, armor)) { return 1; }
    // Isolated ready-to-collect fixture; the shell still owns the click and transfer.
    profile.xplodium = 250;
    if (!profile.refinery.BeginRefinement(6, 6, profile.xplodium, profile.xplodium, CurrentSeconds())) { return 1; }
    const auto coins = profile.coins;
    const auto yield = profile.refinery.GetRefinementSlotYield(6);
    MenuState state;
    state.page = 3;
    state.store.shopCategory = previousCategory;
    // Leave category-specific browsing state behind, as a previous store visit would.
    state.store.shopFilter = 1;
    state.store.shopExclusionFilter = 1;
    state.store.shopScroll = 500;
    state.store.shopDetailOpen = true;
    state.selectedItem = 0;
    state.refinementRequired = true;
    MovieRegion meter;
    unsigned idle = 0, end = 0, showStart = 0, showEnd = 0, headerIdle = 0;
    const auto *main = probe.movies.GetMovie(probe.movies.Ordinal("GLU_MOVIE_EXPLODIUM"));
    const auto *header = probe.movies.GetMovie(probe.movies.Ordinal("GLU_MOVIE_HEADER"));
    const auto *fill = probe.movies.GetMovie(probe.movies.Ordinal("GLU_MOVIE_BUCKET_FILL"));
    const auto *wipe = probe.movies.GetMovie(probe.movies.Ordinal("GLU_MOVIE_WIPE"));
    if (!main || !header || !fill || !wipe || !main->GetChapterRange(1, idle, end) ||
        !header->GetChapterRange(1, showStart, showEnd) || !header->GetChapterRange(2, headerIdle, end) ||
        !probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_EXPLODIUM"), 0, idle, meter)) { return 1; }
    const unsigned entrance = headerIdle - showStart;
    const auto *meterButton = probe.movies.GetMovie(probe.movies.Ordinal("GLU_MOVIE_BUCKET_BUTTON"));
    unsigned clickStart = 0, clickEnd = 0;
    if (!meterButton || !meterButton->GetChapterRange(1, clickStart, clickEnd)) { return 1; }
    const unsigned clickDuration = (clickEnd - clickStart + 1) / 2;
    const std::vector<MenuTestClick> clicks{
        {-100, -100, 1}, {-100, -100, idle + fill->duration + 1},
        {meter.x + meter.width / 2, meter.y + meter.height / 2, 1},
        {-100, -100, clickDuration},
        {-100, -100, 374}, {-100, -100, 1},
        {-100, -100, entrance / 2}, {-100, -100, entrance - entrance / 2},
        {-100, -100, 1, wipe->duration * 2},
        {-100, -100, wipe->duration / 2}, {-100, -100, wipe->duration - wipe->duration / 2}};
    MenuTransitionTrace trace;
    const CPlayerProgress::Template progress = profile.nativeArchive->progression;
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        state, path, TestOutput::Path("refinery-store-transition/store.png"), &clicks, true, &probe.window, true, &trace) != -2 ||
        trace.frames.size() < clicks.size()) { return 1; }
    for (unsigned index = 0; index < clicks.size(); ++index) {
        const auto &frame = trace.frames[index];
        std::printf("[refinery-transition-check] frame=%u page=%u header=%u ready=%d pending=%d wipe=%u active=%d\n",
            index, frame.page, frame.headerTime, frame.navigationReady, frame.refineryExitPending, frame.wipeTime, frame.wipeActive);
        if (index <= 7 && (frame.page != 3 || frame.wipeActive)) { return 1; }
        if ((index == 5 || index == 6) && (!frame.refineryExitPending || frame.navigationReady)) { return 1; }
        if (index == 5 && frame.headerTime != showStart) { return 1; }
        if (index == 7 && !frame.navigationReady) { return 1; }
        if (index == 8 && (frame.page != 2 || !frame.wipeActive || frame.wipeTime != 0)) { return 1; }
        if (index == 9 && (!frame.wipeActive || frame.wipeTime != wipe->duration / 2)) { return 1; }
    }
    if (trace.starts != 1 || trace.active || state.page != 2 || !state.history.empty() ||
        state.refinery.refineryExitPending || profile.coins != coins + yield || profile.xplodium != 0) { return 1; }
    if (state.store.shopCategory != 0) {
        std::printf("[refinery-store-category-check] previous=%u actual=%u expected=0\n",
            previousCategory, state.store.shopCategory);
        return 1;
    }
    std::printf("[refinery-store-category-check] previous=%u guns=1 failures=0\n", previousCategory);
    if (state.store.shopFilter != 0 || state.store.shopExclusionFilter != 0 || state.store.shopScroll != 0 ||
        state.store.shopDetailOpen || state.selectedItem != -1) { return 1; }
    if (state.store.shopGunSlot != profile.activeWeaponSlot) {
        std::printf("[store-return-slot-check] active=%u store=%u\n", profile.activeWeaponSlot, state.store.shopGunSlot);
        return 1;
    }
    std::printf("[refinery-transition-check] header-before-store cold-first-frame one-wipe collect-once failures=0\n");
    if (previousCategory != 3) { return 0; }
    // Simulate each completed combat swap while the old menu state survives.
    for (unsigned slot : {0u, 1u}) {
        profile.activeWeaponSlot = slot;
        state.store.shopGunSlot = 1 - slot;
        if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state, path,
            TestOutput::Path("refinery-store-transition/active-gun-") + std::to_string(slot + 1) + ".png",
            nullptr, true, &probe.window) != -2 || state.store.shopGunSlot != slot || profile.activeWeaponSlot != slot) { return 1; }
        CProfileManager restored;
        if (!LoadNativeProfile(toc, tables, restored, path) || restored.activeWeaponSlot != slot) { return 1; }
        std::printf("[store-return-slot-check] active=%u store=%u saved=%u failures=0\n",
            slot, state.store.shopGunSlot, restored.activeWeaponSlot);
    }
    // A fresh binding must still allow the authored button and PLAYER Flow to swap guns.
    const auto *swapEntry = OriginalMenuData("MDS_BUTTON_STORE_GUN_SWAP", 0);
    MovieRegion swapParent, swapOrigin;
    if (swapEntry == nullptr || !probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_MENU"),
        kStoreGunSwapRegion, 0, swapParent) || !StoreGunSwapOrigin(probe, swapParent, swapOrigin)) { return 1; }
    const unsigned swapId = probe.movies.Ordinal(swapEntry->movies[0]);
    const CMovie *swapMovie = probe.movies.GetMovie(swapId);
    unsigned swapStart = 0, swapEnd = 0, pressStart = 0, pressEnd = 0;
    if (swapMovie == nullptr || !swapMovie->GetChapterRange(0, swapStart, swapEnd) ||
        !swapMovie->GetChapterRange(1, pressStart, pressEnd)) { return 1; }
    bool foundSwap = false;
    MenuTestClick swapClick;
    for (const auto &region : probe.movies.Regions(swapId, swapEnd, swapOrigin.x, swapOrigin.y, true)) {
        if (region.index != 0) { continue; }
        swapClick = {region.x + region.width / 2, region.y + region.height / 2};
        foundSwap = true;
    }
    if (!foundSwap) { return 1; }
    std::vector<MenuTestClick> swapActions{{-100, -100, 1}, {-100, -100, swapEnd - swapStart + 1},
        swapClick, {-100, -100, pressEnd - pressStart + 1}};
    for (unsigned frame = 0; frame < 120; ++frame) { swapActions.push_back({-100, -100, 16}); }
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state, path,
        TestOutput::Path("refinery-store-transition/store-swap.png"), &swapActions, true, &probe.window) != -2 ||
        profile.activeWeaponSlot != 0 || state.store.shopGunSlot != 0) { return 1; }
    CProfileManager swapped;
    if (!LoadNativeProfile(toc, tables, swapped, path) || swapped.activeWeaponSlot != 0) { return 1; }
    std::printf("[store-return-slot-check] real-button swap=1-to-0 saved=0 failures=0\n");
    return 0;
}

int RunRefineryMenuCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) / ("refinery-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = true;
    if (!CheckUnselectedTabArtwork(view, "MDS_BUTTON_REFINE_SLOT_CATEGORY")) { return 1; }
    CPlayerProgress progress;
    progress.Bind(profile.nativeArchive->progression);
    progress.SetExperience(profile.experience);
    MenuState state;
    state.page = 3;
    state.refinementRequired = true;
    // Isolated fixture amount. Native gameplay loads its actual source balance.
    profile.xplodium = 250;
    const auto coins = profile.coins;
    const auto warbucks = profile.warbucks;
    const auto now = CurrentSeconds();
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_EXPLODIUM");
    CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned idle = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, idle, end)) { return 1; }
    const auto *coin = OriginalMenuData("MDS_ICON_STANDARD", 2);
    MovieRegion clamped, lastAnimation;
    if (coin == nullptr || coin->sprites[0] != 0x0004002B ||
        !view.movies.SpriteBounds(4, 43, clamped) || !view.movies.SpriteBounds(4, 32, lastAnimation) ||
        clamped.width != lastAnimation.width || clamped.height != lastAnimation.height) { return 1; }
    MovieRegion meter;
    if (!view.movies.Region(ordinal, 0, idle, meter)) { return 1; }
    view.Begin(3);
    view.SetTestClick({meter.x + meter.width / 2, meter.y + meter.height / 2});
    if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refinery.refineryTransfer != -1) { return 1; }
    view.clock = 2000;
    view.Begin(3);
    if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refinery.refineryTab != 1 ||
        view.Header(profile, progress, 3) == -3 || !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/refinery-original-standard.png"))) { return 1; }
    // Both category buttons use entry ordinal dispatch, not static parameter.
    MovieRegion category, graphic, hit;
    const auto *button = OriginalMenuData("MDS_BUTTON_REFINE_SLOT_CATEGORY", 1);
    if (button == nullptr || !view.movies.Region(ordinal, 16, state.refinery.refineryTime, category)) { return 1; }
    const unsigned categoryMovie = view.movies.Ordinal(button->movies[0]);
    if (!view.movies.Region(categoryMovie, 1, 0, graphic) || !view.movies.Region(categoryMovie, 0, 0, hit)) { return 1; }
    const float categoryX = category.x + category.width / 2 - (graphic.width * 2 + 8) / 2;
    for (unsigned tab : {0u, 1u}) {
        float x = categoryX;
        if (tab == 0) { x += graphic.width + 4; }
        view.Begin(3);
        view.SetTestClick({x + hit.x - 512 + hit.width / 2, category.y + hit.y - 384 + hit.height / 2});
        if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refinery.refineryTab != tab) {
            std::printf("[refinery-check] category failed expected=%u actual=%u\n", tab, state.refinery.refineryTab);
            return 1;
        }
        view.Begin(3);
        if (!DrawRefinery(view, state, profile, refinement, path, now) || view.Header(profile, progress, 3) == -3 ||
            !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/refinery-original-tab-") + std::to_string(tab) + ".png")) { return 1; }
        for (unsigned cell = 1; cell < 6; ++cell) {
            MovieRegion offline;
            if (!view.movies.Region(ordinal, cell, state.refinery.refineryTime, offline)) { return 1; }
            const auto before = profile.refinery.slots[tab * 6 + cell].state;
            view.Begin(3);
            view.SetTestClick({offline.x + offline.width / 2, offline.y + offline.height / 2});
            if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refinery.refineryTransfer != -1 ||
                profile.refinery.slots[tab * 6 + cell].state != before || profile.xplodium != 250) { return 1; }
        }
    }
    // Move the source parent region; no old screen-space hit rectangle remains.
    const CMovie original = *movie;
    if (!view.movies.Region(ordinal, 0, state.refinery.refineryTime, meter)) { return 1; }
    for (auto &object : movie->objects) {
        if (object.type != 6 || object.frames.empty()) { continue; }
        for (auto &frame : object.frames) { frame.x -= 260; }
        break;
    }
    MovieRegion moved;
    if (!view.movies.Region(ordinal, 0, state.refinery.refineryTime, moved)) { return 1; }
    view.Begin(3);
    view.SetTestClick({meter.x + meter.width / 2, meter.y + meter.height / 2});
    if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refinery.refineryTransfer != -1) { return 1; }
    view.Begin(3);
    view.SetTestClick({moved.x + moved.width / 2, moved.y + moved.height / 2});
    if (!DrawRefinery(view, state, profile, refinement, path, now) ||
        !FinishRefineryClick(view, state, profile, refinement, path, now, 6) || state.refinery.refineryTransfer != 6 || profile.xplodium != 250) {
        std::printf("[refinery-check] moved meter click failed transfer=%d\n", state.refinery.refineryTransfer);
        return 1;
    }
    *movie = original;
    // Save halfway through transfer: the original amount has not been committed.
    view.clock += 187;
    view.Begin(3);
    if (!DrawRefinery(view, state, profile, refinement, path, now) || profile.xplodium != 250 ||
        view.RefineryParticleCount(6) == 0 || !DrawRefineryOverlay(view, state) ||
        !profile.SaveToDisk(path) || !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/refinery-original-transfer.png"))) { return 1; }
    CProfileManager restored;
    if (!LoadNativeProfile(toc, tables, restored, path) || restored.xplodium != 250 || restored.coins != coins) { return 1; }
    view.clock += 188;
    view.Begin(3);
    if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refinery.refineryTransfer != -1 ||
        profile.xplodium != 0 || profile.coins != coins || profile.refinery.slots[6].state != 3 ||
        !state.refinementRequired) { return 1; }
    if (!ReloadNativeProfile(restored, path) || restored.refinery.slots[6].state != 3 || restored.xplodium != 0) { return 1; }
    const CMovie *fill = view.movies.GetMovie(view.movies.Ordinal("GLU_MOVIE_BUCKET_FILL"));
    if (fill == nullptr) { return 1; }
    view.clock += fill->duration;
    view.Begin(3);
    if (!DrawRefinery(view, state, profile, refinement, path, now) || state.refinery.refineryStatusChapter[6] != 3 ||
        !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/refinery-original-collect.png"))) { return 1; }
    if (!view.movies.Region(ordinal, 0, state.refinery.refineryTime, meter)) { return 1; }
    const auto yield = profile.refinery.GetRefinementSlotYield(6);
    view.Begin(3);
    view.SetTestClick({meter.x + meter.width / 2, meter.y + meter.height / 2});
    if (!DrawRefinery(view, state, profile, refinement, path, now) ||
        !FinishRefineryClick(view, state, profile, refinement, path, now, 6) || state.refinery.refineryTransfer != 6 || profile.coins != coins) { return 1; }
    view.clock += 187;
    view.Begin(3);
    if (!DrawRefinery(view, state, profile, refinement, path, now) || profile.coins != coins ||
        view.RefineryParticleCount(6) == 0 || view.Header(profile, progress, 3) == -3 ||
        !DrawRefineryOverlay(view, state) ||
        !GB_SAVE_FRAME(view.window, TestOutput::Path("refinery-coin-flight.png"))) { return 1; }
    view.clock += 188;
    view.Begin(3);
    if (!DrawRefinery(view, state, profile, refinement, path, now) || profile.coins != coins + yield ||
        state.refinementRequired || profile.refinery.slots[6].state != 1 || profile.warbucks != warbucks ||
        !ReloadNativeProfile(restored, path) || restored.coins != profile.coins || restored.refinery.slots[6].state != 1) { return 1; }
    if (state.page != 3) {
        std::printf("[refinery-check] collection must wait for navigation: expected page=3 actual=%u\n", state.page);
        return 1;
    }
    // Read the real header before/after the overlay: live arrival particles
    // must still be visible above its artwork after the coin sprite is gone.
    if (view.RefineryParticleCount(6) == 0 || view.Header(profile, progress, 3) == -3) { return 1; }
    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int headerHeight = viewport[3] / 4;
    std::vector<std::uint8_t> headerOnly(viewport[2] * headerHeight * 4);
    std::vector<std::uint8_t> arrival(headerOnly.size());
    glReadPixels(0, viewport[3] - headerHeight, viewport[2], headerHeight, GL_RGBA, GL_UNSIGNED_BYTE, headerOnly.data());
    if (!DrawRefineryOverlay(view, state)) { return 1; }
    glReadPixels(0, viewport[3] - headerHeight, viewport[2], headerHeight, GL_RGBA, GL_UNSIGNED_BYTE, arrival.data());
    if (headerOnly == arrival) {
        std::printf("[refinery-check] coin arrival particles hidden by header\n");
        return 1;
    }
    if (!GB_SAVE_FRAME(view.window, TestOutput::Path("refinery-coin-arrival.png"))) { return 1; }
    view.clock += 100;
    view.Begin(3);
    if (!DrawRefinery(view, state, profile, refinement, path, now) || view.Header(profile, progress, 3) == -3 ||
        !DrawRefineryOverlay(view, state) || profile.coins != coins + yield) { return 1; }
    glReadPixels(0, viewport[3] - headerHeight, viewport[2], headerHeight, GL_RGBA, GL_UNSIGNED_BYTE, headerOnly.data());
    if (headerOnly == arrival) { return 1; }
    if (!GB_SAVE_FRAME(view.window, TestOutput::Path("refinery-coin-dissipating.png"))) { return 1; }
    view.clock += 5000;
    view.Begin(3);
    if (!DrawRefinery(view, state, profile, refinement, path, now) || view.RefineryParticleCount(6) != 0 ||
        profile.coins != coins + yield) { return 1; }
    std::printf("[refinery-particle-check] flight arrival-overlay animation drain single-payout failures=0\n");
    // Reopening an empty refinery manually must not repeat the postgame route.
    MenuState manualState;
    manualState.page = 3;
    view.clock += 400;
    view.Begin(3);
    view.SetTestClick({meter.x + meter.width / 2, meter.y + meter.height / 2});
    if (!DrawRefinery(view, manualState, profile, refinement, path, now) || manualState.refinery.refineryTransfer != -1 ||
        manualState.page != 3 || manualState.refinery.refineryExitPending || profile.coins != coins + yield || glGetError() != 0) { return 1; }
    std::printf("[refinery-check] categories=2 offline=10 region-mutation transfer=375 fill collect save-reload failures=0\n");
    for (unsigned category = 0; category < 4; ++category) {
        if (CheckRefineryStoreTransition(toc, tables, view, refinement, category) != 0) { return 1; }
    }
    if (CheckOnlineRefinery(toc, tables, view, refinement) != 0) { return 1; }
    return 0;
}

int RunNavigationBarCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CProfileManager profile;
    const auto path = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) / ("header-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
    CPlayerProgress progress;
    progress.Bind(profile.nativeArchive->progression);
    progress.SetExperience(profile.experience);
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_HEADER");
    CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(2, start, end)) { return 1; }
    MovieRegion first;
    if (!view.movies.Region(ordinal, 0, start, first)) { return 1; }
    view.Begin(2);
    view.SetTestClick({first.x + first.width / 2, first.y + first.height / 2});
    if (view.Header(profile, progress, 2) != -1) { return 1; }
    view.clock = start / 2;
    view.Begin(2);
    view.SetTestClick({first.x + first.width / 2, first.y + first.height / 2});
    if (view.Header(profile, progress, 2) != -1 ||
        !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/header-original-opening.png"))) { return 1; }
    view.clock = start;
    for (unsigned index = 0; index < std::size(kOriginalNavigationBranches); ++index) {
        MovieRegion area;
        if (!view.movies.Region(ordinal, index, start, area)) { return 1; }
        view.Begin(2);
        view.SetTestClick({area.x + area.width / 2, area.y + area.height / 2});
        if (view.Header(profile, progress, 2) != static_cast<int>(index)) {
            std::printf("[header-check] original button failed index=%u\n", index);
            return 1;
        }
    }
    if (!GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/header-original-ready.png"))) { return 1; }
    // Change only the in-memory research copy; click must follow the source region.
    const CMovie original = *movie;
    for (auto &object : movie->objects) {
        if (object.type != 6 || object.frames.empty()) { continue; }
        for (auto &frame : object.frames) { frame.x += 37; frame.y += 190; }
        break;
    }
    MovieRegion moved;
    if (!view.movies.Region(ordinal, 0, start, moved) || moved.x == first.x || moved.y == first.y) { return 1; }
    view.Begin(2);
    view.SetTestClick({first.x + first.width / 2, first.y + first.height / 2});
    const int oldHit = view.Header(profile, progress, 2);
    if (oldHit != -1) { std::printf("[header-check] old position still hit=%d\n", oldHit); return 1; }
    view.Begin(2);
    view.SetTestClick({moved.x + moved.width / 2, moved.y + moved.height / 2});
    const int movedHit = view.Header(profile, progress, 2);
    if (movedHit != 0) { std::printf("[header-check] moved position hit=%d x=%.0f y=%.0f\n", movedHit, moved.x, moved.y); return 1; }
    *movie = original;
    view.animateNavigation = false;
    view.Begin(25);
    view.SetTestClick({first.x + first.width / 2, first.y + first.height / 2});
    if (view.Header(profile, progress, 25) != -1 ||
        !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/header-original-hidden.png"))) { return 1; }
    view.animateNavigation = true;
    view.Begin(2);
    view.SetTestClick({first.x + first.width / 2, first.y + first.height / 2});
    if (view.Header(profile, progress, 2) != -1) { return 1; }
    unsigned showStart = 0, showEnd = 0;
    if (!movie->GetChapterRange(1, showStart, showEnd)) { return 1; }
    view.clock += start - showStart;
    view.Begin(2);
    view.SetTestClick({first.x + first.width / 2, first.y + first.height / 2});
    if (view.Header(profile, progress, 2) != 0 || glGetError() != 0) { return 1; }
    std::printf("[header-check] seven native branches movie entrance hidden reentry mutated-position failures=0\n");
    return 0;
}

int RunMissionMenuCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CProfileManager profile;
    const auto path = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) / ("mission-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = false;
    const unsigned listOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_LIST");
    const unsigned cardOrdinal = view.movies.Ordinal("GLU_MOVIE_MISSION_BOX");
    const CMovie *list = view.movies.GetMovie(listOrdinal), *card = view.movies.GetMovie(cardOrdinal);
    unsigned testedWaves = 0, testedHordes = 0;
    unsigned start = 0, end = 0;
    if (list == nullptr || card == nullptr || !list->GetChapterRange(1, start, end)) { return 1; }
    for (unsigned planetIndex = 0; planetIndex < view.planetEntries.size(); ++planetIndex) {
        const auto &planet = view.planetEntries[planetIndex];
        if (planet.missions.empty()) { continue; }
        MenuState state;
        state.page = 21;
        state.planet = planetIndex;
        state.mode.modeSelected = true;
        state.mode.modeBound = true;
        state.mode.modePhase = 2;
        state.mode.modeTime = 1200;
        state.mode.modeLastTick = view.clock;
        bool launch = false;
        view.Begin(21);
        if (!DrawOriginalMissionInfo(view, state, profile, launch) || launch) { return 1; }
        const std::string prefix = TestOutput::Path("ui-original-2026-09-09/mission-native-") + std::to_string(planetIndex);
        if (!GB_SAVE_FRAME(view.window, prefix + "-list.png")) { return 1; }
        for (unsigned index = 0; index < planet.missions.size(); ++index) {
            state.missions.missionFocused = -1;
            state.missions.missionFirst = std::min(index, static_cast<unsigned>(planet.missions.size() - 3));
            state.missions.missionListTime = start;
            MovieRegion area;
            if (!view.movies.Region(listOrdinal, index - state.missions.missionFirst + 1, start, area)) { return 1; }
            view.Begin(21);
            view.SetTestClick({area.x + area.width / 2, area.y + area.height / 2});
            if (!DrawOriginalMissionInfo(view, state, profile, launch) || launch || state.missions.missionFocused != static_cast<int>(index)) {
                std::printf("[mission-menu-check] focus failed planet=%u index=%u actual=%d\n", planetIndex, index, state.missions.missionFocused);
                return 1;
            }
            view.clock += 1000;
            view.Begin(21);
            if (!DrawOriginalMissionInfo(view, state, profile, launch) || launch) { return 1; }
            const auto &mission = planet.missions[index];
            const auto &info = planet.missionInfo[index];
            if (index == 0 || index + 1 == planet.missions.size()) {
                if (!GB_SAVE_FRAME(view.window, prefix + "-expanded-" + std::to_string(index) + ".png")) { return 1; }
            }
            std::printf("[mission-menu-check] planet=%u mission=%u type=%u level=%d threshold=%u waves=%u prereqs=%zu locked=%d title=%s\n",
                planetIndex, index, mission.type, info.requiredLevel, mission.value64, info.waveCount, info.prerequisites.size(),
                OriginalMissionLocked(profile, mission, info), info.title.c_str());
            MovieRegion cardBounds, detail;
            if (!view.movies.Region(cardOrdinal, 0, state.missions.missionCardTime, cardBounds)) { return 1; }
            bool foundDetail = false;
            for (const auto &region : view.movies.Regions(cardOrdinal, state.missions.missionCardTime,
                kMenuWidth / 2 - cardBounds.width / 2, kMenuHeight / 2 - cardBounds.height / 2)) {
                if (region.index == 8) { detail = region; foundDetail = true; }
            }
            if (!foundDetail) { return 1; }
            if (mission.type == 1) {
                MovieRegion pageRegion, tabGraphic, scrollbar;
                if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_WAVE_SELECT"), 1, state.missions.missionWaveTime, pageRegion) ||
                    !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_BUTTON_LG"), 1, 0, tabGraphic) ||
                    !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_SCROLLBAR_HORIZ"), 0, 0, scrollbar)) { return 1; }
                const unsigned rowStep = static_cast<unsigned>(pageRegion.height + tabGraphic.height - scrollbar.height) / 3;
                for (unsigned localWave = 0; localWave < info.waveCount; ++localWave) {
                    state.missions.wavePage = localWave / 10;
                    const unsigned cell = localWave % 10;
                    const float x = detail.x + (cell % 5 + 1) * std::floor(pageRegion.width / 6);
                    const float y = detail.y + rowStep * (cell / 5 + 1) - tabGraphic.height;
                    view.Begin(21);
                    view.SetTestClick({x, y});
                    if (!DrawOriginalMissionInfo(view, state, profile, launch)) { return 1; }
                    const unsigned wave = mission.value64 + localWave;
                    const bool allowed = !OriginalMissionLocked(profile, mission, info) && wave <= NativeMissionProgress(profile, mission.level);
                    if (launch != allowed || (launch && (state.starMap.startingWave != static_cast<int>(wave) || !SameObject(state.selectedMission, planet.data.missions[index])))) {
                        std::printf("[mission-menu-check] wave click failed planet=%u index=%u local=%u launch=%d allowed=%d actual=%d\n",
                            planetIndex, index, localWave, launch, allowed, state.starMap.startingWave);
                        return 1;
                    }
                    ++testedWaves;
                }
            } else if (mission.type == 2 && !OriginalMissionLocked(profile, mission, info)) {
                const auto *play = OriginalMenuData("MDS_BUTTON_PLAY", 0);
                MovieRegion graphic, art;
                if (play == nullptr || !view.movies.Region(view.movies.Ordinal(play->movies[0]), 1, 0, graphic) ||
                    !view.movies.SpriteBounds(5, 42 + mission.value66, art)) { return 1; }
                const float x = detail.x + art.width + (detail.width - art.width - graphic.width) / 2;
                const float y = detail.y + detail.height - graphic.height;
                view.Begin(21);
                view.SetTestClick({x + graphic.width / 2, y + graphic.height / 2});
                if (!DrawOriginalMissionInfo(view, state, profile, launch) || !launch || state.hordeStart != index ||
                    !SameObject(state.selectedMission, planet.data.missions[index])) { return 1; }
                ++testedHordes;
            }
        }
    }
    CProfileManager fresh;
    if (!LoadNativeProfile(toc, tables, fresh, path / "fresh")) { return 1; }
    for (const auto &planet : view.planetEntries) {
        for (unsigned index = 0; index < planet.missions.size(); ++index) {
            const auto &mission = planet.missions[index];
            const auto &info = planet.missionInfo[index];
            // Actual archive export2 must drive both level and previous-round locks.
            if (info.prerequisites.empty()) { continue; }
            if (info.requiredLevel > 1 || mission.value64 != 0) {
                if (!OriginalMissionLocked(fresh, mission, info)) { return 1; }
            }
        }
    }
    std::printf("[mission-menu-check] all authored missions focus wave-clicks=%u horde-clicks=%u fresh-profile script locks failures=0\n",
        testedWaves, testedHordes);
    return 0;
}

int RunPlanetMenuCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) /
        ("planet-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = true;
    MenuState state;
    const unsigned mapOrdinal = view.movies.Ordinal("GLU_MOVIE_MAP_PARALAX_COPY");
    const unsigned modeOrdinal = view.movies.Ordinal("GLU_MOVIE_MULTIPLAYER_AND_VERSUS_MAP");
    const CMovie *map = view.movies.GetMovie(mapOrdinal);
    const CMovie *overlay = view.movies.GetMovie(modeOrdinal);
    unsigned begin = 0, end = 0;
    if (map == nullptr || overlay == nullptr || !overlay->GetChapterRange(0, begin, end)) { return 1; }
    // Zero is the menu's unbound-clock sentinel. Start this scripted fixture at
    // a nonzero tick, as the live window does; the elapsed time still comes from BIG.
    view.clock = 1;
    view.Begin(0);
    if (!DrawOriginalStarMap(view, state, profile) || !DrawOriginalModeOverlay(view, state)) { return 1; }
    view.clock += end;
    view.Begin(0);
    if (!DrawOriginalStarMap(view, state, profile) || !DrawOriginalModeOverlay(view, state) ||
        state.mode.modeTime != end ||
        !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/mode-original-expanded.png"))) { return 1; }
    for (unsigned mode : {1u, 2u, 0u}) {
        MovieRegion touch;
        if (!view.movies.Region(modeOrdinal, mode * 2 + 1, state.mode.modeTime, touch)) { return 1; }
        view.Begin(0);
        view.SetTestClick({touch.x + touch.width / 2, touch.y + touch.height / 2});
        if (!DrawOriginalModeOverlay(view, state)) { return 1; }
        if (mode != 0) {
            if (state.gameMode != 0 || state.mode.modeSelected || !state.storePromptRequested || state.storePromptIndex != 2) {
                std::printf("[planet-check] offline-click mode=%u time=%u expected-open=%u phase=%u selected=%d prompt=%d index=%u\n",
                    mode, state.mode.modeTime, end, state.mode.modePhase, state.mode.modeSelected, state.storePromptRequested, state.storePromptIndex);
                return 1;
            }
            if (!DrawStorePrompt(view, state)) { return 1; }
            view.clock += 1000;
            view.Begin(0);
            if (!DrawOriginalStarMap(view, state, profile) || !DrawOriginalModeOverlay(view, state) || !DrawStorePrompt(view, state) ||
                !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/mode-original-offline.png"))) { return 1; }
            state.storePopup.Hide();
            state.storePopup.Update(1000);
            state.storePopup.Update(1000);
        }
    }
    if (!state.mode.modeSelected || state.mode.modePhase != 1) { return 1; }
    view.clock += 150;
    view.Begin(0);
    if (!DrawOriginalStarMap(view, state, profile) || !DrawOriginalModeOverlay(view, state) ||
        !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/mode-original-folding.png"))) { return 1; }
    view.clock += overlay->duration;
    view.Begin(0);
    if (!DrawOriginalStarMap(view, state, profile) || !DrawOriginalModeOverlay(view, state) || state.mode.modePhase != 2 ||
        !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/mode-original-collapsed.png"))) { return 1; }
    MovieRegion before, middle, after;
    if (!view.movies.Region(modeOrdinal, 1, 700, before) || !view.movies.Region(modeOrdinal, 1, 850, middle) ||
        !view.movies.Region(modeOrdinal, 1, 1000, after) || after.width != 175 || after.height != 175 ||
        std::abs(middle.x - (before.x + after.x) / 2) > 1 || std::abs(middle.y - (before.y + after.y) / 2) > 1) { return 1; }
    std::printf("[planet-check] mode-native-regions anchors-interpolate unscaled-hit-box offline-no-fake-match failures=0\n");
    for (unsigned index = 0; index < profile.clearedWaves.size(); ++index) {
        const auto &planet = view.planetEntries[index];
        if (planet.missions.empty() || planet.missions[0].type != 1 ||
            !SameObject(planet.missions[0].level, profile.nativeArchive->survivalLevels[index]) ||
            !map->GetChapterRange(planet.data.mapSlot - 1, begin, end)) { return 1; }
        // Test positions the original playback cursor at its authored stop.
        state.page = 0;
        state.starMap.starSelectedSlot = -1;
        state.starMap.starLocked = false;
        state.starMap.starTargetTime = -1;
        state.starMap.starTime = end;
        view.clock += 1000;
        MovieRegion touch;
        if (!view.movies.Region(mapOrdinal, planet.data.mapSlot, end, touch)) { return 1; }
        view.Begin(0);
        view.SetTestClick({touch.x + touch.width / 2, touch.y + touch.height / 2});
        if (!DrawOriginalStarMap(view, state, profile) || state.starMap.starSelectedSlot != static_cast<int>(planet.data.mapSlot) || state.page != 0) { return 1; }
        view.clock += 1000;
        view.Begin(0);
        if (!DrawOriginalStarMap(view, state, profile) || !state.starMap.starLocked || !DrawOriginalModeOverlay(view, state)) { return 1; }
        const std::string screenshot = TestOutput::Path("ui-original-2026-09-09/planet-selected-") + std::to_string(index) + ".png";
        if (!GB_SAVE_FRAME(view.window, screenshot)) { return 1; }
        if (planet.data.requiredLevel > 1) {
            // Only the isolated profile changes; geometry and lock art stay in BIG.
            const auto experience = profile.experience;
            profile.experience = 0;
            view.Begin(0);
            if (!DrawOriginalStarMap(view, state, profile) ||
                !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/planet-level-locked-") + std::to_string(index) + ".png")) { return 1; }
            profile.experience = profile.nativeArchive->progression.GetExperienceForLevel(planet.data.requiredLevel) + 1;
            view.Begin(0);
            if (!DrawOriginalStarMap(view, state, profile) ||
                !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/planet-level-unlocked-") + std::to_string(index) + ".png")) { return 1; }
            profile.experience = experience;
        }
        view.Begin(0);
        view.SetTestClick({touch.x + touch.width / 2, touch.y + touch.height / 2});
        if (!DrawOriginalStarMap(view, state, profile) || !state.starMap.starEntering || state.page != 0) { return 1; }
        view.clock += 1000;
        view.Begin(0);
        if (!DrawOriginalStarMap(view, state, profile) || state.page != 21 || state.planet != index) { return 1; }
        std::printf("[planet-check] slot=%u original-thumb double-select exit-chapter host=%u level=%u:%u failures=0\n",
            planet.data.mapSlot, index, planet.missions[0].level.packHash, planet.missions[0].level.localIndex);
    }
    return 0;
}

int RunSocialOfflineCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) /
        ("social-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
    const auto originalCoins = profile.coins;
    const auto originalBucks = profile.warbucks;
    // Make every legacy condition pass, then prove the native boundary blocks it.
    profile.clearedWaves.fill(500);
    profile.enemyKills.fill(1000);
    for (unsigned index = 0; index < 8; ++index) {
        if (profile.ClaimActivity(index)) { return 1; }
    }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    const auto *button = OriginalMenuData("MDS_BUTTON_CONNECTIVITY", 0);
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_OFFLINE_BROHOOD");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (button == nullptr || button->action != 86 || movie == nullptr || !movie->GetChapterRange(1, start, end)) { return 1; }
    for (unsigned page : {4u, 5u, 11u, 13u}) {
        for (bool credentials : {false, true}) {
            MenuState state;
            state.page = page;
            GameHostSettings().isConnected = credentials;
            view.Begin(page);
            if (!DrawOriginalSocialOffline(view, state, credentials)) { return 1; }
            MovieRegion area;
            if (!view.movies.Region(ordinal, 0, start, area)) { return 1; }
            bool found = false;
            MenuTestClick click;
            for (const auto &part : view.movies.Regions(view.movies.Ordinal(button->movies[0]), 0, area.x, area.y)) {
                if (part.index == 0) { click = {part.x + part.width / 2, part.y + part.height / 2}; found = true; }
            }
            if (!found) { return 1; }
            view.clock += movie->duration * 3;
            view.Begin(page);
            view.SetTestClick(click);
            if (!DrawOriginalSocialOffline(view, state, credentials) || view.ExchangeClick(false) ||
                state.page != page || state.promotion.IsActive() || state.social.socialTime < start || state.social.socialTime > end) { return 1; }
            const auto screenshot = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) /
                ("social-original-" + std::to_string(page) + "-" + std::to_string(credentials) + ".png");
            if (!GB_SAVE_FRAME(view.window, screenshot.string())) { return 1; }
            std::printf("[social-check] page=%u credentials=%u original-regions retry-offline failures=0\n", page, credentials);
        }
    }
    // The local connection must pass the same mode selection path as a click.
    GameHostSettings().isConnected = true;
    MenuState onlineMode;
    view.animateNavigation = false;
    view.Begin(0);
    if (!DrawOriginalModeOverlay(view, onlineMode)) { return 1; }
    MovieRegion multiplayer;
    if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_MULTIPLAYER_AND_VERSUS_MAP"),
        3, onlineMode.mode.modeTime, multiplayer)) { return 1; }
    view.Begin(0);
    view.SetTestClick({multiplayer.x + multiplayer.width / 2, multiplayer.y + multiplayer.height / 2});
    if (!DrawOriginalModeOverlay(view, onlineMode) || onlineMode.gameMode != 1 ||
        onlineMode.storePromptRequested) {
        std::printf("[local-online-check] connected mode selection blocked\n");
        return 1;
    }
    GameHostSettings().isConnected = false;
    if (profile.coins != originalCoins || profile.warbucks != originalBucks || !profile.SaveToDisk(path) ||
        !ReloadNativeProfile(profile, path) || profile.coins != originalCoins || profile.warbucks != originalBucks) { return 1; }
    std::printf("[social-check] native-no-legacy-reward wallet-reload failures=0\n");
    return RunLocalOnlineCheck(bigDirectory);
}

int RunOptionsCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) /
        ("options-check-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = true;
    MenuState state;
    state.page = 6;
    const unsigned list = view.movies.Ordinal("GLU_MOVIE_LIST_MENU");
    const unsigned button = view.movies.Ordinal("GLU_MOVIE_LIST_MENU_BUTTON");
    const CMovie *movie = view.movies.GetMovie(list);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, start, end)) { return 1; }
    bool changed = false;
    view.Begin(6);
    if (!DrawOptions(view, state, profile, changed)) { return 1; }
    view.clock += start / 2;
    view.Begin(6);
    view.SetTestClick({240, 360});
    if (!DrawOptions(view, state, profile, changed) || changed || state.settings.optionsOpening != start / 2 ||
        !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/options-opening.png"))) { return 1; }
    const unsigned order[] = {0, 1, 3, 4, 5, 7, 9, 6, 2, 8};
    for (unsigned index : order) {
        changed = false;
        state.page = 6;
        state.settings.optionsScroll = std::clamp(float(static_cast<int>(index) - 1), 0.0f, 7.0f);
        state.settings.optionsTarget = state.settings.optionsScroll;
        view.clock += movie->duration;
        view.Begin(6);
        if (!DrawOptions(view, state, profile, changed)) { return 1; }
        const int base = static_cast<int>(state.settings.optionsScroll) - 1;
        MenuTestClick click;
        bool found = false;
        for (const auto &region : view.movies.Regions(list, start)) {
            if (region.type < 2 || base + static_cast<int>(region.type) - 2 != static_cast<int>(index)) { continue; }
            for (const auto &part : view.movies.Regions(button, 0, region.x + region.width / 2, region.y + region.height / 2)) {
                if (part.index == 0) { click = {part.x + part.width / 2, part.y + part.height / 2}; found = true; }
            }
        }
        const auto before = profile;
        const auto *entry = OriginalMenuData("MDS_OPTIONS", index);
        const auto label = OptionsText(view, profile, index, 0);
        const auto body = OptionsText(view, profile, index, 1);
        if (!found || entry == nullptr || label.empty() || body.empty()) { return 1; }
        view.Begin(6);
        view.SetTestClick(click);
        if (!DrawOptions(view, state, profile, changed)) { return 1; }
        if (entry->action != 1 && state.settings.optionsFocus != index) { return 1; }
        if (entry->action == 9 && profile.soundEnabled == before.soundEnabled) { return 1; }
        if (entry->action == 10 && profile.musicEnabled == before.musicEnabled) { return 1; }
        if (entry->action == 17 && profile.options.AutoBro() != (before.options.AutoBro() + 1) % 3) { return 1; }
        if (entry->action == 77 && profile.options.NotificationsEnabled() == before.options.NotificationsEnabled()) { return 1; }
        if (entry->action == 113 && profile.pushChallenges == before.pushChallenges) { return 1; }
        if (entry->action == 79 && state.page != 29) { return 1; }
        if (entry->action == 1 && state.page != 8) { return 1; }
        if (entry->action == 20 && state.page != 6) { return 1; }
        const auto expectedOptions = profile.options;
        const bool expectedSound = profile.soundEnabled, expectedMusic = profile.musicEnabled, expectedPush = profile.pushChallenges;
        if (!profile.SaveToDisk(path) || !profile.LoadFromDisk(path) || profile.soundEnabled != expectedSound ||
            profile.musicEnabled != expectedMusic || profile.pushChallenges != expectedPush ||
            profile.options.AutoBro() != expectedOptions.AutoBro() ||
            profile.options.NotificationsEnabled() != expectedOptions.NotificationsEnabled() ||
            profile.coins != before.coins || profile.warbucks != before.warbucks) { return 1; }
        view.clock += movie->duration;
        view.Begin(6);
        if (!DrawOptions(view, state, profile, changed) || glGetError() != 0 ||
            !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/options-entry-") + std::to_string(index) + ".png")) { return 1; }
        std::printf("[options-check] index=%u action=%u label=%s body=%zu original-layout native-reload failures=0\n",
            index, entry->action, label.c_str(), body.size());
        if (index == 9) {
            MovieRegion bodyArea;
            if (!view.movies.Region(list, 8, start, bodyArea)) { return 1; }
            view.Begin(6);
            view.SetTestClick({bodyArea.x + bodyArea.width / 2, bodyArea.y + bodyArea.height / 2});
            view.dragY = -bodyArea.height / 2;
            if (!DrawOptions(view, state, profile, changed) || state.settings.optionsBodyScroll <= 0 ||
                !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/options-about-scroll.png"))) { return 1; }
            view.clock += movie->duration;
            view.Begin(6);
            view.SetTestClick({bodyArea.x + bodyArea.width / 2, bodyArea.y + bodyArea.height / 2});
            view.dragY = -bodyArea.height * 10;
            if (!DrawOptions(view, state, profile, changed) ||
                !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/options-about-last-page.png"))) { return 1; }
            std::printf("[options-check] authored text pages scrollbar drag-clamp failures=0\n");
        }
    }
    state.page = 6;
    state.settings.optionsFocus = 2;
    state.Navigate(8);
    view.animateNavigation = false;
    unsigned helpCount = 0;
    while (OriginalMenuData("MDS_HELP", helpCount)) { ++helpCount; }
    for (unsigned index = 0; index < helpCount; ++index) {
        state.settings.optionsFocus = index;
        state.settings.optionsScroll = state.settings.optionsTarget = std::clamp(float(int(index) - 1), 0.0f, float(helpCount - 3));
        view.clock += movie->duration;
        view.Begin(8);
        if (!DrawOptions(view, state, profile, changed)) { return 1; }
        if (OptionsText(view, profile, index, 0, "MDS_HELP").empty()) { return 1; }
    }
    if (!GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/help-last-item.png"))) { return 1; }
    const auto regions = view.movies.Regions(list, start, 512, 384, true);
    const auto *back = OriginalMenuData("MDS_BUTTON_BACK", 0);
    bool returned = false;
    for (const auto &region : regions) {
        if (region.index != regions.size() - 3) { continue; }
        MovieRegion touch;
        bool found = false;
        for (const auto &part : view.movies.Regions(view.movies.Ordinal(back->movies[0]), 0,
            region.x + region.width / 2, region.y + region.height / 2, true)) {
            if (part.index == 0) { touch = part; found = true; }
        }
        if (!found) { return 1; }
        view.Begin(8);
        view.SetTestClick({touch.x + touch.width / 2, touch.y + touch.height / 2});
        if (!DrawOptions(view, state, profile, changed)) { return 1; }
        returned = state.page == 6 && state.settings.optionsFocus == 2;
    }
    std::printf("[options-check] help-items=%u original-back=%d\n", helpCount, returned);
    if (!returned || helpCount == 0) { return 1; }
    return 0;
}

/** Retained milestone: aggregate the original menu paths, never legacy mock UI. */
int RunGameMenuCheck(const std::string &bigDirectory) {
    if (RunNavigationBarCheck(bigDirectory) != 0 || RunOptionsCheck(bigDirectory) != 0 ||
        RunSocialOfflineCheck(bigDirectory) != 0 || RunPromotionCheck(bigDirectory) != 0 ||
        RunLoadingWipeCheck(bigDirectory) != 0) { return 1; }
    std::printf("[menu-check] original header, list, social, promotions and transitions failures=0\n");
    return 0;
}

int RunPromotionCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadStoreCatalog(toc, tables, store) ||
        !LoadWeaponCatalog(toc, tables, weapons) || !LoadArmorCatalog(toc, tables, armor)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const std::filesystem::path path = TestOutput::Path("ui-restoration-promotion-profile");
    if (!LoadNativeProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.animateNavigation = false;
    unsigned failures = 0;
    for (unsigned row = 0; row < 2; ++row) {
        MenuState state;
        state.page = 2;
        view.Begin(2);
        if (!DrawStore(view, toc, tables, profile, 200, store, weapons, armor, state, path)) { return 1; }
        MovieRegion slot, body;
        if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_STORE_SCROLL"), 1, view.storeRestTime, slot)) { return 1; }
        StoreCardFace face;
        face.x = slot.x; face.y = slot.y + row * (slot.height / 2 + 5);
        if (!CardRegion(view, view.movies.Ordinal("GLU_MOVIE_SHOP_BOX"), 0, face, body)) { return 1; }
        view.SetTestClick({body.x + body.width / 2, body.y + body.height / 2});
        if (!DrawStore(view, toc, tables, profile, 200, store, weapons, armor, state, path)) { return 1; }
        unsigned action = 125;
        if (row == 1) { action = 130; }
        const bool opened = state.page == 2 && state.promotion.IsActive() && state.promotion.Action() == action;
        if (!opened) { ++failures; }
        std::printf("[promotion-check] card=%u page=%u modal=%d action=%u failures=%u\n",
            row, state.page, state.promotion.IsActive(), state.promotion.Action(), failures);
        if (!opened) { continue; }
        const CMovie *movie = view.movies.GetMovie(state.promotion.Ordinal());
        unsigned start = 0, end = 0;
        if (!movie->GetChapterRange(2, start, end)) { return 1; }
        if (state.promotion.Click(body.x, body.y) != 0) { ++failures; }
        state.promotion.Update(start);
        view.Begin(2);
        view.ExchangeClick(false);
        if (!DrawStore(view, toc, tables, profile, 200, store, weapons, armor, state, path)) { return 1; }
        CPlayerProgress::Template progressTemplate;
        CPlayerProgress progress;
        if (!LoadPlayerProgress(toc, tables, progressTemplate)) { return 1; }
        progress.Bind(progressTemplate); progress.SetExperience(profile.experience);
        if (view.Header(profile, progress, 2) == -3 || !state.promotion.Draw(view.movies) ||
            !state.promotion.IsReady() || state.promotion.Hits().size() != 3) { return 1; }
        const std::string capture = TestOutput::Path("ui-promotion-") + std::to_string(action) + ".png";
        if (!GB_SAVE_FRAME(view.window, capture)) { return 1; }
        const auto hit = state.promotion.Hits().back().first;
        if (state.promotion.Click(hit.x + hit.width / 2, hit.y + hit.height / 2) != 45 ||
            state.promotion.IsReady()) { ++failures; }
        state.promotion.Update(movie->duration);
        if (state.promotion.IsActive() || state.page != 2) { ++failures; }
        std::printf("[promotion-check] movie=%u chapters=%zu close-region=1 retained-store=1 failures=%u\n",
            state.promotion.Ordinal(), movie->chapters.size(), failures);
    }
    profile.coins = 0;
    for (unsigned index = 0; index < 3; ++index) {
        MenuState state;
        ShowStoreFundsPrompt(state, store, profile, 0, 1, false); // Test-only shortage.
        view.Begin(2);
        if (!DrawStorePrompt(view, state)) { return 1; }
        const unsigned popup = view.movies.Ordinal("GLU_MOVIE_POPUP");
        view.clock += view.movies.GetMovie(popup)->duration;
        view.Begin(2);
        if (!DrawStorePrompt(view, state) || !state.storePopup.IsReady()) { return 1; }
        if (index == 0 && !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-store-funds-original.png"))) { return 1; }
        const auto *entry = OriginalMenuData("MDS_BUTTON_STORE_PROMPT", index);
        MovieRegion area;
        if (!view.movies.Region(popup, index + 2, state.storePopup.MovieTime(), area)) { return 1; }
        view.SetTestClick({area.x + area.width / 2, area.y + area.height / 2});
        if (!DrawStorePrompt(view, state)) { return 1; }
        if (entry->action == 71 && !state.currencyPending) { ++failures; }
        if (entry->action != 71 && state.storePopup.IsReady()) { ++failures; }
        std::printf("[promotion-check] funds-button=%u action=%u failures=%u\n", index, entry->action, failures);
    }
    return failures != 0;
}

int RunLoadingWipeCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CWindow window;
    if (!window.Open("Gun Bros", 1600, 1200)) { return 1; }
    MovieRenderer movies;
    auto &core = *toc.GetPack(toc.GetCorePackIndex());
    if (!movies.Init(core, core)) { return 1; }
    CRefinementManager::Template refinement;
    CProfileManager profile;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    profile.Reset(core.GetPackHash(), refinement);
    if (!LoadNativeProfile(toc, tables, profile, TestOutput::Path("ui-restoration-loading-profile"), TestOutput::Fixtures())) { return 1; }
    unsigned count = 1, failures = 0;
    for (unsigned index = 0; index < count; ++index) {
        OriginalLoadingSplash splash;
        if (!splash.Init(movies, index, &profile)) { return 1; }
        count = splash.Count();
        glViewport(0, 0, 1600, 1200);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!splash.Draw(splash.IdleStart()) || splash.ImageHandle() == 0 || splash.TextHandle() == 0) { return 1; }
        if (index < 3 && !GB_SAVE_FRAME(window, TestOutput::Path("ui-loading-cg-") + std::to_string(index) + ".png")) { return 1; }
    }
    MenuWipe wipe;
    glClearColor(1, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    movies.Rectangle(0, 0, 64, 64, 0, 1, 0);
    movies.Rectangle(960, 704, 64, 64, 1, 1, 1);
    if (!wipe.Remember() || !wipe.Begin(movies)) { return 1; }
    const unsigned duration = wipe.Duration();
    for (const char *name : {"GLU_MOVIE_STORE_SCROLL", "GLU_MOVIE_SHOP_BOX", "GLU_MOVIE_SPLASH"}) {
        const auto *source = movies.GetMovie(movies.Ordinal(name));
        std::printf("[ui-chapters] %s duration=%u chapters=", name, source->duration);
        for (auto time : source->chapters) { std::printf("%u,", time); }
        std::printf("\n");
    }
    for (unsigned frame = 0; frame < 3; ++frame) {
        if (frame > 0) { wipe.Update((duration + 1) / 2); }
        glClearColor(0, 0, 1, 1); glClear(GL_COLOR_BUFFER_BIT);
        if (!wipe.Draw()) { return 1; }
        std::vector<std::uint8_t> pixels(1600 * 1200 * 4);
        glReadPixels(0, 0, 1600, 1200, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        unsigned red = 0, blue = 0;
        for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
            if (pixels[pixel] > 250 && pixels[pixel + 2] < 5) { ++red; }
            if (pixels[pixel] < 5 && pixels[pixel + 2] > 250) { ++blue; }
        }
        if (frame == 0) {
            const unsigned topLeft = (1150 * 1600 + 50) * 4;
            const unsigned bottomRight = (50 * 1600 + 1550) * 4;
            if (red == 0 || pixels[topLeft] > 5 || pixels[topLeft + 1] < 250 ||
                pixels[bottomRight] < 250 || pixels[bottomRight + 1] < 250) { ++failures; }
        }
        if (frame == 1 && (red == 0 || blue == 0)) { ++failures; }
        if (frame == 2 && (red != 0 || wipe.IsActive())) { ++failures; }
        if (!GB_SAVE_FRAME(window, TestOutput::Path("ui-wipe-fixture-") + std::to_string(frame) + ".png")) { return 1; }
        std::printf("[loading-wipe-check] frame=%u time=%u duration=%u old-pixels=%u new-pixels=%u failures=%u\n",
            frame, wipe.Time(), duration, red, blue, failures);
    }
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    CPlayerProgress::Template progress;
    if (!LoadStoreCatalog(toc, tables, store) || !LoadWeaponCatalog(toc, tables, weapons) ||
        !LoadArmorCatalog(toc, tables, armor) || !LoadPlayerProgress(toc, tables, progress)) { return 1; }
    const auto header = movies.Ordinal("GLU_MOVIE_HEADER");
    unsigned headerStart = 0, headerEnd = 0;
    if (!movies.GetMovie(header)->GetChapterRange(2, headerStart, headerEnd)) { return 1; }
    MovieRegion options, storeTab, play, categoryBar;
    if (!movies.Region(header, 5, headerStart, options) || !movies.Region(header, 3, headerStart, storeTab) ||
        !movies.Region(header, 0, headerStart, play) ||
        !movies.Region(movies.Ordinal("GLU_MOVIE_STORE_MENU"), kStoreCategoryRegion, 0, categoryBar)) { return 1; }
    std::vector<MenuTestClick> targets = {
        {options.x + options.width / 2, options.y + options.height / 2},
        {play.x + play.width / 2, play.y + play.height / 2}
    };
    float categoryX = categoryBar.x;
    for (unsigned index = 0; index < 4; ++index) {
        const auto *entry = OriginalMenuData("MDS_BUTTON_STORE_CATEGORIES", index);
        MovieRegion label, touch;
        if (!movies.Region(movies.Ordinal(entry->movies[0]), 1, 0, label) ||
            !movies.Region(movies.Ordinal(entry->movies[0]), 0, 0, touch)) { return 1; }
        if (index != 0) { targets.push_back({categoryX + touch.x - label.x + touch.width / 2,
            categoryBar.y + touch.y - label.y + touch.height / 2}); }
        categoryX += label.width + kCategoryGap;
    }
    for (unsigned index = 0; index < targets.size(); ++index) {
        MenuState state;
        state.page = 2;
        auto target = targets[index];
        target.advanceMs = 1;
        target.renderDelayMs = duration * 2;
        const std::vector<MenuTestClick> clicks = {{-100, -100, 1}, {-100, -100, headerEnd + 1},
            {-100, -100, headerEnd + 1}, {-100, -100, headerEnd + 1}, target,
            {storeTab.x + storeTab.width / 2, storeTab.y + storeTab.height / 2, duration / 2}};
        const std::string capture = TestOutput::Path("ui-wipe-menu-") + std::to_string(index) + ".png";
        MenuTransitionTrace trace;
        if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state,
            TestOutput::Path("ui-restoration-loading-profile"), capture, &clicks, true, &window, true, &trace) != -2) { return 1; }
        const unsigned expectedPage[] = {6, 0, 2, 2, 2};
        // Targets 0 and 1 leave the STORE branch and sweep, which also swallows
        // the store click that follows. The categories are menus inside that
        // branch, so they change with no sweep and nothing to swallow.
        const bool branchChange = index < 2;
        bool wrong = state.page != expectedPage[index] || (index >= 2 && state.store.shopCategory != index - 1);
        if (branchChange && (!trace.active || trace.time != duration / 2 || trace.starts != 1)) { wrong = true; }
        if (!branchChange && (trace.active || trace.starts != 0)) { wrong = true; }
        if (wrong) { ++failures; }
        std::printf("[loading-wipe-check] cold-load=%u midpoint-active=%d time=%u starts=%u expected-sweep=%d\n",
            target.renderDelayMs, trace.active, trace.time, trace.starts, branchChange);
        std::printf("[loading-wipe-check] real-shell target=%u page=%u category=%u blocked-store-click=%d failures=%u\n",
            index, state.page, state.store.shopCategory, branchChange, failures);
    }
    // Actual planet click -> authored reticle exit -> REV page, with cold work.
    MenuState revolution;
    revolution.page = 0;
    revolution.mode.modeSelected = true;
    const unsigned mapId = movies.Ordinal("GLU_MOVIE_MAP_PARALAX_COPY");
    const auto *mapMovie = movies.GetMovie(mapId);
    unsigned mapStart = 0, mapEnd = 0, exitStart = 0, exitEnd = 0;
    if (!mapMovie || !mapMovie->GetChapterRange(0, mapStart, mapEnd) ||
        !movies.GetMovie(movies.Ordinal("GLU_MOVIE_MAP_RETICLE"))->GetChapterRange(2, exitStart, exitEnd)) { return 1; }
    MovieRegion planet;
    if (!movies.Region(mapId, 1, mapEnd, planet)) { return 1; }
    MenuTestClick planetClick{planet.x + planet.width / 2, planet.y + planet.height / 2, 1};
    // No trailing store click: the REV list opens inside the PLAY branch with no
    // sweep to swallow it, so such a click would simply leave for the store.
    const std::vector<MenuTestClick> revolutionClicks = {{-100, -100, 1}, {-100, -100, headerEnd + 1},
        {-100, -100, headerEnd + 1}, {-100, -100, headerEnd + 1}, planetClick,
        {-100, -100, exitEnd + 1, duration * 2}, {-100, -100, duration / 2}};
    MenuTransitionTrace revolutionTrace;
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, revolution,
        TestOutput::Path("ui-restoration-loading-profile"), TestOutput::Path("ui-wipe-revolution.png"), &revolutionClicks,
        true, &window, true, &revolutionTrace) != -2) { return 1; }
    if (revolution.page != 21 || revolutionTrace.active || revolutionTrace.starts != 0) { ++failures; }
    std::printf("[loading-wipe-check] planet-to-REV page=%u active=%d starts=%u expected-sweep=0 failures=%u\n",
        revolution.page, revolutionTrace.active, revolutionTrace.starts, failures);
    // Startup has the original launch image and animated core 0:124 only.
    {
        LoadingScreen startup(window, movies, tables, &profile, false, true);
        if (!startup.IsValid() || !startup.CaptureFrame(TestOutput::Path("ui-startup-loading.png"), 0) ||
            !startup.CaptureFrame(TestOutput::Path("ui-startup-loading-next.png"), 200)) { return 1; }
        startup.OnResourceRead();
        startup.Finish();
        if (!startup.IsValid()) { return 1; }
    }
    std::printf("[loading-wipe-check] original-CG-and-STR-pairs=%u failures=%u\n", count, failures);
    return failures != 0;
}

/** Real BIG card rendering and tab input, with isolated save data. */
int RunPostGamePresentationCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CPlayerProgress::Template progress;
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    if (!LoadPlayerProgress(toc, tables, progress) || !LoadRefinementTemplate(toc, tables, refinement) ||
        !LoadStoreCatalog(toc, tables, store) || !LoadWeaponCatalog(toc, tables, weapons) ||
        !LoadArmorCatalog(toc, tables, armor)) { return 1; }
    CProfileManager profile;
    const auto path = std::filesystem::path(TestOutput::Path("postgame-presentation")) / std::to_string(GetTickCount64());
    if (!LoadNativeProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
    CWindow window;
    if (!window.Open("Postgame presentation verification", 1600, 1200)) { return 1; }
    GameMenu view(&window);
    if (!view.Open(toc, tables, &profile)) { return 1; }
    unsigned failures = 0;
    for (unsigned index : {0u, 1u, 4u, 5u}) {
        const auto *entry = OriginalMenuData("MDS_ICON_POSTGAME", index);
        if (!entry) { return 1; }
        const unsigned sprite = entry->sprites[0];
        const unsigned duration = view.movies.SpriteDuration(sprite >> 16, sprite & 255);
        MovieRegion icon;
        icon.index = 1; icon.x = 462; icon.y = 334; icon.width = 100; icon.height = 100;
        std::vector<std::uint8_t> first, current(320 * 320 * 4);
        unsigned changed = 0;
        std::size_t particles = 0;
        unsigned lateChanged = 0;
        std::size_t lateParticles = 0;
        std::vector<std::uint8_t> previous;
        // Sample the final three seconds of a 30-second run to catch a finite
        // emitter that stops after the short initial sparkle check has passed.
        for (unsigned time = 0; time <= 30000; time += 20) {
            view.Begin(27);
            unsigned delta = 20;
            if (time == 0) { delta = 0; }
            if (!view.AdvancePostGameEffect(index, delta)) { return 1; }
            particles = std::max(particles, view.PostGameParticleCount(index));
            const std::string value;
            PostGameCardCallbacks callback(view, *entry, value, time);
            if (!callback.DrawMovieRegion(icon)) { return 1; }
            if (time == 1000 && !GB_SAVE_FRAME(window, TestOutput::Path("postgame-icon-") + std::to_string(index) + "-1000.png")) { return 1; }
            glReadPixels(640, 440, 320, 320, GL_RGBA, GL_UNSIGNED_BYTE, current.data());
            if (time >= 27000) {
                if (current != previous) { ++lateChanged; }
                lateParticles = std::max(lateParticles, view.PostGameParticleCount(index));
            }
            previous = current;
            if (first.empty()) { first = current; }
            else if (current != first) {
                if (changed == 0 && !GB_SAVE_FRAME(window, TestOutput::Path("postgame-icon-") + std::to_string(index) + "-sparkle.png")) { return 1; }
                ++changed;
            }
        }
        if (changed == 0 || particles == 0 || lateChanged == 0 || lateParticles == 0) { ++failures; }
        std::printf("[postgame-loop-check] icon=%u late-changed=%u late-particles=%zu failures=%u\n",
            index, lateChanged, lateParticles, failures);
        std::printf("[postgame-presentation-check] icon=%u sprite=%u:%u duration=%u changed=%u particles=%zu failures=%u\n",
            index, sprite >> 16, sprite & 255, duration, changed, particles, failures);
    }
    // Inspect the same original ENEMY resources in both spawn paths.
    std::vector<EnemyTemplateData> enemies;
    MenuState casualtyState;
    casualtyState.page = 28;
    casualtyState.result.horde = true;
    if (!LoadEnemyCatalog(toc, tables, enemies)) { return 1; }
    for (const auto &entry : enemies) {
        std::vector<std::uint8_t> bytes;
        if (!tables.ReadSectionResource(entry.packHash, GameSection::Enemy, entry.ordinal, bytes)) { return 1; }
        CArrayInputStream input(bytes);
        input.ReadUInt8();
        CGameAssetRef nameRef;
        nameRef.Init(input);
        const std::string name = ReadGameString(toc, nameRef);
        if (name.find("Zom") != 0 && name.find("ZOM") != 0 && name != "CUTTLES" && name != "Cuttles") { continue; }
        EnemyCasualty casualty;
        casualty.resource.packHash = entry.packHash;
        casualty.resource.localIndex = static_cast<std::uint8_t>(entry.ordinal);
        casualty.name = name;
        casualty.count = 5;
        casualtyState.result.casualties.push_back(casualty);
        for (auto mode : {EnemySpawnMode::Menu, EnemySpawnMode::Level}) {
            EnemyModel model;
            if (!LoadEnemyModel(tables, entry, false, nullptr, mode, model)) { return 1; }
            const int config = EnemyPartConfig(model, 0);
            if (config < 0) { return 1; }
            const auto &bounds = model.configs[config]->mesh.GetBounds();
            const auto &meshConfig = entry.moveSet.GetMeshConfigs()[config];
            std::vector<std::uint8_t> meshBytes;
            if (!tables.ReadSectionResource(entry.moveSet.GetPackHash(), GameSection::Mesh, meshConfig.meshOrdinal, meshBytes)) { return 1; }
            CArrayInputStream meshInput(meshBytes);
            CMesh originalMesh;
            // CMoveSetMesh::LoadMesh :123178 supplies its move ranges to CMesh.
            if (!originalMesh.Init(meshInput, &entry.moveSet)) { return 1; }
            const auto &originalBounds = originalMesh.GetBounds();
            if (std::abs(bounds.inverseExtent - originalBounds.inverseExtent) > 0.00001f) { ++failures; }
            std::printf("[enemy-scale-check] expected-inverse=%.5f retained-bounds=%.2f/%.2f/%.2f failures=%u\n",
                originalBounds.inverseExtent, originalBounds.maxX-originalBounds.minX,
                originalBounds.maxY-originalBounds.minY, originalBounds.maxZ-originalBounds.minZ, failures);
            std::printf("[enemy-scale-check] %s %s mode=%d game=%.1f ui=%.1f factor=%.4f bounds=%.2f/%.2f/%.2f inverse=%.5f world=%.4f\n",
                entry.owner.c_str(), name.c_str(), int(mode), entry.gameScale, entry.uiScalePercent,
                model.enemy.combat.scaleFactor, bounds.maxX-bounds.minX, bounds.maxY-bounds.minY,
                bounds.maxZ-bounds.minZ, bounds.inverseExtent, EnemyModelWorldScale(model, entry.gameScale, 1));
        }
    }
    // Render the corrected models through the real, uniformly sized Movie cards.
    if (casualtyState.result.casualties.size() != 7) { return 1; }
    view.Begin(28);
    if (!DrawOriginalPostGame(view, casualtyState, toc, tables, profile)) { return 1; }
    for (unsigned page = 0; page < 3; ++page) {
        view.Begin(28);
        view.clock += 1000;
        if (!DrawOriginalPostGame(view, casualtyState, toc, tables, profile)) { return 1; }
        casualtyState.postGame.postGameGalleryPosition = static_cast<float>(page * 2);
        view.Begin(28);
        if (!DrawOriginalPostGame(view, casualtyState, toc, tables, profile) ||
            !GB_SAVE_FRAME(window, TestOutput::Path("enemy-scale-casualties-") + std::to_string(page) + ".png")) { return 1; }
    }
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_WRAPUP_SCREEN");
    unsigned idleStart = 0, idleEnd = 0;
    if (!view.movies.GetMovie(ordinal)->GetChapterRange(1, idleStart, idleEnd)) { return 1; }
    MovieRegion tabs, buttonBounds;
    const auto *button = OriginalMenuData("MDS_BUTTON_POSTGAME_INFO", 0);
    if (!view.movies.Region(ordinal, 1, idleStart, tabs) || !button ||
        !view.movies.Region(view.movies.Ordinal(button->movies[0]), 0, 0, buttonBounds)) { return 1; }
    const float firstX = tabs.x + static_cast<int>(tabs.width) / 2 - static_cast<int>((buttonBounds.width + 2) * 2) / 2;
    for (unsigned target = 0; target < 2; ++target) {
        MenuState state;
        state.page = 28 - target;
        MenuTransitionTrace trace;
        const std::vector<MenuTestClick> clicks{{-100, -100, 1}, {-100, -100, idleEnd + 1},
            {firstX + target * (buttonBounds.width + 2) + buttonBounds.width / 2, tabs.y + buttonBounds.height / 2, 1}};
        if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state, path,
            TestOutput::Path("postgame-tab-") + std::to_string(target) + ".png", &clicks, true, &window, true, &trace) != -2) { return 1; }
        if (state.page != 27 + target || trace.starts != 0 || trace.active) { ++failures; }
        std::printf("[postgame-presentation-check] tab=%u page=%u wipes=%u active=%d failures=%u\n",
            target, state.page, trace.starts, trace.active, failures);
    }
    return failures != 0;
}
