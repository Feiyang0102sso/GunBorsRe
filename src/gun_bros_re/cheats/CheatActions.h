#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include "gun_bros_re/data/CPlayerProgress.h"
class CWindow;
class CProfileManager;
class CDailyBonusTracking;
class CombatScene;
class SurvivalSession;
class PowerupScene;
struct PlayerVitals;
struct SurvivalGameContext;
namespace MenuDetail { struct MenuState; }

/** Commands mutate game state here; the host applies the returned timing/UI changes. */
struct CombatCheatResult {
    bool resume = false;
    bool resetClock = false;
};
bool ApplyCombatCheat(const std::string &command, CombatScene &scene, PlayerVitals &vitals,
    PowerupScene &powerups, SurvivalSession &session, SurvivalGameContext *context, CombatCheatResult &result,
    const CPlayerProgress::Template &progressData, CPlayerProgress &progress);
bool ProcessMenuCheats(CWindow &window, CProfileManager &profile, MenuDetail::MenuState &state,
    const CDailyBonusTracking &daily, const std::filesystem::path &savePath,
    const CPlayerProgress::Template &progressData, CPlayerProgress &progress);
namespace GameCheats {
/** Host targets use the original BIG progression and LEVEL tables, never copied resource values. */
std::uint64_t ExperienceTarget(const std::string &command,
    const CPlayerProgress::Template &data, const CPlayerProgress &progress);
bool UnlockAllWaves(CProfileManager &profile);
}
namespace MenuDetail {
void AdvanceDailyDebugDay(CProfileManager &profile, const CDailyBonusTracking &daily, std::uint32_t now);
}
