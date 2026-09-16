/** @file ZSurvivalGameContext.h
 * @brief Persistent game session settings; absent in permanent research harnesses.
 */
#ifndef GUN_BROS_RE_ZSURVIVALGAMECONTEXT_H
#define GUN_BROS_RE_ZSURVIVALGAMECONTEXT_H
#include "gun_bros_re/data/CProfileManager.h"
#include "gun_bros_re/gameplay/ZCombatTypes.h"
#include "gun_bros_re/gameplay/ZMultiplayerStatistics.h"
#include <map>

struct ZSurvivalResult {
    bool live = false;
    bool deathmatch = false;
    unsigned matchResult = 0;
    unsigned matchKillLimit = 0, matchTimeLimitSeconds = 0;
    std::string peerName = "LOCAL BOT";
    ZMultiplayerStats peers[2];
    bool horde = false;
    unsigned score = 0, highScore = 0, bestKillStreak = 0, stopwatchMs = 0;
    unsigned wavesPerRevolution = 0, waveLimit = 0;
    unsigned kills = 0, waves = 0, perfectWaves = 0, wave = 0;
    std::uint64_t experience = 0, xplodium = 0;
    std::vector<ZEnemyCasualty> casualties;
    std::vector<ZWeaponCombatProgress> weapons;
};

class CBGM;
class ZSurvivalFrameDriver;
class ZLocalBotFriend;

struct ZSurvivalGameContext {
    CProfileManager &profile;
    std::filesystem::path savePath;
    unsigned planet = 0;
    unsigned accountedKills = 0;
    int hordeStart = -1;
    GameObjectRef mission, missionLevel;
    // Optional borrowed input/observation driver supplied by the caller.
    ZSurvivalFrameDriver *frameDriver = nullptr;
    bool tutorial = false;
    bool debugTutorial = false; // Menu-only replay: ESC returns directly, with a no-save notice.
    std::uint64_t startingExperience = 0;
    std::map<std::uint64_t, unsigned> accountedWeaponExperience;
    ZSurvivalResult result;
    CBGM *music = nullptr; // Owned by the outer game flow, including loading and results.
    // Debug map sessions use a copy of the current profile without disk writes.
    bool persistProgress = true;
    ZLocalBotFriend *botFriend = nullptr;
    std::uint64_t accountedPeerXplodium = 0;
    unsigned accountedPeerKills = 0;
    bool SaveProfile() const {
        if (!persistProgress) { return true; }
        return profile.SaveToDisk(savePath);
    }
};
#endif
