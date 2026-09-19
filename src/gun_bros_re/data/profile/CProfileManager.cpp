#include "gun_bros_re/data/profile/CRefinementManager.h"
/** @file CProfileManager.cpp
 * @brief Preserve the previous profile on failed writes; never touch original saves.
 */
#define NOMINMAX
#include "gun_bros_re/data/profile/CProfileManager.h"
#include <Windows.h>
#include <fstream>
#include <cstdio>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace {
constexpr unsigned kProfileVersion = 12;
constexpr unsigned kMaximumInventoryRecords = 1024;

void WriteRef(std::ostream &stream, const GameObjectRef &ref) {
    stream << ref.packHash << ' ' << unsigned(ref.localIndex) << '\n';
}

bool ReadRef(std::istream &stream, GameObjectRef &ref) {
    unsigned index = 0;
    if (!(stream >> ref.packHash >> index) || index > 255) { return false; }
    ref.localIndex = static_cast<std::uint8_t>(index);
    return true;
}
}

void CProfileManager::Reset(std::uint32_t corePackHash, const CRefinementManager::Template &refinement) {
    nativeArchive.reset();
    firstLaunch = true;
    activeWeaponSlot = 0;
    tutorialSeen.fill(0);
    statistics.fill(0);
    dailyLastLaunchSeconds = 0;
    dailyConsecutiveSeconds = 0;
    dailyLastCommit = 0;
    experience = 0;
    coins = 0;
    warbucks = 0;
    xplodium = 0;
    clearedWaves.fill(0);
    for (auto &waves : perfectedWaves) { waves.reset(); }
    dailyLastClaimDay = -1;
    dailyConsecutiveDays = 0;
    dailyDayOffset = 0;
    tutorialCompleted = false;
    tutorialSteps = 0;
    configuration.SetDefaults(corePackHash);
    inventory.clear();
    powerups.clear();
    weaponMastery.clear();
    purchasedPackages.clear();
    packagesPurchasedThisSession.clear();
    options.Reset();
    musicEnabled = true;
    soundEnabled = true;
    brotherEnabled = true;
    pushChallenges = true;
    playerBrother = 0;
    claimedActivities = 0;
    enemyKills.fill(0);
    hordeBestKills.fill(0);
    hordeBestWave.fill(0);
    hordeBestScore.fill(0);
    stat42Bits = 0;
    for (const GameObjectRef &gun : configuration.guns) { Grant(6, gun); }
    for (const GameObjectRef &armor : configuration.armor) {
        if (!armor.IsNull()) { Grant(2, armor); }
    }
    refinery.Bind(refinement);
}

bool CProfileManager::IsPackagePurchased(const GameObjectRef &ref) const {
    // CPackageOfferMgr::IsPackagePurchased :396345 tests key existence.
    for (const GameObjectRef &entry : purchasedPackages) {
        if (entry.packHash == ref.packHash && entry.localIndex == ref.localIndex) { return true; }
    }
    return false;
}

bool CProfileManager::Owns(unsigned type, const GameObjectRef &ref) const {
    for (const GameObjectTypeRef &entry : inventory) {
        if (entry.type == type && entry.object.packHash == ref.packHash && entry.object.localIndex == ref.localIndex) { return true; }
    }
    return false;
}

unsigned CProfileManager::GetWeaponExperience(const GameObjectRef &ref) const {
    for (const auto &entry : weaponMastery) {
        if (entry.resource.packHash == ref.packHash && entry.resource.localIndex == ref.localIndex) { return entry.experience; }
    }
    return 0;
}

bool CProfileManager::IsPackageHidden(const GameObjectRef &ref) const {
    if (!IsPackagePurchased(ref)) { return false; }
    for (const GameObjectRef &entry : packagesPurchasedThisSession) {
        if (entry.packHash == ref.packHash && entry.localIndex == ref.localIndex) { return false; }
    }
    return true;
}

void CProfileManager::AddWeaponExperience(const GameObjectRef &ref, unsigned amount, unsigned maximum) {
    if (ref.IsNull() || amount == 0 || maximum == 0) { return; }
    for (auto &entry : weaponMastery) {
        if (entry.resource.packHash != ref.packHash || entry.resource.localIndex != ref.localIndex) { continue; }
        // Imported saves may exceed the current template cap. Earning XP must
        // never reduce an existing record, including records from older tables.
        if (entry.experience >= maximum) { return; }
        entry.experience = static_cast<unsigned>(std::min<std::uint64_t>(maximum, static_cast<std::uint64_t>(entry.experience) + amount));
        return;
    }
    weaponMastery.push_back({ref, std::min(amount, maximum)});
}

void CProfileManager::Grant(unsigned type, const GameObjectRef &ref) {
    if (Owns(type, ref)) { return; }
    GameObjectTypeRef entry;
    entry.type = static_cast<std::uint8_t>(type);
    entry.object = ref;
    inventory.push_back(entry);
}

CProfileManager::PurchaseResult CProfileManager::AcquireItem(const CStoreItem &item, unsigned level, bool award) {
    // Consumables, bundles with consumables, and real-money products need their
    // own runtime systems; do not charge for an item we cannot deliver.
    // Stage 10: supported consumables now use their own count inventory.
    if (item.type >= 14 || item.objects.empty()) { return CProfileManager::PurchaseResult::Unsupported; }
    if (!award && item.singlePurchase != 0) {
        if (item.resource.IsNull()) { return CProfileManager::PurchaseResult::Unsupported; }
        if (IsPackagePurchased(item.resource)) { return CProfileManager::PurchaseResult::Owned; }
    }
    bool missing = false;
    for (const GameObjectTypeRef &ref : item.objects) {
        if (ref.type == 17 && !ref.object.IsNull()) {
            if (award && GetPowerupCount(ref.object) >= 99) { continue; }
            if (!award && item.commonPrice == 0 && item.rarePrice == 0) { return CProfileManager::PurchaseResult::Unsupported; }
            missing = true;
            continue;
        }
        if ((ref.type != 2 && ref.type != 6) || ref.object.IsNull()) { return CProfileManager::PurchaseResult::Unsupported; }
        if (!Owns(ref.type, ref.object)) { missing = true; }
    }
    if (!missing) { return CProfileManager::PurchaseResult::Owned; }
    if (level < item.requiredLevel) { return CProfileManager::PurchaseResult::LevelLocked; }
    // CStoreAggregator::AcquireItem :158044 has a separate award branch;
    // its level gate still applies. Never rewrite a BIG item's price to grant it.
    if (!award) {
        if (item.commonPrice != 0) {
            if (coins < item.commonPrice) { return CProfileManager::PurchaseResult::InsufficientCoins; }
            coins -= item.commonPrice;
        } else {
            if (warbucks < item.rarePrice) { return CProfileManager::PurchaseResult::InsufficientWarbucks; }
            warbucks -= item.rarePrice;
        }
    }
    for (const GameObjectTypeRef &ref : item.objects) {
        if (ref.type == 17) {
            if (!award || GetPowerupCount(ref.object) < 99) { AddPowerup(ref.object, 1); }
        }
        else { Grant(ref.type, ref.object); }
    }
    if (!award && (item.commonPrice != 0 || item.rarePrice != 0)) {
        // Original GUNS/ARMOR/POWERUPS_BOUGHT statistics :158145.
        if (item.type <= 6) { ++statistics[10]; }
        else if (item.type <= 9) { ++statistics[11]; }
        else if (item.type <= 13) { ++statistics[12]; }
    }
    // CStoreAggregator::AcquireItem :158173 records only an actual purchase,
    // separately from the consumable counts and the award branch.
    if (!award && item.singlePurchase != 0) {
        purchasedPackages.push_back(item.resource);
        packagesPurchasedThisSession.push_back(item.resource);
    }
    return CProfileManager::PurchaseResult::Purchased;
}

CProfileManager::PurchaseResult CProfileManager::AcquireCurrency(const CStoreItem &item) {
    // CurrencyPurchase :155009. The absent online checkout is explicitly
    // confirmed by the local-mode screen before this original settlement.
    if (item.type == 14) { coins += item.commonPrice; return CProfileManager::PurchaseResult::Purchased; }
    if (item.type == 15) { warbucks += item.rarePrice; return CProfileManager::PurchaseResult::Purchased; }
    if (item.type != 16) { return CProfileManager::PurchaseResult::Unsupported; }
    if (item.value32 != 0) {
        if (coins < item.commonPrice) { return CProfileManager::PurchaseResult::InsufficientCoins; }
        coins -= item.commonPrice;
        warbucks += item.rarePrice;
    } else {
        if (warbucks < item.rarePrice) { return CProfileManager::PurchaseResult::InsufficientWarbucks; }
        warbucks -= item.rarePrice;
        coins += item.commonPrice;
    }
    return CProfileManager::PurchaseResult::Purchased;
}

bool CProfileManager::LoadFromDisk(const std::filesystem::path &path) {
    if (nativeArchive) { return (*this).ReloadNative(path); }
    if (!std::filesystem::exists(path)) { return true; }
    std::ifstream stream(path);
    std::string magic;
    unsigned version = 0;
    if (!(stream >> magic >> version) || magic != "GUNBROS_RE_PROFILE" || version < 1 || version > kProfileVersion) { return false; }
    // Parse into a candidate so truncated/corrupt files cannot partly overwrite
    // the currently loaded account. The refinery keeps its bound template.
    CProfileManager candidate = *this;
    if (!(stream >> candidate.experience >> candidate.coins >> candidate.warbucks >> candidate.xplodium)) { return false; }
    for (unsigned &wave : candidate.clearedWaves) {
        if (!(stream >> wave) || wave > 500) { return false; }
    }
    for (GameObjectRef &ref : candidate.configuration.guns) {
        if (!ReadRef(stream, ref)) { return false; }
    }
    for (GameObjectRef &ref : candidate.configuration.armor) {
        if (!ReadRef(stream, ref)) { return false; }
    }
    unsigned count = 0;
    if (!(stream >> count) || count > kMaximumInventoryRecords) { return false; }
    candidate.inventory.clear();
    for (unsigned index = 0; index < count; ++index) {
        unsigned type = 0;
        GameObjectRef ref;
        if (!(stream >> type) || (type != 2 && type != 6) || !ReadRef(stream, ref) || ref.IsNull()) { return false; }
        candidate.Grant(type, ref);
    }
    for (auto &slot : candidate.refinery.slots) {
        if (!(stream >> slot.state >> slot.amount >> slot.finishTime >> slot.efficiency) || slot.state > 3 ||
            !std::isfinite(slot.efficiency) || slot.efficiency < 0 || slot.efficiency > 100) { return false; }
    }
    std::string end;
    candidate.powerups.clear();
    if (version >= 2) {
        if (!(stream >> count) || count > kMaximumInventoryRecords) { return false; }
        for (unsigned index = 0; index < count; ++index) {
            GameObjectRef ref;
            unsigned quantity = 0;
            if (!ReadRef(stream, ref) || ref.IsNull() || !(stream >> quantity) || quantity > 1000000) { return false; }
            candidate.AddPowerup(ref, quantity);
        }
    }
    candidate.musicEnabled = true;
    candidate.soundEnabled = true;
    candidate.brotherEnabled = true;
    candidate.playerBrother = 0;
    candidate.claimedActivities = 0;
    candidate.enemyKills.fill(0);
    candidate.hordeBestKills.fill(0);
    candidate.hordeBestWave.fill(0);
    candidate.hordeBestScore.fill(0);
    candidate.stat42Bits = 0;
    if (version >= 3) {
        if (!(stream >> candidate.musicEnabled >> candidate.soundEnabled >> candidate.brotherEnabled >>
            candidate.playerBrother >> candidate.claimedActivities) || candidate.playerBrother > 1 || candidate.claimedActivities > 255) { return false; }
        for (std::uint64_t &kills : candidate.enemyKills) {
            if (!(stream >> kills)) { return false; }
        }
    }
    if (version >= 4) {
        for (unsigned &kills : candidate.hordeBestKills) { if (!(stream >> kills)) { return false; } }
        for (unsigned &wave : candidate.hordeBestWave) { if (!(stream >> wave) || wave > 999) { return false; } }
    }
    if (version >= 5) {
        for (unsigned &score : candidate.hordeBestScore) { if (!(stream >> score) || score > 3000000000u) { return false; } }
    }
    if (version >= 6 && !(stream >> candidate.stat42Bits)) { return false; }
    for (auto &waves : candidate.perfectedWaves) { waves.reset(); }
    if (version >= 7) {
        for (auto &waves : candidate.perfectedWaves) {
            std::string bits;
            if (!(stream >> bits) || bits.size() != 500 || bits.find_first_not_of("01") != std::string::npos) { return false; }
            waves = std::bitset<500>(bits);
        }
    }
    candidate.dailyLastClaimDay = -1;
    candidate.dailyConsecutiveDays = 0;
    candidate.dailyDayOffset = 0;
    if (version >= 8 && !(stream >> candidate.dailyLastClaimDay >> candidate.dailyConsecutiveDays >> candidate.dailyDayOffset)) { return false; }
    // Existing host accounts predate tutorial support and keep their progress.
    candidate.tutorialCompleted = true;
    candidate.tutorialSteps = 0;
    if (version >= 9 && (!(stream >> candidate.tutorialCompleted >> candidate.tutorialSteps) || candidate.tutorialSteps > 255)) { return false; }
    candidate.weaponMastery.clear();
    if (version >= 10) {
        if (!(stream >> count) || count > kMaximumInventoryRecords) { return false; }
        for (unsigned index = 0; index < count; ++index) {
            CProfileManager::WeaponMasteryEntry entry;
            if (!ReadRef(stream, entry.resource) || entry.resource.IsNull() || !(stream >> entry.experience)) { return false; }
            candidate.weaponMastery.push_back(entry);
        }
    }
    candidate.purchasedPackages.clear();
    if (version >= 11) {
        if (!(stream >> count) || count > kMaximumInventoryRecords) { return false; }
        for (unsigned index = 0; index < count; ++index) {
            GameObjectRef ref;
            if (!ReadRef(stream, ref) || ref.IsNull()) { return false; }
            candidate.purchasedPackages.push_back(ref);
        }
    }
    candidate.configuration.powerups.fill(255);
    if (version >= 12) {
        for (auto &powerup : candidate.configuration.powerups) {
            unsigned ordinal = 0;
            if (!(stream >> ordinal) || ordinal > 255) { return false; }
            powerup = static_cast<std::uint8_t>(ordinal);
        }
    }
    if (!(stream >> end) || end != "END") { return false; }
    for (const GameObjectRef &ref : candidate.configuration.guns) {
        if (!candidate.Owns(6, ref)) { return false; }
    }
    for (const GameObjectRef &ref : candidate.configuration.armor) {
        if (!ref.IsNull() && !candidate.Owns(2, ref)) { return false; }
    }
    *this = std::move(candidate);
    std::printf("[profile] loaded xp=%llu coins=%llu inventory=%zu\n", experience, coins, inventory.size());
    return true;
}

bool CProfileManager::SaveToDisk(const std::filesystem::path &path) const {
    if (nativeArchive) { return (*this).SaveNative(path); }
    if (!path.parent_path().empty()) { std::filesystem::create_directories(path.parent_path()); }
    std::filesystem::path temporary = path;
    temporary += L".tmp";
    std::ofstream stream(temporary, std::ios::trunc);
    if (!stream) { return false; }
    stream << "GUNBROS_RE_PROFILE " << kProfileVersion << '\n';
    stream << experience << ' ' << coins << ' ' << warbucks << ' ' << xplodium << '\n';
    for (unsigned wave : clearedWaves) { stream << wave << ' '; }
    stream << '\n';
    for (const GameObjectRef &ref : configuration.guns) { WriteRef(stream, ref); }
    for (const GameObjectRef &ref : configuration.armor) { WriteRef(stream, ref); }
    stream << inventory.size() << '\n';
    for (const GameObjectTypeRef &ref : inventory) {
        stream << unsigned(ref.type) << ' ';
        WriteRef(stream, ref.object);
    }
    stream << std::setprecision(9);
    for (const auto &slot : refinery.slots) {
        stream << slot.state << ' ' << slot.amount << ' ' << slot.finishTime << ' ' << slot.efficiency << '\n';
    }
    stream << powerups.size() << '\n';
    for (const CProfileManager::PowerupInventoryEntry &item : powerups) {
        WriteRef(stream, item.resource);
        stream << item.count << '\n';
    }
    stream << musicEnabled << ' ' << soundEnabled << ' ' << brotherEnabled << ' ' << playerBrother << ' ' << claimedActivities << '\n';
    for (std::uint64_t kills : enemyKills) { stream << kills << ' '; }
    stream << '\n';
    for (unsigned kills : hordeBestKills) { stream << kills << ' '; }
    stream << '\n';
    for (unsigned wave : hordeBestWave) { stream << wave << ' '; }
    stream << '\n';
    for (unsigned score : hordeBestScore) { stream << score << ' '; }
    stream << '\n' << stat42Bits;
    for (const auto &waves : perfectedWaves) { stream << '\n' << waves.to_string(); }
    stream << '\n' << dailyLastClaimDay << ' ' << dailyConsecutiveDays << ' ' << dailyDayOffset;
    stream << '\n' << tutorialCompleted << ' ' << tutorialSteps;
    stream << '\n' << weaponMastery.size() << '\n';
    for (const auto &entry : weaponMastery) { WriteRef(stream, entry.resource); stream << entry.experience << '\n'; }
    stream << purchasedPackages.size() << '\n';
    for (const auto &ref : purchasedPackages) { WriteRef(stream, ref); }
    // Archived host profiles retain the same selection as native DataStore 1001.
    for (const auto powerup : configuration.powerups) { stream << unsigned(powerup) << ' '; }
    stream << "\nEND\n";
    stream.close();
    if (stream.fail()) { return false; }
    // Windows rename cannot replace an existing destination; use its atomic
    // replace primitive after the entire new file was closed successfully.
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::printf("[profile] replace failed error=%lu\n", GetLastError());
        return false;
    }
    return true;
}

void CProfileManager::AddPowerup(const GameObjectRef &ref, unsigned count) {
    for (CProfileManager::PowerupInventoryEntry &entry : powerups) {
        if (entry.resource.packHash == ref.packHash && entry.resource.localIndex == ref.localIndex) {
            entry.count += count;
            return;
        }
    }
    powerups.push_back({ref, count});
}

unsigned CProfileManager::GetPowerupCount(const GameObjectRef &ref) const {
    for (const CProfileManager::PowerupInventoryEntry &entry : powerups) {
        if (entry.resource.packHash == ref.packHash && entry.resource.localIndex == ref.localIndex) { return entry.count; }
    }
    return 0;
}

bool CProfileManager::ConsumePowerup(const GameObjectRef &ref, unsigned count) {
    for (CProfileManager::PowerupInventoryEntry &entry : powerups) {
        if (entry.resource.packHash != ref.packHash || entry.resource.localIndex != ref.localIndex) { continue; }
        if (entry.count < count) { return false; }
        entry.count -= count;
        return true;
    }
    return false;
}

unsigned CProfileManager::ActivityTarget(unsigned index) {
    constexpr unsigned targets[] = {5, 50, 250, 4, 15, 1000, 4, 50};
    if (index >= 8) { return 0; }
    return targets[index];
}

unsigned CProfileManager::ActivityProgress(unsigned index) const {
    std::uint64_t value = 0;
    if (index == 0 || index == 7) {
        for (unsigned waves : clearedWaves) { value += waves; }
    } else if (index == 1 || index == 5) {
        for (std::uint64_t kills : enemyKills) { value += kills; }
    } else if (index == 2) { value = enemyKills[0]; }
    else if (index == 3) {
        for (const GameObjectTypeRef &item : inventory) { if (item.type == 6) { ++value; } }
    } else if (index == 4) { value = clearedWaves[1]; }
    else if (index == 6) {
        for (unsigned waves : clearedWaves) { if (waves > 0) { ++value; } }
    }
    return static_cast<unsigned>(std::min<std::uint64_t>(value, ActivityTarget(index)));
}

bool CProfileManager::ClaimActivity(unsigned index) {
    // Historical host-only research rewards are not CChallengeTemplate prizes.
    // Native DataStores must never receive this invented reward table.
    if (nativeArchive) {
        std::printf("[profile] reject legacy activity reward for native account index=%u\n", index);
        return false;
    }
    if (index >= 8 || (claimedActivities & (1u << index)) != 0 || ActivityProgress(index) < ActivityTarget(index)) { return false; }
    // The original online reward service is absent. These explicitly local
    // activities give an offline route to currency; never repeat a claimed reward.
    constexpr unsigned coinRewards[] = {100, 150, 500, 250, 750, 1000, 1000, 1500};
    constexpr unsigned warbuckRewards[] = {1, 1, 3, 2, 3, 5, 5, 10};
    coins += coinRewards[index];
    warbucks += warbuckRewards[index];
    claimedActivities |= 1u << index;
    return true;
}
