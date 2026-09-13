/** BRO-OPS regression with real templates, Movies and a disposable native save. */
#include "ui/MenuChecks.h"
#include "gun_bros_re/ui/SurvivalHud.h"
#include "TestOutput.h"

int CheckBroOps(GameMenu &view, CResTOCManager &toc, PackTables &tables, const CProfileManager &source) {
    CProfileManager profile = source;
    // Leave room for the actual consumable reward in this disposable profile.
    profile.powerups.clear();
    CChallengeManager manager;
    std::vector<WeaponEntry> weapons;
    std::vector<StoreEntry> store;
    if (!manager.Load(toc, tables) || !LoadWeaponCatalog(toc, tables, weapons) || !LoadStoreCatalog(toc, tables, store)) { return 1; }
    unsigned day = 25000;
    unsigned selected = 0;
    bool found = false;
    for (; day < 25500 && !found; ++day) {
        const auto list = manager.GenerateChallengeList(day);
        for (unsigned index = 0; index < list.size(); ++index) {
            const auto &entry = manager.templates[list[index]];
            if (entry.requiredKills > 1 && entry.circumstanceMask == 0 && entry.lastWave == 0 &&
                entry.perfectWaves == 0 && entry.levelIncreases == 0 && entry.friendIncreases == 0 && entry.powerups.empty()) {
                selected = index; found = true; break;
            }
        }
    }
    if (!found) { return 1; }
    --day;
    const unsigned seconds = day * 86400 - 36000;
    if (!manager.InitProgressData(toc, tables, profile, seconds - 1) || manager.cycleDay != day - 1 ||
        !manager.InitProgressData(toc, tables, profile, seconds) || manager.cycleDay != day) { return 1; }
    manager.current[selected].counters.kills = 1;
    manager.UpdateChallengeStatusData(profile, false);
    if (!manager.StoreProgress(profile)) { return 1; }
    const auto saved = profile.nativeArchive->records[17].payload;
    if (!manager.InitProgressData(toc, tables, profile, seconds - 100) || manager.cycleDay != day ||
        profile.nativeArchive->records[17].payload != saved) { return 1; }
    auto &challenge = manager.current[selected];
    const auto &entry = manager.templates[challenge.templateIndex];
    CChallengeManager::Session session;
    session.level = entry.level;
    if (session.level.IsNull()) { session.level = profile.nativeArchive->survivalLevels[0]; }
    if (entry.flags & 2) { session.gameType = 2; }
    session.guns = profile.configuration.guns;
    CChallengeManager::Kill kill;
    kill.count = entry.requiredKills;
    if (!entry.enemies.empty()) { kill.enemy = entry.enemies.front(); }
    bool bulletFound = entry.weapons.empty() && entry.gunCategoryMask == 0;
    for (const auto &ref : entry.weapons) {
        if (ref.type == 3) { kill.bullet = ref.object; bulletFound = true; break; }
    }
    if (!bulletFound) {
        for (const auto &weapon : weapons) {
            if (entry.gunCategoryMask && !(entry.gunCategoryMask & (1u << weapon.data.GetCategory()))) { continue; }
            bool allowed = entry.weapons.empty();
            for (const auto &ref : entry.weapons) {
                if (ref.type == 6 && ref.object.packHash == weapon.packHash && ref.object.localIndex == weapon.ordinal) { allowed = true; }
            }
            if (!allowed) { continue; }
            session.guns[0].packHash = weapon.packHash;
            session.guns[0].localIndex = static_cast<std::uint8_t>(weapon.ordinal);
            kill.bullet = weapon.data.GetBulletRef();
            bulletFound = true; break;
        }
    }
    if (!bulletFound) { return 1; }
    session.kills.push_back(kill);
    const unsigned previousKills = challenge.counters.kills;
    session.kills.front().player = false;
    manager.UpdateFromLevelSession(session, weapons, profile);
    if (challenge.counters.kills != previousKills) { return 1; }
    session.kills.front().player = true;
    if (!entry.level.IsNull()) {
        const auto requiredLevel = session.level;
        session.level = {};
        manager.UpdateFromLevelSession(session, weapons, profile);
        if (challenge.counters.kills != previousKills) { return 1; }
        session.level = requiredLevel;
    }
    if (entry.flags & 1) {
        manager.UpdateChallengeStatusData(profile, true);
        if (challenge.counters.kills != 0 || challenge.progress != 0) { return 1; }
    }
    manager.UpdateFromLevelSession(session, weapons, profile);
    if (challenge.progress != 100 || !manager.StoreProgress(profile)) { return 1; }
    session.kills.clear();
    session.ended = true;
    manager.UpdateFromLevelSession(session, weapons, profile);
    if (challenge.progress != 100) { return 1; }
    for (auto &value : manager.current) {
        const auto &definition = manager.templates[value.templateIndex];
        if (!definition.lastWave || !definition.perfectWaves || definition.requiredKills) { continue; }
        CChallengeManager::Session wave;
        wave.level = definition.level;
        wave.waveCleared = true;
        wave.perfect = true;
        wave.wave = definition.firstWave + 50;
        manager.UpdateFromLevelSession(wave, weapons, profile);
        if (value.counters.perfectWaves != 0) { return 1; }
        wave.wave = definition.firstWave;
        manager.UpdateFromLevelSession(wave, weapons, profile);
        const auto bits = value.counters.perfectWaves;
        manager.UpdateFromLevelSession(wave, weapons, profile);
        if (!bits || value.counters.perfectWaves != bits) { return 1; }
        wave.ended = true;
        wave.waveCleared = false;
        manager.UpdateFromLevelSession(wave, weapons, profile);
        if ((definition.flags & 1) && value.progress != 100 && value.counters.perfectWaves) { return 1; }
        break;
    }
    const CProfileManager completedProfile = profile;
    const std::string completedName = challenge.name;
    GameObjectRef rewardedPowerup;
    for (const auto &reward : challenge.prizes[0].storeItems) {
        for (const auto &item : store) {
            if (item.ref.packHash != reward.packHash || item.ref.localIndex != reward.localIndex) { continue; }
            for (const auto &object : item.data.objects) {
                if (object.type == 17) { rewardedPowerup = object.object; }
            }
        }
    }
    const auto beforeCoins = profile.coins, beforeBucks = profile.warbucks;
    unsigned expectedCoins = 0, expectedBucks = 0;
    for (const auto &value : manager.current) {
        if (value.progress != 100) { continue; }
        const auto &definition = manager.templates[value.templateIndex];
        for (unsigned tier = value.rewardStatus; tier < 3; ++tier) {
            if (definition.participationRequired[tier] > value.completedFriends) { break; }
            expectedCoins += value.prizes[tier].coins;
            expectedBucks += value.prizes[tier].warbucks;
        }
    }
    unsigned totalAwards = 0;
    for (unsigned iteration = 0; iteration <= manager.current.size(); ++iteration) {
        unsigned awarded = 0;
        if (!manager.AwardAvailableRewards(profile, store, awarded)) { return 1; }
        totalAwards += awarded;
    }
    if (!totalAwards || profile.coins != beforeCoins + expectedCoins || profile.warbucks != beforeBucks + expectedBucks) { return 1; }
    if (!rewardedPowerup.IsNull() && profile.GetPowerupCount(rewardedPowerup) == 0) { return 1; }
    const auto directory = std::filesystem::path(TestOutput::Path("bro-ops"));
    if (!profile.SaveToDisk(directory)) { return 1; }
    CProfileManager reloaded;
    if (!LoadNativeProfile(toc, tables, reloaded, directory) || !manager.Bind(toc, tables, reloaded, seconds)) { return 1; }
    unsigned duplicate = 0;
    if (!manager.AwardAvailableRewards(reloaded, store, duplicate) || duplicate || reloaded.coins != profile.coins || reloaded.warbucks != profile.warbucks) { return 1; }
    // The same real content is drawn in the held and automatic overlays.
    SurvivalHud hud;
    if (!hud.Init(toc, tables)) { return 1; }
    hud.SetChallenges(&manager);
    SurvivalHudState state;
    state.originalUi = true;
    state.health = state.maximumHealth = 1;
    MovieRegion button;
    if (!hud.FindActionRegion(state, SurvivalHudAction::BroOps, button)) { return 1; }
    MovieRegion flag;
    view.movies.SpriteBounds(0, 170, flag);
    std::printf("[bro-ops-geometry] button=%.0f,%.0f %.0fx%.0f flag=%.0f,%.0f %.0fx%.0f\n", button.x, button.y, button.width, button.height, flag.x, flag.y, flag.width, flag.height);
    if (button.x < 0 || button.x >= 1024 || button.y < 0 || button.y >= 768) { return 1; }
    const float x = button.x + button.width / 2, y = button.y + button.height / 2;
    hud.Pointer(state, x, y, false);
    if (hud.Pointer(state, x, y, true) != SurvivalHudAction::BroOps || !hud.IsChallengeHeld()) { return 1; }
    hud.Advance(250);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!hud.Draw(state) || hud.ChallengeRowsDrawn() != manager.current.size() ||
        !GB_SAVE_FRAME(view.window, (directory / "held.png").string())) { return 1; }
    hud.Pointer(state, x, y, false);
    if (hud.IsChallengeHeld()) { return 1; }
    hud.OnOriginalWaveClear(1, false, 0, false);
    hud.Advance(10000); // Finish only the first authored notice.
    if (hud.TakeInterstitialCompletion()) { return 1; }
    hud.Advance(250);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!hud.Draw(state) || hud.ChallengeRowsDrawn() != manager.current.size() ||
        !GB_SAVE_FRAME(view.window, (directory / "wave-update.png").string())) { return 1; }
    hud.Advance(10000);
    if (!hud.TakeInterstitialCompletion() || hud.HasInterstitial()) { return 1; }
    CProfileManager menuProfile = completedProfile;
    CRefinementManager::Template refinement;
    std::vector<ArmorEntry> armor;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadArmorCatalog(toc, tables, armor)) { return 1; }
    MenuState menu;
    menu.page = 5;
    std::vector<MenuTestClick> frames(4);
    for (auto &frame : frames) { frame.advanceMs = 500; }
    if (ShowGameMenu(toc, tables, menuProfile, menuProfile.nativeArchive->progression, refinement, store, weapons, armor,
        menu, directory / "menu-profile", (directory / "reward-prompt.png").string(), &frames, true, &view.window) != -2 ||
        menu.challengeRewardTitle.empty() || menu.challengeRewardBody.find(completedName) == std::string::npos ||
        menu.social.challenges.current[selected].rewardStatus == 0) { return 1; }
    if (!rewardedPowerup.IsNull() && menuProfile.GetPowerupCount(rewardedPowerup) == 0) { return 1; }
    GameHostSettings().isConnected = false;
    if (hud.FindActionRegion(state, SurvivalHudAction::BroOps, button)) { return 1; }
    GameHostSettings().isConnected = true;
    if (!manager.InitProgressData(toc, tables, profile, seconds + 86400) || manager.cycleDay != day + 1) { return 1; }
    for (const auto &value : manager.current) { if (value.progress || value.rewardStatus || value.counters.kills) { return 1; } }
    std::printf("[bro-ops-check] rollover=1 backwards-clock=1 kills=1 saved-rewards=1 duplicate=0 held=1 update=1 disconnect=1\n");
    return 0;
}
