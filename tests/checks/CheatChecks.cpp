/** Verify desktop cheats through the menu, combat and native save consumers. */
#include "gun_bros_re/ui/ZMenuInternal.h"
#include "gun_bros_re/cheats/CheatCodes.h"
#include "gun_bros_re/data/ZProfileStorage.h"
#include "gun_bros_re/gameplay/CGame.h"
#include "gun_bros_re/ui/CPowerUpSelector.h"
#include "TestOutput.h"
#include <SDL3/SDL.h>

unsigned CheckCheatActions(CResTOCManager &toc, ZPackTables &tables,
    const CPlayerProgress::Template &data) {
    unsigned failures = 0;
    CProfileManager profile;
    if (!CreateTransientProfile(toc, tables, profile)) { return 1; }
    ZWindow window;
    if (!window.Open("Cheat action checks", 320, 240)) { return 1; }
    GameCheats::Bind();
    window.EnableCheats(true);
    CPlayerProgress progress;
    progress.Bind(data);
    MenuDetail::ZMenuState menu;
    CDailyBonusTracking daily;
    const auto savePath = TestOutput::Path("cheat-profile");
    const auto initialCoins = profile.coins;
    const auto initialWarbucks = profile.warbucks;
    const auto initialXplodium = profile.xplodium;
    if (!menu.social.challenges.InitProgressData(toc, tables, profile, static_cast<unsigned>(MenuDetail::CurrentSeconds()))) { return 1; }
    const unsigned initialChallengeDay = menu.social.challenges.cycleDay;
    menu.social.challenges.current.front().counters.kills = 1;
    menu.social.challenges.current.front().progress = 100;
    menu.social.challenges.current.front().rewardStatus = 1;
    if (!menu.social.challenges.StoreProgress(profile)) { return 1; }
    const auto initialDailyLaunch = profile.dailyLastLaunchSeconds;
    const auto initialDailySeconds = profile.dailyConsecutiveSeconds;
    // The same SDL input and menu consumer used by the game also save each action.
    for (const char *command : {GameCheats::Money, GameCheats::LevelUp, GameCheats::UnlockWaves,
        GameCheats::UpdateChallenges, GameCheats::UpdateChallenges, GameCheats::Xplodium,
        GameCheats::ToggleRefineryLocks, GameCheats::AdvanceRefinery}) {
        for (const char *letter = command; *letter != '\0'; ++letter) {
            SDL_Event event{};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.key = *letter;
            SDL_PushEvent(&event);
            event.type = SDL_EVENT_KEY_UP;
            SDL_PushEvent(&event);
        }
        if (!window.PumpEvents() || !ProcessMenuCheats(window, profile, menu, daily, savePath, data, progress)) { return 1; }
    }
    if (profile.coins != initialCoins + 500000 || profile.warbucks != initialWarbucks + 500 || progress.GetLevel() != 2) { ++failures; }
    for (unsigned cleared : profile.clearedWaves) { if (cleared != 500) { ++failures; } }
    if (!ReloadProfile(profile, savePath) || profile.experience != progress.GetExperience()) { ++failures; }
    if (profile.xplodium != initialXplodium + GameCheats::XplodiumAmount) { ++failures; }
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    for (unsigned slot = 0; slot < refinement.minutes.size(); ++slot) {
        if (profile.refinery.slots[slot].state != 1) { ++failures; }
    }
    // Lock during an uncommitted paid-chamber transfer, then unlock again.
    // Only the lock cancels that transfer; the UI consumes this cancellation flag.
    unsigned paidSlot = 0;
    while (paidSlot < refinement.minutes.size() && !refinement.commonPrice[paidSlot] && !refinement.rarePrice[paidSlot]) { ++paidSlot; }
    if (paidSlot == refinement.minutes.size()) { return 1; }
    for (unsigned toggle = 0; toggle < 2; ++toggle) {
        menu.refinery.refineryTransfer = static_cast<int>(paidSlot);
        menu.refinery.refineryCancelTransfer = false;
        for (const char *letter = GameCheats::ToggleRefineryLocks; *letter; ++letter) {
            SDL_Event event{};
            event.type = SDL_EVENT_KEY_DOWN;
            event.key.key = *letter;
            SDL_PushEvent(&event);
            event.type = SDL_EVENT_KEY_UP;
            SDL_PushEvent(&event);
        }
        if (!window.PumpEvents() || !ProcessMenuCheats(window, profile, menu, daily, savePath, data, progress) ||
            !ReloadProfile(profile, savePath)) { return 1; }
        if (menu.refinery.refineryCancelTransfer != (toggle == 0) || profile.xplodium != initialXplodium + GameCheats::XplodiumAmount) { ++failures; }
    }
    menu.refinery.refineryTransfer = -1;
    CChallengeManager restoredChallenges;
    if (!restoredChallenges.Bind(toc, tables, profile, 0)) { return 1; }
    if (restoredChallenges.cycleDay != initialChallengeDay + 2 || menu.social.contentBound ||
        profile.dailyLastLaunchSeconds != initialDailyLaunch || profile.dailyConsecutiveSeconds != initialDailySeconds) { ++failures; }
    const auto expectedList = restoredChallenges.GenerateChallengeList(initialChallengeDay + 2);
    for (unsigned index = 0; index < restoredChallenges.current.size(); ++index) {
        const auto &challenge = restoredChallenges.current[index];
        if (challenge.templateIndex != expectedList[index] || challenge.progress || challenge.rewardStatus || challenge.counters.kills) { ++failures; }
    }
    for (const auto &ref : profile.nativeArchive->survivalLevels) {
        if (MenuDetail::NativeMissionProgress(profile, ref) != 500) { ++failures; }
    }

    ZShaderProgram program;
    ZPlayerModel player;
    ZPlayerVitals vitals;
    vitals.maximum = progress.GetHealth();
    vitals.health = vitals.maximum / 2;
    CLevel scene(toc, tables, program);
    std::vector<ZEnemyTemplateData> enemies;
    scene.BindCombat(enemies, player, vitals, 1);
    scene.SetPlayerProgress(&progress);
    CMap map;
    CGame session(scene, map, enemies);
    CPowerUpSelector powerups(toc, tables, player, vitals, scene, profile);
    ZSurvivalGameContext context{profile, savePath};
    CombatCheatResult result;
    // Start real BIG intervals, including >24h standard work, then advance only refinery time.
    profile.refinery.Bind(refinement);
    const auto refineryNow = MenuDetail::CurrentSeconds();
    for (unsigned slot = 0; slot < refinement.minutes.size(); ++slot) {
        if (refinement.minutes[slot] == 0) { continue; }
        profile.refinery.slots[slot].state = 1;
        if (!profile.refinery.BeginRefinement(slot, slot, 10, profile.xplodium, refineryNow)) { return 1; }
    }
    const auto refineryCoins = profile.coins;
    if (!ApplyCombatCheat(GameCheats::AdvanceRefinery, scene, vitals, powerups, session, &context, result, data, progress) ||
        !ReloadProfile(profile, savePath)) { return 1; }
    for (unsigned slot = 0; slot < refinement.minutes.size(); ++slot) {
        const auto &record = profile.refinery.slots[slot];
        if (refinement.minutes[slot] == 0) { if (record.state != 1) { ++failures; } continue; }
        if (refinement.minutes[slot] <= 24 * 60) { if (record.state != 3) { ++failures; } }
        else if (record.state != 2 || record.finishTime != refineryNow + refinement.minutes[slot] * 60 - 86400) { ++failures; }
        if (record.amount != 10 || record.totalDurationMs != refinement.minutes[slot] * 60000 ||
            record.startTimeSeconds != refineryNow) { ++failures; }
    }
    std::uint64_t returnedOre = 0;
    for (unsigned slot = 0; slot < refinement.minutes.size(); ++slot) {
        if (refinement.minutes[slot] != 0) { returnedOre += profile.refinery.slots[slot].amount; }
    }
    const auto beforeLock = profile.xplodium;
    if (!ApplyCombatCheat(GameCheats::ToggleRefineryLocks, scene, vitals, powerups, session, &context, result, data, progress) ||
        !ReloadProfile(profile, savePath) || profile.xplodium != beforeLock + returnedOre || profile.coins != refineryCoins) { return 1; }
    for (unsigned slot = 0; slot < refinement.minutes.size(); ++slot) {
        if (refinement.minutes[slot] != 0 &&
            (profile.refinery.slots[slot].state != 0 || profile.refinery.slots[slot].amount)) { ++failures; }
        if (refinement.minutes[slot] == 0 && profile.refinery.slots[slot].state != 1) { ++failures; }
    }
    if (!ApplyCombatCheat(GameCheats::Xplodium, scene, vitals, powerups, session, &context, result, data, progress) ||
        !ReloadProfile(profile, savePath) || profile.xplodium != beforeLock + returnedOre + GameCheats::XplodiumAmount) { return 1; }
    std::printf("[cheat-check] refinery skip-24h=1 long-interval=1 lock-refund=1 ore=1 reload=1 failures=%u\n", failures);
    if (!ApplyCombatCheat(GameCheats::LevelUp, scene, vitals, powerups, session, &context, result, data, progress) ||
        progress.GetLevel() != 3 || std::abs(vitals.health / vitals.maximum - 0.5f) > 0.001f) { ++failures; }
    if (!ApplyCombatCheat(GameCheats::MaximumLevel, scene, vitals, powerups, session, &context, result, data, progress) ||
        !progress.IsMaximumLevel() || vitals.maximum != progress.GetHealth() ||
        std::abs(vitals.health / vitals.maximum - 0.5f) > 0.001f) { ++failures; }
    const auto maximumExperience = progress.GetExperience();
    if (!ApplyCombatCheat(GameCheats::LevelUp, scene, vitals, powerups, session, &context, result, data, progress) ||
        progress.GetExperience() != maximumExperience) { ++failures; }
    if (!ReloadProfile(profile, savePath) || profile.experience != maximumExperience) { ++failures; }
    if (!ApplyCombatCheat(GameCheats::UpdateChallenges, scene, vitals, powerups, session, &context, result, data, progress) ||
        !result.challengesUpdated || !ReloadProfile(profile, savePath) ||
        !restoredChallenges.Bind(toc, tables, profile, 0)) { return 1; }
    if (restoredChallenges.cycleDay != initialChallengeDay + 3 ||
        profile.dailyLastLaunchSeconds != initialDailyLaunch || profile.dailyConsecutiveSeconds != initialDailySeconds) { ++failures; }
    std::printf("[cheat-check] chupdate menu=2 combat=1 reset=1 reload=1 daily-unchanged=1 failures=%u\n", failures);
    std::printf("[cheat-check] max-level=%u menu=combat=save-reload wave-slots=4 failures=%u\n", progress.GetLevel(), failures);
    return failures;
}
