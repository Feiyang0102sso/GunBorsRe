/** Local challenge content and the original deterministic daily selection. */
#pragma once
#include "gun_bros_re/data/CDailyBonusTracking.h"
#include "gun_bros_re/data/WeaponCatalog.h"

class CChallengeManager {
public:
    struct Counters {
        unsigned kills = 0, clearedWaves = 0, perfectWaves = 0;
        unsigned initialLevel = 0, initialFriends = 0, usedPowerups = 0;
    };
    /** CStatisticEnemy key: enemy, bullet, LEVEL native 68 group, mastery critical. */
    struct Kill {
        GameObjectRef enemy, bullet;
        unsigned group = 0, count = 0;
        bool critical = false, player = true;
    };
    struct Session {
        GameObjectRef level;
        std::array<GameObjectRef, 2> guns;
        unsigned gameType = 1, wave = 0;
        bool waveCleared = false, perfect = false, ended = false;
        std::vector<Kill> kills;
        std::vector<GameObjectRef> powerups;
    };
    struct Template {
        GameObjectRef reference;
        unsigned category = 0;
        CGameAssetRef name, description;
        std::array<unsigned, 3> participationRequired{};
        std::array<GameObjectRef, 3> prizes;
        GameObjectRef level;
        unsigned flags = 0, requiredKills = 0, circumstanceMask = 0, gunCategoryMask = 0;
        std::vector<GameObjectTypeRef> weapons;
        std::vector<GameObjectRef> enemies, powerups;
        unsigned firstWave = 0, lastWave = 0, perfectWaves = 0, levelIncreases = 0, friendIncreases = 0;
        bool Init(CArrayInputStream &stream);
    };
    struct Challenge {
        unsigned templateIndex = 0, progress = 0, rewardStatus = 0, completedFriends = 0;
        unsigned achieved = 0, target = 0, progressLabel = 0;
        std::string name, description;
        std::array<DailyPrize, 3> prizes;
        Counters counters;
        bool applicable = true;
    };
    bool Load(CResTOCManager &toc, PackTables &tables);
    std::vector<unsigned> GenerateChallengeList(unsigned cycleDay) const;
    /** Read saved cycle/progress; only an uninitialized cycle uses host time. */
    bool Bind(CResTOCManager &toc, PackTables &tables, const CProfileManager &profile, unsigned seconds);
    /** Apply the original forward-only network day rollover, then bind its list. */
    bool InitProgressData(CResTOCManager &toc, PackTables &tables, CProfileManager &profile, unsigned seconds);
    void UpdateFromLevelSession(const Session &session, const std::vector<WeaponEntry> &weapons, const CProfileManager &profile);
    void UpdateChallengeStatusData(const CProfileManager &profile, bool ended);
    bool StoreProgress(CProfileManager &profile) const;
    /** Award one completed challenge's available tiers; caller saves the transaction. */
    bool AwardAvailableRewards(CProfileManager &profile, const std::vector<StoreEntry> &store, unsigned &awarded);
    unsigned cycleDay = 0;
    std::vector<Template> templates;
    std::vector<Challenge> current;
private:
    std::vector<std::vector<unsigned>> packTemplates;
};
