#include "gun_bros_re/debug/Capture.h"
/** Real BIG Live presentation, local transport and independent friend persistence. */
#include "ui/MenuChecks.h"
#include "gun_bros_re/ui/hud/CInputPad.h"
#include "gun_bros_re/ui/menus/CMenuSplash.h"
#include "gun_bros_re/gameplay/multiplayer/ZLiveShopSession.h"
#include "TestOutput.h"
#include <fstream>

std::vector<std::uint8_t> ReadLiveListPixels(ZMenuSurface &view, ZMovieRegion region) {
    int width = 0, height = 0;
    view.window.GetDrawableSize(width, height);
    region.height = 768 - region.y;
    const int x = static_cast<int>(region.x * width / 1024);
    const int y = static_cast<int>((768 - region.y - region.height) * height / 768);
    const int cropWidth = static_cast<int>(region.width * width / 1024);
    const int cropHeight = static_cast<int>(region.height * height / 768);
    std::vector<std::uint8_t> pixels(cropWidth * cropHeight * 4);
    glReadPixels(x, y, cropWidth, cropHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return pixels;
}

int CheckLocalRoster(CResTOCManager &toc, ZPackTables &tables, CProfileManager &profile, ZMenuSurface &view) {
    const auto root = std::filesystem::path(TestOutput::Path("roster"));
    CProfileManager player = profile;
    ZLocalBotFriend migrated;
    if (!migrated.Load(toc, tables, root, &player)) { return 1; }
    migrated.profile.experience += 13;
    const auto migrationXP = migrated.profile.experience;
    if (!migrated.Save()) { return 1; }
    ZLocalBotRoster roster;
    if (!roster.Load(toc, tables, root, player) || roster.At(0)->profile.experience != migrationXP) { return 1; }
    const auto config = root / "local-bots.cfg";
    std::ifstream input(config);
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();
    const auto bodyStart = text.find(']');
    if (bodyStart == std::string::npos) { return 1; }
    const auto body = text.substr(bodyStart + 1);
    {
        std::ofstream output(config);
        for (unsigned index = 0; index < 10; ++index) {
            std::string values = body;
            const auto name = values.find("name=LOCAL BOT");
            if (name == std::string::npos) { return 1; }
            values.replace(name, 14, "name=TEST BRO " + std::to_string(index + 1));
            output << "[test-bro-" << index + 1 << "]" << values;
        }
    }
    if (!roster.Load(toc, tables, root, player) || roster.Count() != 10 || player.friendCount != 10 ||
        !roster.Select(7) || roster.At(6)->name != "TEST BRO 7") { return 1; }
    roster.At(6)->profile.coins = 321;
    roster.At(6)->profile.experience += 13;
    const auto savedXP = roster.At(6)->profile.experience;
    if (!roster.At(6)->Save()) { return 1; }
    for (unsigned match = 0; match < 12; ++match) {
        if (roster.MatchSelected() != roster.At(6)) { return 1; }
    }
    ZLocalBotRoster reloaded;
    if (!reloaded.Load(toc, tables, root, player) || reloaded.Selected() != 7 ||
        reloaded.MatchSelected() != reloaded.At(6) || reloaded.At(6)->profile.coins != 321 ||
        reloaded.At(6)->profile.experience != savedXP || profile.friendCount != 0 ||
        CFriendPowerManager::Bonus(player.friendCount, 1) != 15 || player.refinery.friendEfficiencyBonus != 10) { return 1; }
    if (!reloaded.Select(3) || reloaded.MatchSelected() != reloaded.At(2) ||
        !reloaded.Select(0) || reloaded.MatchSelected() != reloaded.At(0) ||
        !reloaded.Select(7)) { return 1; }
    std::printf("[live-regression] active-bot-match=1 repeat=12 reload=1 selection-change=1 default=1\n");
    CMenuSystem friends;
    friends.stack.page = 4;
    friends.botRoster = &reloaded;
    friends.botFriend = reloaded.At(6);
    friends.social.selectedLocalFriend = 7;
    const bool wasConnected = GameHostSettings().isConnected;
    GameHostSettings().isConnected = true;
    view.Begin();
    if (!FinishMenuFrame(friends.social.Draw(view, friends, player, false), friends) || !friends.social.onlinePage ||
        !Capture::SaveFrame(view.window, TestOutput::Path("local-bot-roster.png"))) { return 1; }
    friends.social.socialTab = 1;
    view.Begin();
    if (!FinishMenuFrame(friends.social.Draw(view, friends, player, false), friends) ||
        !Capture::SaveFrame(view.window, TestOutput::Path("local-bot-boost.png"))) { return 1; }
    GameHostSettings().isConnected = wasConnected;
    friends.online.SetConnected(true);
    GameHostSettings().isConnected = false;
    UpdateLocalConnection(friends);
    const bool defaultRestored = friends.social.selectedLocalFriend == 0 && friends.botFriend == nullptr;
    GameHostSettings().isConnected = wasConnected;
    std::printf("[live-regression] disconnect-default=%d\n", defaultRestored);
    if (!defaultRestored) { return 1; }
    CProfileManager ordinary = profile;
    CProfileManager boosted = player;
    CDailyBonusTracking daily;
    std::vector<ZStoreEntry> store;
    if (!daily.Load(toc, tables) || !LoadStoreCatalog(toc, tables, store)) { return 1; }
    const auto day = ordinary.dailyLastLaunchSeconds + 172800u;
    const auto prizeIndex = daily.CalculateBonus(ordinary, day);
    if (!daily.CommitBonus(ordinary, day, store) || !daily.CommitBonus(boosted, day, store)) { return 1; }
    if (boosted.coins - ordinary.coins != daily.prizes[prizeIndex].coins * 20u / 100u ||
        boosted.warbucks != ordinary.warbucks || boosted.experience != ordinary.experience) { return 1; }
    CRefinementManager::Template refineryData;
    if (!LoadRefinementTemplate(toc, tables, refineryData)) { return 1; }
    ordinary.refinery.Bind(refineryData);
    boosted.refinery.Bind(refineryData);
    ordinary.refinery.slots[0].state = 1;
    boosted.refinery.slots[0].state = 1;
    ordinary.xplodium = boosted.xplodium = 100;
    if (!ordinary.refinery.BeginRefinement(0, 0, 100, ordinary.xplodium, day) ||
        !boosted.refinery.BeginRefinement(0, 0, 100, boosted.xplodium, day) ||
        boosted.refinery.GetRefinementSlotYield(0) != ordinary.refinery.GetRefinementSlotYield(0) + 10) { return 1; }
    // Four entries including the default reproduce the real reported roster.
    {
        std::ofstream output(config);
        for (unsigned index = 0; index < 3; ++index) { output << "[test-bro-" << index + 1 << "]" << body; }
    }
    ZLocalBotRoster compactRoster;
    if (!compactRoster.Load(toc, tables, root, player)) { return 1; }
    CMenuSystem compact;
    compact.stack.page = 4;
    compact.botRoster = &compactRoster;
    GameHostSettings().isConnected = true;
    view.Begin();
    if (!FinishMenuFrame(compact.social.Draw(view, compact, player, false), compact)) { return 1; }
    ZMovieRegion viewport;
    if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_BROBUFF_MENU"), 4, compact.social.socialTime, viewport)) { return 1; }
    const auto dim = ReadLiveListPixels(view, viewport);
    view.clock += 200;
    view.Begin();
    if (!FinishMenuFrame(compact.social.Draw(view, compact, player, false), compact)) { return 1; }
    const bool glow = dim != ReadLiveListPixels(view, viewport);
    std::printf("[live-regression] selected-bro-glow=%d default-name=%s\n", glow, compact.social.brotherName.c_str());
    if (!glow || compact.social.brotherName != view.movies.NamedString("IDS_FRIEND_DEFAULT_BRO2") ||
        !Capture::SaveFrame(view.window, TestOutput::Path("bros-selected-glow.png"))) { return 1; }
    view.Begin();
    view.clock += 16;
    view.InjectTap({viewport.x + 10, viewport.y + 10});
    view.pointerPressed = view.pointerHeld = true;
    view.dragY = -300;
    const bool drawn = FinishMenuFrame(compact.social.Draw(view, compact, player, false), compact);
    const bool scrolled = drawn && compact.social.scrollPosition > 0;
    std::printf("[live-regression] four-entry-bros-scroll=%d position=%.1f\n", scrolled, compact.social.scrollPosition);
    view.pointerPressed = view.pointerHeld = false;
    view.dragY = 0;
    view.ExchangeClick(false);
    compact.social.scrollMotion = ZMenuScrollMotion{};
    unsigned rowStart = 0, rowEnd = 0;
    const unsigned rows = view.movies.Ordinal("GLU_MOVIE_BROTHER_MENU_SCROLL_3_OPTION");
    if (!view.movies.GetMovie(rows)->GetChapterRange(1, rowStart, rowEnd)) { return 1; }
    const auto rowRegions = view.movies.Regions(rows, rowStart, viewport.x, viewport.y);
    ZMovieRegion lastCard;
    if (rowRegions.size() < 4) { return 1; }
    const auto &lastRow = rowRegions[3];
    const unsigned cardMovie = view.movies.Ordinal("GLU_MOVIE_BROTHER_BOX");
    unsigned cardStart = 0, cardEnd = 0;
    if (!view.movies.GetMovie(cardMovie)->GetChapterRange(0, cardStart, cardEnd) ||
        !view.movies.Region(cardMovie, 0, cardEnd, lastCard)) { return 1; }
    view.Begin();
    view.InjectTap({lastRow.x + lastCard.x - 512 + lastCard.width / 2, lastRow.y + lastCard.y - 384 + lastCard.height / 2});
    if (!FinishMenuFrame(compact.social.Draw(view, compact, player, false), compact) || compact.social.selectedLocalFriend != 3) { return 1; }
    view.clock += 200;
    view.Begin();
    if (!FinishMenuFrame(compact.social.Draw(view, compact, player, false), compact) ||
        !Capture::SaveFrame(view.window, TestOutput::Path("bros-last-selected.png"))) { return 1; }
    GameHostSettings().isConnected = wasConnected;
    if (!scrolled) { return 1; }
    std::printf("[bot-roster-check] count=10 selection-reload=1 match-cycle=1 progress-preserved=1 boost-tiers=1\n");
    return 0;
}

int CheckLiveMode(CResTOCManager &toc, ZPackTables &tables, CProfileManager &profile) {
    ZLiveShopSession shop;
    if (!shop.Request(1, 100) || shop.Request(0, 200) || shop.Visible(849) || !shop.Visible(850) ||
        shop.Remaining(850) != 10000 || shop.Close(0)) { return 1; }
    shop.Update(10849);
    if (!shop.Active() || shop.Remaining(10849) != 1) { return 1; }
    shop.Update(10850);
    if (shop.Active() || !shop.Request(0, 11000) || !shop.Close(0)) { return 1; }
    const auto root = std::filesystem::path(TestOutput::Path("live-friend"));
    ZLocalBotFriend friendData;
    if (!friendData.Load(toc, tables, root, &profile) || !friendData.Select(true)) { return 1; }
    const auto originalCoins = profile.coins;
    friendData.profile.coins = 321;
    if (!friendData.Save()) { return 1; }
    ZLocalBotFriend reloaded;
    if (!reloaded.Load(toc, tables, root, &profile) || !reloaded.selected || reloaded.profile.coins != 321 ||
        profile.coins != originalCoins) { return 1; }
    ZMenuSurface view;
    if (!view.Open(toc, tables, &profile)) { return 1; }
    view.scripted = true;
    view.animateNavigation = false;
    if (CheckLocalRoster(toc, tables, profile, view) != 0) { return 1; }
    CKeysetResource wallpapers;
    if (!wallpapers.Load(view.movies.CorePack(), "KEYSET_SPLASH_IMAGES")) { return 1; }
    for (unsigned index = 0; index < 5; ++index) {
        CMenuSplash splash;
        if (!splash.Init(view.movies, index, &profile, ZLoadingMode::Live)) { return 1; }
        if (splash.ImageHandle() != wallpapers.handles[wallpapers.handles.size() - 5 + index % 4]) { return 1; }
        view.Begin();
        if (!splash.Draw(splash.IdleStart()) || !Capture::SaveFrame(view.window,
            TestOutput::Path("live-loading-" + std::to_string(index) + ".png"))) { return 1; }
    }
    CMenuSplash deathmatchSplash;
    if (!deathmatchSplash.Init(view.movies, 2, &profile, ZLoadingMode::Deathmatch) ||
        deathmatchSplash.ImageHandle() != wallpapers.handles.back()) { return 1; }
    view.Begin();
    if (!deathmatchSplash.Draw(deathmatchSplash.IdleStart()) ||
        !Capture::SaveFrame(view.window, TestOutput::Path("deathmatch-loading.png"))) { return 1; }
    CInputPad hud;
    if (!hud.Init(toc, tables)) { return 1; }
    if (hud.DefaultBrotherName(1) != "Francis Gun" || hud.DefaultBrotherName(0) != "Percy Gun") { return 1; }
    ZInputPadState hudState;
    hudState.localLive = true;
    hudState.withBrother = true;
    hudState.health = hudState.maximumHealth = 100;
    hudState.brotherHealth = hudState.brotherMaximumHealth = 100;
    ZMultiplayerStatistics player, peer;
    player.wave = {12, 3, 1, 0, 9, 24, 120};
    peer.wave = {7, 5, 0, 1, 4, 11, 70};
    player.total = player.wave; peer.total = peer.wave;
    hud.BeginLiveWave(player, peer);
    for (unsigned elapsed = 0; elapsed < 3000; elapsed += 16) { hud.Advance(16); }
    view.Begin();
    if (!hud.Draw(hudState) || !Capture::SaveFrame(view.window, TestOutput::Path("live-wave-results.png"))) { return 1; }
    while (hud.LiveWaveRemaining() > 6000) { hud.Advance(1); }
    view.Begin();
    if (!hud.Draw(hudState) || !Capture::SaveFrame(view.window, TestOutput::Path("live-wave-countdown.png"))) { return 1; }
    hudState.indicators.push_back({-1, Collision::Brother, 4, 512, -100, 500, -1});
    view.Begin();
    if (!hud.Draw(hudState) || !Capture::SaveFrame(view.window, TestOutput::Path("live-peer-indicator.png"))) { return 1; }
    hudState.indicators.clear();
    while (hud.LiveWaveRemaining() > 1) { hud.Advance(1); }
    if (!hud.HasInterstitial() || hud.TakeInterstitialCompletion()) { return 1; }
    hud.Advance(1);
    if (hud.HasInterstitial() || !hud.TakeInterstitialCompletion()) { return 1; }
    hudState.shopOpen = true;
    hudState.remoteShop = true;
    hudState.shopRemainingMs = 7000;
    hudState.brotherName = "LOCAL BOT";
    hudState.inventory = reloaded.profile.powerups;
    hudState.coins = reloaded.profile.coins;
    view.Begin();
    if (!hud.Draw(hudState)) { return 1; }
    hud.AdvanceMenu(3000);
    hud.BrowseRemoteShop(3);
    view.Begin();
    if (!hud.Draw(hudState) || !Capture::SaveFrame(view.window, TestOutput::Path("live-peer-shop.png"))) { return 1; }
    for (unsigned y = 0; y < 768; y += 32) {
        for (unsigned x = 0; x < 1024; x += 32) {
            if (hud.Pointer(hudState, float(x), float(y), true) != ZInputPadAction::None) { return 1; }
            hud.Pointer(hudState, float(x), float(y), false);
        }
    }
    CMenuSystem results;
    results.online.SetConnected(true);
    results.stack.page = 27;
    results.result.live = true;
    results.result.peers[0] = player.total;
    results.result.peers[1] = peer.total;
    results.result.wavesPerRevolution = 50;
    results.result.waveLimit = 500;
    results.result.waves = 1;
    for (unsigned frame = 0; frame < 5; ++frame) {
        view.clock += 1000;
        view.Begin();
        if (!FinishMenuFrame(results.postGame.Draw(view, results, toc, tables, profile), results)) { return 1; }
    }
    if (!Capture::SaveFrame(view.window, TestOutput::Path("live-final-results.png"))) { return 1; }
    for (const auto &region : view.movies.Regions(view.movies.Ordinal("GLU_MOVIE_WRAPUP_SCREEN_MP"), results.postGame.postGameTime)) {
        std::printf("[live-layout-check] region=%u x=%.1f y=%.1f width=%.1f height=%.1f\n", region.index, region.x, region.y, region.width, region.height);
    }
    results.postGame.livePosition = 3;
    view.Begin();
    if (!FinishMenuFrame(results.postGame.Draw(view, results, toc, tables, profile), results) ||
        !Capture::SaveFrame(view.window, TestOutput::Path("live-final-results-bottom.png"))) { return 1; }
    ZMovieRegion replay;
    if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_WRAPUP_SCREEN_MP"), 7, results.postGame.postGameTime, replay)) { return 1; }
    view.Begin();
    view.InjectTap({replay.x + replay.width / 2, replay.y + replay.height / 2});
    if (!FinishMenuFrame(results.postGame.Draw(view, results, toc, tables, profile), results) || !results.postGame.liveReplay) { return 1; }
    std::printf("[live-mode-check] shop-owner-delay-timeout=1 friend-reload=1 wave-timer=1 remote-readonly=1 final-results=1\n");

    // Enter the real menu loop with its popup clock and launch return value.
    const bool connected = GameHostSettings().isConnected;
    GameHostSettings().isConnected = true;
    CMenuSystem match;
    match.gameMode = 1;
    match.stack.page = 0;
    if (!BeginLocalMatch(match)) { return 1; }
    CRefinementManager::Template refinement;
    std::vector<ZStoreEntry> store;
    std::vector<ZWeaponEntry> weapons;
    std::vector<ZArmorEntry> armor;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadStoreCatalog(toc, tables, store) ||
        !LoadWeaponCatalog(toc, tables, weapons) || !LoadArmorCatalog(toc, tables, armor)) { return 1; }
    std::vector<ZMenuInputFrame> waits(12, {-100, -100, 500});
    const int launch = ShowGameMenu(toc, tables, profile, profile.nativeArchive->progression, refinement,
        store, weapons, armor, match, root, TestOutput::Path("live-matching-failure.png"), &waits, false, &view.window);
    GameHostSettings().isConnected = connected;
    if (launch != 0 || match.matchingPrompt || match.online.IsMatching()) {
        std::printf("[live-mode-check] real menu matching failed result=%d\n", launch); return 1;
    }
    std::printf("[live-mode-check] real-menu-match=1\n");
    return 0;
}
