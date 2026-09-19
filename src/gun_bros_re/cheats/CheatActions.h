#pragma once
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include "gun_bros_re/data/profile/CPlayerProgress.h"
class ZWindow;
class CProfileManager;
class CDailyBonusTracking;
class CChallengeManager;
class CLevel;
class CGame;
class CPowerUpSelector;
struct CGameFlow;
namespace MenuDetail { class CMenuSystem; }

/** Commands mutate game state here; the host applies the returned timing/UI changes. */
struct CombatCheatResult {
    bool resume = false;
    bool resetClock = false;
    bool challengesUpdated = false;
    bool botShop = false, botPowerup = false;
};
bool ApplyCombatCheat(const std::string &command, CLevel &scene, CBrother::Vitals &vitals,
    CPowerUpSelector &powerups, CGame &session, CGameFlow *context, CombatCheatResult &result,
    const CPlayerProgress::Template &progressData, CPlayerProgress &progress);
bool ProcessMenuCheats(ZWindow &window, CProfileManager &profile, MenuDetail::CMenuSystem &state,
    const CDailyBonusTracking &daily, const std::filesystem::path &savePath,
    const CPlayerProgress::Template &progressData, CPlayerProgress &progress);
namespace GameCheats {
/** Local resource shortcuts share the menu/combat path and original save records. */
bool ApplyRefineryCheat(const std::string &command, CProfileManager &profile, std::int64_t now);
/** Host targets use the original BIG progression and LEVEL tables, never copied resource values. */
std::uint64_t ExperienceTarget(const std::string &command,
    const CPlayerProgress::Template &data, const CPlayerProgress &progress);
bool UnlockAllWaves(CProfileManager &profile);
/** Advance only the saved challenge day, using the original rollover and selection. */
bool AdvanceChallenges(CProfileManager &profile, CChallengeManager &challenges, std::uint32_t now);
}
namespace MenuDetail {
void AdvanceDailyDebugDay(CProfileManager &profile, const CDailyBonusTracking &daily, std::uint32_t now);
}
