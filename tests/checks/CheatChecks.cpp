/** Verify desktop cheats through the menu, combat and native save consumers. */
#include "gun_bros_re/ui/MenuInternal.h"
#include "gun_bros_re/cheats/CheatCodes.h"
#include "gun_bros_re/data/NativeProfile.h"
#include "gun_bros_re/gameplay/SurvivalSession.h"
#include "gun_bros_re/gameplay/PowerupScene.h"
#include "TestOutput.h"
#include <SDL3/SDL.h>

unsigned CheckCheatActions(CResTOCManager &toc, PackTables &tables,
    const CPlayerProgress::Template &data) {
    unsigned failures = 0;
    CProfileManager profile;
    if (!CreateTransientNativeProfile(toc, tables, profile)) { return 1; }
    CWindow window;
    if (!window.Open("Cheat action checks", 320, 240)) { return 1; }
    GameCheats::Bind();
    window.EnableCheats(true);
    CPlayerProgress progress;
    progress.Bind(data);
    MenuDetail::MenuState menu;
    CDailyBonusTracking daily;
    const auto savePath = TestOutput::Path("cheat-profile");
    const auto initialCoins = profile.coins;
    const auto initialWarbucks = profile.warbucks;
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
        GameCheats::UpdateChallenges, GameCheats::UpdateChallenges}) {
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
    if (!ReloadNativeProfile(profile, savePath) || profile.experience != progress.GetExperience()) { ++failures; }
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

    CShaderProgram program;
    PlayerModel player;
    PlayerVitals vitals;
    vitals.maximum = progress.GetHealth();
    vitals.health = vitals.maximum / 2;
    WeaponEffects effects(toc, tables, program);
    std::vector<EnemyTemplateData> enemies;
    CombatScene scene(tables, program, enemies, player, vitals, effects, 1);
    scene.SetPlayerProgress(&progress);
    CMap map;
    SurvivalSession session(scene, map, enemies);
    PowerupScene powerups(toc, tables, player, vitals, scene, effects, profile);
    SurvivalGameContext context{profile, savePath};
    CombatCheatResult result;
    if (!ApplyCombatCheat(GameCheats::LevelUp, scene, vitals, powerups, session, &context, result, data, progress) ||
        progress.GetLevel() != 3 || std::abs(vitals.health / vitals.maximum - 0.5f) > 0.001f) { ++failures; }
    if (!ApplyCombatCheat(GameCheats::MaximumLevel, scene, vitals, powerups, session, &context, result, data, progress) ||
        !progress.IsMaximumLevel() || vitals.maximum != progress.GetHealth() ||
        std::abs(vitals.health / vitals.maximum - 0.5f) > 0.001f) { ++failures; }
    const auto maximumExperience = progress.GetExperience();
    if (!ApplyCombatCheat(GameCheats::LevelUp, scene, vitals, powerups, session, &context, result, data, progress) ||
        progress.GetExperience() != maximumExperience) { ++failures; }
    if (!ReloadNativeProfile(profile, savePath) || profile.experience != maximumExperience) { ++failures; }
    if (!ApplyCombatCheat(GameCheats::UpdateChallenges, scene, vitals, powerups, session, &context, result, data, progress) ||
        !result.challengesUpdated || !ReloadNativeProfile(profile, savePath) ||
        !restoredChallenges.Bind(toc, tables, profile, 0)) { return 1; }
    if (restoredChallenges.cycleDay != initialChallengeDay + 3 ||
        profile.dailyLastLaunchSeconds != initialDailyLaunch || profile.dailyConsecutiveSeconds != initialDailySeconds) { ++failures; }
    std::printf("[cheat-check] chupdate menu=2 combat=1 reset=1 reload=1 daily-unchanged=1 failures=%u\n", failures);
    std::printf("[cheat-check] max-level=%u menu=combat=save-reload wave-slots=4 failures=%u\n", progress.GetLevel(), failures);
    return failures;
}
