/** @file CProfileManager.h
 * @brief Rebuilt offline profile. Its disk format is deliberately separate from iOS.
 * Current GUI: original iOS DataStore records via NativeProfile; the separate
 * text format above is retained only for explicitly selected .dat research.
 */
#ifndef GUN_BROS_RE_CPROFILEMANAGER_H
#define GUN_BROS_RE_CPROFILEMANAGER_H
#include "gun_bros_re/data/CPlayerConfiguration.h"
#include "gun_bros_re/data/CStoreItem.h"
#include "gun_bros_re/data/CRefinementManager.h"
#include "gun_bros_re/data/NativeProfile.h"
#include <optional>
#include <filesystem>
#include <string>
#include <bitset>
#include "gun_bros_re/data/CFriendPowerManager.h"

enum class PurchaseResult { Purchased, Owned, LevelLocked, InsufficientCoins, InsufficientWarbucks, Unsupported };

struct PowerupInventoryEntry {
    GameObjectRef resource;
    unsigned count = 0;
};

struct WeaponMasteryEntry {
    GameObjectRef resource;
    unsigned experience = 0;
};

class CProfileManager {
public:
    unsigned friendCount = 0; // Runtime roster membership, not native record 1006.
    void Reset(std::uint32_t corePackHash, const CRefinementManager::Template &refinement);
    bool LoadFromDisk(const std::filesystem::path &path);
    bool SaveToDisk(const std::filesystem::path &path) const;
    bool Owns(unsigned type, const GameObjectRef &ref) const;
    bool IsPackagePurchased(const GameObjectRef &ref) const;
    bool IsPackageHidden(const GameObjectRef &ref) const;
    void Grant(unsigned type, const GameObjectRef &ref);
    void AddPowerup(const GameObjectRef &ref, unsigned count);
    unsigned GetPowerupCount(const GameObjectRef &ref) const;
    bool ConsumePowerup(const GameObjectRef &ref, unsigned count = 1);
    unsigned GetWeaponExperience(const GameObjectRef &ref) const;
    void AddWeaponExperience(const GameObjectRef &ref, unsigned amount, unsigned maximum);
    /** Local activity rewards are persisted together with their claimed bit. */
    bool ClaimActivity(unsigned index);
    unsigned ActivityProgress(unsigned index) const;
    static unsigned ActivityTarget(unsigned index);
    /** Offline equipment purchase follows level, common-else-rare, then ownership. */
    PurchaseResult AcquireItem(const CStoreItem &item, unsigned level, bool award = false);
    PurchaseResult AcquireCurrency(const CStoreItem &item);

    std::uint64_t experience = 0;
    std::uint64_t coins = 0;
    std::uint64_t warbucks = 0;
    std::uint64_t xplodium = 0;
    std::array<unsigned, 4> clearedWaves{};
    // A cleared wave is not necessarily perfect; old host saves start unmarked.
    std::array<std::bitset<500>, 4> perfectedWaves{};
    std::int64_t dailyLastClaimDay = -1;
    unsigned dailyConsecutiveDays = 0;
    unsigned dailyDayOffset = 0;
    // Host checkpoints follow the restored level tutorial, separate from UI tips.
    bool tutorialCompleted = false;
    unsigned tutorialSteps = 0;
    CPlayerConfiguration configuration;
    CRefinementManager refinery;
    std::vector<GameObjectTypeRef> inventory;
    std::vector<PowerupInventoryEntry> powerups;
    std::vector<WeaponMasteryEntry> weaponMastery;
    // CPackageOfferMgr Collection (1018), keyed by original STORE reference.
    std::vector<GameObjectRef> purchasedPackages;
    // Session-only adapter for the original cached store override: show OWNED
    // until restart, then apply CStoreItemOverride's purchased-package hiding.
    std::vector<GameObjectRef> packagesPurchasedThisSession;
    COptionsMgr options;
    bool musicEnabled = true;
    bool soundEnabled = true;
    bool brotherEnabled = true;
    bool pushChallenges = true; // CPlayerProgress mem+86, gbPushChallenges.
    unsigned playerBrother = 0;
    unsigned claimedActivities = 0;
    std::array<std::uint64_t, 4> enemyKills{};
    // Personal bests remain separate from the four retail planet campaigns.
    std::array<unsigned, 10> hordeBestKills{};
    std::array<unsigned, 10> hordeBestWave{};
    std::array<unsigned, 10> hordeBestScore{};
    unsigned stat42Bits = 0; // Authored level event bits; remote achievement reporting is separate.
    // Original client fields, independent from the older research text profile.
    std::optional<NativeProfileArchive> nativeArchive;
    bool firstLaunch = true; // Original flag clears at player selection, not tutorial end.
    unsigned activeWeaponSlot = 0;
    std::array<std::uint8_t, 22> tutorialSeen{};
    std::array<std::uint32_t, 47> statistics{};
    std::uint32_t dailyLastLaunchSeconds = 0;
    std::uint32_t dailyConsecutiveSeconds = 0;
    std::uint32_t dailyLastCommit = 0;
};
#endif
