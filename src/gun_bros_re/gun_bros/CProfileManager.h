/** @file CProfileManager.h
 * @brief Rebuilt offline profile. Its disk format is deliberately separate from iOS.
 */
#ifndef GUN_BROS_RE_CPROFILEMANAGER_H
#define GUN_BROS_RE_CPROFILEMANAGER_H
#include "gun_bros/CPlayerConfiguration.h"
#include "gun_bros/CStoreItem.h"
#include "gun_bros/CRefinementManager.h"
#include <filesystem>
#include <string>

enum class PurchaseResult { Purchased, Owned, LevelLocked, InsufficientCoins, InsufficientWarbucks, Unsupported };

struct PowerupInventoryEntry {
    GameObjectRef resource;
    unsigned count = 0;
};

class CProfileManager {
public:
    void Reset(std::uint32_t corePackHash, const CRefinementManager::Template &refinement);
    bool LoadFromDisk(const std::filesystem::path &path);
    bool SaveToDisk(const std::filesystem::path &path) const;
    bool Owns(unsigned type, const GameObjectRef &ref) const;
    void Grant(unsigned type, const GameObjectRef &ref);
    void AddPowerup(const GameObjectRef &ref, unsigned count);
    unsigned GetPowerupCount(const GameObjectRef &ref) const;
    bool ConsumePowerup(const GameObjectRef &ref, unsigned count = 1);
    /** Local activity rewards are persisted together with their claimed bit. */
    bool ClaimActivity(unsigned index);
    unsigned ActivityProgress(unsigned index) const;
    static unsigned ActivityTarget(unsigned index);
    /** Offline equipment purchase follows level, common-else-rare, then ownership. */
    PurchaseResult AcquireItem(const CStoreItem &item, unsigned level);
    PurchaseResult AcquireCurrency(const CStoreItem &item);

    std::uint64_t experience = 0;
    std::uint64_t coins = 0;
    std::uint64_t warbucks = 0;
    std::uint64_t xplodium = 0;
    std::array<unsigned, 4> clearedWaves{};
    CPlayerConfiguration configuration;
    CRefinementManager refinery;
    std::vector<GameObjectTypeRef> inventory;
    std::vector<PowerupInventoryEntry> powerups;
    bool musicEnabled = true;
    bool soundEnabled = true;
    bool brotherEnabled = true;
    unsigned playerBrother = 0;
    unsigned claimedActivities = 0;
    std::array<std::uint64_t, 4> enemyKills{};
    // Personal bests remain separate from the four retail planet campaigns.
    std::array<unsigned, 10> hordeBestKills{};
    std::array<unsigned, 10> hordeBestWave{};
    std::array<unsigned, 10> hordeBestScore{};
    unsigned stat42Bits = 0; // Authored level event bits; remote achievement reporting is separate.
};
#endif
