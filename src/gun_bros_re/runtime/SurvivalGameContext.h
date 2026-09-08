/** @file SurvivalGameContext.h
 * @brief Persistent game session settings; absent in permanent research harnesses.
 */
#ifndef GUN_BROS_RE_SURVIVALGAMECONTEXT_H
#define GUN_BROS_RE_SURVIVALGAMECONTEXT_H
#include "gun_bros/CProfileManager.h"
#include "gun_bros/CombatTypes.h"
#include <map>

struct SurvivalResult {
    unsigned kills = 0, waves = 0, perfectWaves = 0, wave = 0;
    std::uint64_t experience = 0, xplodium = 0;
    std::vector<EnemyCasualty> casualties;
    std::vector<WeaponCombatProgress> weapons;
};

struct SurvivalGameContext {
    CProfileManager &profile;
    std::filesystem::path savePath;
    unsigned planet = 0;
    unsigned accountedKills = 0;
    int hordeStart = -1;
    // Program-local pointer injection for the persistent profile regression.
    bool checkControls = false;
    bool tutorial = false;
    std::uint64_t startingExperience = 0;
    std::map<std::uint64_t, unsigned> accountedWeaponExperience;
    SurvivalResult result;
};
#endif
