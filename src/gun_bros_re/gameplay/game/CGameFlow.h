/** @file CGameFlow.h
 * @brief Cross-session configuration and progress, following gameFlow.cpp :77286.
 * Save paths, local bots and result snapshots are explicit desktop extensions.
 */
#ifndef GUN_BROS_RE_CGAMEFLOW_H
#define GUN_BROS_RE_CGAMEFLOW_H
#include "gun_bros_re/gameplay/enemy/CEnemyCasualty.h"
#include "gun_bros_re/gameplay/weapon/CGun.h"
#include "gun_bros_re/data/CProfileManager.h"
#include "gun_bros_re/gameplay/collision/Collision.h"
#include "gun_bros_re/gameplay/multiplayer/ZMultiplayerStatistics.h"
#include <map>

class CBGM;
class CLevel;
class CPlayerProgress;
class ZLocalBotFriend;

struct CGameFlow {
    struct Result {
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
        std::vector<CEnemyCasualty> casualties;
        std::vector<CGun::Progress> weapons;
    };

    CProfileManager &profile;
    std::filesystem::path savePath;
    unsigned planet = 0;
    unsigned accountedKills = 0;
    int hordeStart = -1;
    GameObjectRef mission, missionLevel;
    bool tutorial = false;
    bool debugTutorial = false; // Menu-only replay: ESC returns directly, with a no-save notice.
    std::uint64_t startingExperience = 0;
    std::map<std::uint64_t, unsigned> accountedWeaponExperience;
    Result result;
    CBGM *music = nullptr; // Owned by the outer game flow, including loading and results.
    // Debug map sessions use a copy of the current profile without disk writes.
    bool persistProgress = true;
    ZLocalBotFriend *botFriend = nullptr;
    std::uint64_t accountedPeerXplodium = 0;
    unsigned accountedPeerKills = 0;
    // CGameFlow::UpdatePlayerProgress :77476 and CGame's wave/wrap-up callbacks.
    bool UpdatePlayerProgress(const CPlayerProgress &progress, const CLevel &level, std::uint64_t &accountedXplodium,
                              bool missionEnded = false);
    bool SaveProfile() const {
        if (!persistProgress) { return true; }
        return profile.SaveToDisk(savePath);
    }
};
#endif
