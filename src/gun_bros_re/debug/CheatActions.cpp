/** Host cheat actions shared by the menu and survival entry points. */
#include "gun_bros_re/debug/CheatActions.h"
#include "gun_bros_re/debug/CheatCodes.h"
#include "gun_bros_re/ui/MenuInternal.h"
#include "gun_bros_re/gameplay/SurvivalSession.h"
#include "gun_bros_re/gameplay/PowerupScene.h"

namespace MenuDetail {
/** User-authorized cht advances the native elapsed-day accumulator. The saved
 * launch timestamp stays on the real clock, so restarting cannot underflow it. */
void AdvanceDailyDebugDay(CProfileManager &profile, const CDailyBonusTracking &daily, std::uint32_t now) {
    if (!profile.nativeArchive) { ++profile.dailyDayOffset; return; }
    daily.RefreshUsageData(profile, now);
    profile.dailyConsecutiveSeconds += 86400;
    profile.dailyConsecutiveDays = profile.dailyConsecutiveSeconds / 86400 + 1;
}
}

bool ProcessMenuCheats(CWindow &window, CProfileManager &profile, MenuDetail::MenuState &state,
    const CDailyBonusTracking &daily, const std::filesystem::path &savePath) {
#if GB_ENABLE_CHEATS
    for (std::string cheat = window.TakeCheatCode(); !cheat.empty(); cheat = window.TakeCheatCode()) {
        if (cheat == GameCheats::Money) {
            profile.coins += GameCheats::Coins;
            profile.warbucks += GameCheats::Warbucks;
            state.message = GameCheats::MoneyMessage;
        }
        if (cheat == GameCheats::NextDay) {
            MenuDetail::AdvanceDailyDebugDay(profile, daily, static_cast<std::uint32_t>(MenuDetail::CurrentSeconds()));
            state.Navigate(24);
        }
        if (cheat == GameCheats::ToggleDebug) { GameHostSettings().debugMode = !GameHostSettings().debugMode; }
        if (cheat == GameCheats::ToggleConnection) { GameHostSettings().isConnected = !GameHostSettings().isConnected; }
        if (cheat == GameCheats::HealthOrGreeting) { state.Navigate(24); }
        if (cheat == GameCheats::UnlockWaves) {
            profile.clearedWaves.fill(GameCheats::ClearedWaves);
            state.message = GameCheats::UnlockMessage;
        }
        if (!profile.SaveToDisk(savePath)) { return false; }
        std::printf("[cheat] %s\n", cheat.c_str());
    }
#endif
    return true;
}

bool ApplyCombatCheat(const std::string &cheat, CombatScene &scene, PlayerVitals &vitals,
    PowerupScene &powerups, SurvivalSession &session, SurvivalGameContext *context, CombatCheatResult &result) {
#if GB_ENABLE_CHEATS
    if (cheat == GameCheats::ToggleDebug) { GameHostSettings().debugMode = !GameHostSettings().debugMode; }
    if (cheat == GameCheats::ToggleConnection) { GameHostSettings().isConnected = !GameHostSettings().isConnected; }
    if (cheat == GameCheats::Invincible) { vitals.invincible = !vitals.invincible; }
    if (cheat == GameCheats::HealthOrGreeting && !vitals.dead) { vitals.health = vitals.maximum; }
    if (cheat == GameCheats::Suicide && !powerups.IsMovieActive() && scene.Suicide()) {
        result.resume = true;
        std::printf("[death] suicide started\n");
    }
    if (cheat == GameCheats::Boss) {
        result.resume = session.SkipToBoss();
        result.resetClock = true;
    }
    if (context != nullptr) {
        if (cheat == GameCheats::Money) { context->profile.coins += GameCheats::Coins; context->profile.warbucks += GameCheats::Warbucks; }
        if (cheat == GameCheats::NextDay) { ++context->profile.dailyDayOffset; }
        if (cheat == GameCheats::UnlockWaves) { context->profile.clearedWaves.fill(GameCheats::ClearedWaves); }
        if (!context->SaveProfile()) { return false; }
    }
    std::printf("[cheat] %s invincible=%d\n", cheat.c_str(), vitals.invincible);
#endif
    return true;
}
