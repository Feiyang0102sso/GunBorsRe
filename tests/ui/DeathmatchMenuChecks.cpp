#include "gun_bros_re/debug/Capture.h"
/** Exercise local matchmaking, original match HUD, results and replay input. */
#include "ui/MenuChecks.h"
#include "gun_bros_re/ui/hud/CInputPad.h"
#include "gun_bros_re/gameplay/multiplayer/CMPMatch.h"
#include "TestOutput.h"

int CheckDeathmatchMenus(CResTOCManager &toc, ZPackTables &tables, CProfileManager &profile) {
    ZMenuSurface view;
    if (!view.Open(toc, tables, &profile)) { return 1; }
    view.scripted = true; view.animateNavigation = false;
    const bool wasConnected = GameHostSettings().isConnected;
    GameHostSettings().isConnected = true;
    CMenuSystem match;
    view.Begin();
    if (!match.mode.Draw(view, match)) { return 1; }
    ZMovieRegion mode;
    if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_MULTIPLAYER_AND_VERSUS_MAP"), 5, match.mode.modeTime, mode)) { return 1; }
    view.Begin();
    view.InjectTap({mode.x + mode.width / 2, mode.y + mode.height / 2});
    if (!match.mode.Draw(view, match) || match.gameMode != 2) { return 1; }
    if (!BeginLocalMatch(match)) { return 1; }
    CRefinementManager::Template refinement;
    std::vector<ZStoreEntry> store;
    std::vector<ZWeaponEntry> weapons;
    std::vector<ZArmorEntry> armor;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadStoreCatalog(toc, tables, store) ||
        !LoadWeaponCatalog(toc, tables, weapons) || !LoadArmorCatalog(toc, tables, armor)) { return 1; }
    std::vector<ZMenuInputFrame> waits(12, {-100, -100, 500});
    const int launch = ShowGameMenu(toc, tables, profile, profile.nativeArchive->progression, refinement,
        store, weapons, armor, match, TestOutput::Path("deathmatch-profile"),
        TestOutput::Path("deathmatch-matching.png"), &waits, false, &view.window);
    if (launch != 0 || match.gameMode != 2 || match.matchingPrompt || match.online.IsMatching()) { return 1; }

    CInputPad hud;
    if (!hud.Init(toc, tables)) { return 1; }
    std::vector<CMPMatch::Entry> templates;
    if (!LoadMPMatches(toc, tables, templates) || !hud.ConfigureDeathmatch(templates[0].data.stores)) { return 1; }
    ZInputPadState state;
    state.deathmatch = true; state.withBrother = true;
    state.health = 0; state.maximumHealth = 120; state.dead = true;
    state.matchScore[0] = 1; state.matchScore[1] = 2; state.matchLimit = 3; state.respawnMs = 8000;
    state.brotherName = "LOCAL BOT";
    state.guns[0] = templates[0].guns[0]; state.guns[1] = templates[0].guns[1];
    hud.BeginDeathmatch(templates[0].data.killLimit);
    const auto *intro = view.movies.GetMovie(view.movies.Ordinal("GLU_MOVIE_WAVE_CLEARED"));
    if (intro == nullptr || hud.NoticeCount() != 1 || hud.TakeDeathmatchIntroCompletion() ||
        hud.m_notices.front().title.find(std::to_string(templates[0].data.killLimit)) == std::string::npos) { return 1; }
    hud.Advance(intro->duration / 2);
    view.Begin();
    if (!hud.Draw(state) || !Capture::SaveFrame(view.window, TestOutput::Path("deathmatch-intro.png"))) { return 1; }
    hud.Advance(intro->duration - intro->duration / 2 - 1);
    if (hud.TakeDeathmatchIntroCompletion()) { return 1; }
    hud.Advance(1);
    if (!hud.TakeDeathmatchIntroCompletion() || hud.TakeDeathmatchIntroCompletion()) { return 1; }
    hud.ResetNotices();
    view.Begin();
    if (!hud.Draw(state) || !Capture::SaveFrame(view.window, TestOutput::Path("deathmatch-respawn.png"))) { return 1; }
    ZMovieRegion pause;
    if (!hud.FindActionRegion(state, ZInputPadAction::Pause, pause)) { return 1; }
    hud.Pointer(state, pause.x + pause.width / 2, pause.y + pause.height / 2, false);
    if (hud.Pointer(state, pause.x + pause.width / 2, pause.y + pause.height / 2, true) != ZInputPadAction::Pause) { return 1; }
    state.paused = true;
    view.Begin();
    if (!hud.Draw(state)) { return 1; }
    state.paused = false; state.dead = false; state.health = state.maximumHealth;
    state.shopOpen = true; state.remoteShop = false; state.shopRemainingMs = 8000;
    state.inventory = profile.powerups;
    // The intro passes mode 1; later shopping passes mode 0 (Show :186527).
    hud.ResetSelector(true);
    view.Begin();
    if (!hud.Draw(state)) { return 1; }
    hud.AdvanceMenu(3000);
    view.Begin();
    ZMovieRegion initialGun;
    if (!hud.Draw(state) || !hud.FindActionRegion(state, ZInputPadAction::SelectMatchGun, initialGun)) {
        std::printf("[dm-entry-selector] initial shop did not open GUNS\n");
        return 1;
    }
    if (!Capture::SaveFrame(view.window, TestOutput::Path("deathmatch-initial-guns.png"))) { return 1; }
    hud.ResetSelector();
    view.Begin();
    if (!hud.Draw(state)) { return 1; }
    hud.AdvanceMenu(3000);
    view.Begin();
    if (!hud.Draw(state) || !Capture::SaveFrame(view.window, TestOutput::Path("deathmatch-shop.png"))) { return 1; }
    ZMovieRegion powerupTab, gunTab, powerupGraphic, gunGraphic, powerupTouch, gunTouch;
    const auto *powerupButton = CMenuDataProvider::Find("MDS_BUTTON_POWERUP_SELECTOR", 4);
    const auto *gunButton = CMenuDataProvider::Find("MDS_BUTTON_POWERUP_SELECTOR", 5);
    if (!hud.FindActionRegion(state, ZInputPadAction::ShowPowerups, powerupTab) ||
        !hud.FindActionRegion(state, ZInputPadAction::ShowGuns, gunTab) ||
        !view.movies.Region(view.movies.Ordinal(powerupButton->movies[0]), 1, 0, powerupGraphic) ||
        !view.movies.Region(view.movies.Ordinal(gunButton->movies[0]), 1, 0, gunGraphic) ||
        !view.movies.Region(view.movies.Ordinal(powerupButton->movies[0]), 0, 0, powerupTouch) ||
        !view.movies.Region(view.movies.Ordinal(gunButton->movies[0]), 0, 0, gunTouch)) { return 1; }
    // Compare actual rendered artwork edges, not the larger input rectangles.
    const float powerupRight = powerupTab.x + powerupGraphic.x - powerupTouch.x + powerupGraphic.width;
    const float gunLeft = gunTab.x + gunGraphic.x - gunTouch.x;
    const float tabGap = gunLeft - powerupRight;
    const bool defaultPowerups = !hud.FindActionRegion(state, ZInputPadAction::SelectMatchGun, gunGraphic);
    std::printf("[dm-presentation] shop-default-powerups=%d artwork-gap=%.1f expected=4\n", defaultPowerups, tabGap);
    if (!defaultPowerups || std::abs(tabGap - 4) > 0.01f) { return 1; }
    hud.Pointer(state, gunTab.x + gunTab.width / 2, gunTab.y + gunTab.height / 2, false);
    hud.Pointer(state, gunTab.x + gunTab.width / 2, gunTab.y + gunTab.height / 2, true);
    view.Begin();
    if (!hud.Draw(state) || !Capture::SaveFrame(view.window, TestOutput::Path("deathmatch-guns.png"))) { return 1; }
    ZMovieRegion gunCard;
    if (!hud.FindActionRegion(state, ZInputPadAction::SelectMatchGun, gunCard)) { return 1; }
    hud.Pointer(state, gunCard.x + gunCard.width / 2, gunCard.y + gunCard.height / 2, false);
    hud.Pointer(state, gunCard.x + gunCard.width / 2, gunCard.y + gunCard.height / 2, true);
    if (hud.Pointer(state, gunCard.x + gunCard.width / 2, gunCard.y + gunCard.height / 2, false) != ZInputPadAction::SelectMatchGun) { return 1; }
    hud.AdvanceMatchSelection();
    hud.AdvanceMenu(250);
    view.Begin();
    if (!hud.Draw(state) || !Capture::SaveFrame(view.window, TestOutput::Path("deathmatch-gun-selected.png"))) { return 1; }
    // A death may replace an already-open gun shop without a hidden render frame.
    hud.ResetSelector();
    view.Begin();
    if (!hud.Draw(state) || hud.m_selector.m_matchGuns) { return 1; }
    std::vector<ZPowerupEntry> powerups;
    if (!LoadPowerupCatalog(toc, tables, powerups)) { return 1; }
    bool cooldownDrawn = false;
    for (const auto &entry : powerups) {
        std::printf("[dm-cooldown-data] powerup=%u name=%s seconds=%u\n", entry.resource.localIndex, entry.name.c_str(), entry.data.field124);
        if (entry.resource.localIndex != 13 || entry.data.field124 == 0) { continue; }
        state.shopOpen = false;
        state.rightPowerup = entry.resource; state.rightCount = 2;
        state.powerupCooldowns[entry.resource.localIndex] = entry.data.field124 * 500;
        view.Begin();
        if (!hud.Draw(state) || !Capture::SaveFrame(view.window, TestOutput::Path("deathmatch-cooldown.png"))) { return 1; }
        cooldownDrawn = true;
    }
    if (!cooldownDrawn) { return 1; }
    std::string grenadeName;
    for (const auto &entry : powerups) {
        if (entry.resource.localIndex == 13) { grenadeName = entry.name; }
    }
    hud.OnDeathmatchPowerup(grenadeName);
    if (grenadeName.empty() || hud.m_matchMessages.empty() || hud.m_matchMessages.back().text.find(grenadeName) == std::string::npos ||
        hud.m_matchMessages.back().text.find('%') != std::string::npos) { return 1; }
    view.Begin();
    if (!hud.Draw(state) || !Capture::SaveFrame(view.window, TestOutput::Path("deathmatch-powerup-message.png"))) { return 1; }
    // Reopening the same selector must discard its previous GUNS tab.
    state.shopOpen = true;
    view.Begin();
    if (!hud.Draw(state) || hud.m_selector.m_matchGuns) { return 1; }
    state.shopOpen = false;
    const auto *ending = view.movies.GetMovie(view.movies.Ordinal("GLU_MOVIE_MISSION_END"));
    if (ending == nullptr) { return 1; }
    hud.AdvanceDeathmatchWrapUp(0);
    if (hud.IsDeathmatchWrapUpComplete()) { return 1; }
    hud.AdvanceDeathmatchWrapUp(ending->duration / 2);
    view.Begin();
    // Draw against a nonblack field so the resource's alpha is observable.
    view.movies.Rectangle(0, 0, 1024, 768, 0.4f, 0.6f, 0.8f, 1);
    if (!hud.Draw(state) || !Capture::SaveFrame(view.window, TestOutput::Path("deathmatch-fade.png"))) { return 1; }
    hud.AdvanceDeathmatchWrapUp(ending->duration - ending->duration / 2 - 1);
    if (hud.IsDeathmatchWrapUpComplete()) { return 1; }
    hud.AdvanceDeathmatchWrapUp(1);
    if (!hud.IsDeathmatchWrapUpComplete()) { return 1; }
    std::printf("[dm-presentation] intro=%u ms mission-end=%u ms completed=1\n", intro->duration, ending->duration);
    CGameFlow context{profile, {}};
    context.persistProgress = false;
    context.result.live = true; context.result.deathmatch = true;
    context.result.matchResult = static_cast<unsigned>(CMPMatch::Result::PlayerWon);
    context.result.matchKillLimit = templates[0].data.killLimit;
    context.result.peers[0].kills = 3; context.result.peers[0].experience = 30;
    context.result.peers[0].bestStreak = 2;
    context.result.peers[1].deaths = 3;
    CMenuSystem results;
    results.postGame.Refresh(results, context, weapons);
            results.UpdateNavigation();
    if (results.stack.page != 27 || results.postGame.postGameUpgradePending) { return 1; }
    for (unsigned frame = 0; frame < 300; ++frame) {
        view.clock += 16;
        view.Begin();
        if (!FinishMenuFrame(results.postGame.Draw(view, results, toc, tables, profile), results)) { return 1; }
    }
    if (!Capture::SaveFrame(view.window, TestOutput::Path("deathmatch-results.png"))) { return 1; }
    results.postGame.livePosition = 4;
    view.Begin();
    if (!FinishMenuFrame(results.postGame.Draw(view, results, toc, tables, profile), results) || results.postGame.livePosition != 3) { return 1; }
    ZMovieRegion replay;
    if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_WRAPUP_SCREEN_MP"), 7, results.postGame.postGameTime, replay)) { return 1; }
    view.Begin();
    view.InjectTap({replay.x + replay.width / 2, replay.y + replay.height / 2});
    results.online.SetConnected(false);
    if (!FinishMenuFrame(results.postGame.Draw(view, results, toc, tables, profile), results) || results.postGame.liveReplay) { return 1; }
    results.online.SetConnected(true);
    view.Begin();
    view.InjectTap({replay.x + replay.width / 2, replay.y + replay.height / 2});
    if (!FinishMenuFrame(results.postGame.Draw(view, results, toc, tables, profile), results) || !results.postGame.liveReplay) { return 1; }
    GameHostSettings().isConnected = wasConnected;
    std::printf("[deathmatch-ui] connected-match=1 respawn-pause=1 gun-selector=1 results=1 replay=1 offline-replay-blocked=1\n");
    return 0;
}
