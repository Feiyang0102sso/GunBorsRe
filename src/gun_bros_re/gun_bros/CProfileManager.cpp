/** @file CProfileManager.cpp
 * @brief Preserve the previous profile on failed writes; never touch original saves.
 */
#define NOMINMAX
#include "gun_bros/CProfileManager.h"
#include "runtime/PowerupCatalog.h"
#include <Windows.h>
#include <fstream>
#include <cstdio>
#include <iomanip>
#include <cmath>

namespace {
constexpr unsigned kProfileVersion = 2;
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
    experience = 0;
    coins = 0;
    warbucks = 0;
    xplodium = 0;
    clearedWaves.fill(0);
    configuration.SetDefaults(corePackHash);
    inventory.clear();
    powerups.clear();
    for (const GameObjectRef &gun : configuration.guns) { Grant(6, gun); }
    for (const GameObjectRef &armor : configuration.armor) {
        if (!armor.IsNull()) { Grant(2, armor); }
    }
    refinery.Bind(refinement);
}

bool CProfileManager::Owns(unsigned type, const GameObjectRef &ref) const {
    for (const GameObjectTypeRef &entry : inventory) {
        if (entry.type == type && entry.object.packHash == ref.packHash && entry.object.localIndex == ref.localIndex) { return true; }
    }
    return false;
}

void CProfileManager::Grant(unsigned type, const GameObjectRef &ref) {
    if (Owns(type, ref)) { return; }
    GameObjectTypeRef entry;
    entry.type = static_cast<std::uint8_t>(type);
    entry.object = ref;
    inventory.push_back(entry);
}

PurchaseResult CProfileManager::AcquireItem(const CStoreItem &item, unsigned level) {
    // Consumables, bundles with consumables, and real-money products need their
    // own runtime systems; do not charge for an item we cannot deliver.
    // Stage 10: supported consumables now use their own count inventory.
    if (item.type >= 14 || item.objects.empty()) { return PurchaseResult::Unsupported; }
    bool missing = false;
    for (const GameObjectTypeRef &ref : item.objects) {
        if (ref.type == 17 && IsPlayablePowerup(ref.object)) {
            if (item.commonPrice == 0 && item.rarePrice == 0) { return PurchaseResult::Unsupported; }
            missing = true;
            continue;
        }
        if ((ref.type != 2 && ref.type != 6) || ref.object.IsNull()) { return PurchaseResult::Unsupported; }
        if (!Owns(ref.type, ref.object)) { missing = true; }
    }
    if (!missing) { return PurchaseResult::Owned; }
    if (level < item.requiredLevel) { return PurchaseResult::LevelLocked; }
    if (item.commonPrice != 0) {
        if (coins < item.commonPrice) { return PurchaseResult::InsufficientCoins; }
        coins -= item.commonPrice;
    } else {
        if (warbucks < item.rarePrice) { return PurchaseResult::InsufficientWarbucks; }
        warbucks -= item.rarePrice;
    }
    for (const GameObjectTypeRef &ref : item.objects) {
        if (ref.type == 17) { AddPowerup(ref.object, 1); }
        else { Grant(ref.type, ref.object); }
    }
    return PurchaseResult::Purchased;
}

bool CProfileManager::LoadFromDisk(const std::filesystem::path &path) {
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
    for (const PowerupInventoryEntry &item : powerups) {
        WriteRef(stream, item.resource);
        stream << item.count << '\n';
    }
    stream << "END\n";
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
    for (PowerupInventoryEntry &entry : powerups) {
        if (entry.resource.packHash == ref.packHash && entry.resource.localIndex == ref.localIndex) {
            entry.count += count;
            return;
        }
    }
    powerups.push_back({ref, count});
}

unsigned CProfileManager::GetPowerupCount(const GameObjectRef &ref) const {
    for (const PowerupInventoryEntry &entry : powerups) {
        if (entry.resource.packHash == ref.packHash && entry.resource.localIndex == ref.localIndex) { return entry.count; }
    }
    return 0;
}

bool CProfileManager::ConsumePowerup(const GameObjectRef &ref, unsigned count) {
    for (PowerupInventoryEntry &entry : powerups) {
        if (entry.resource.packHash != ref.packHash || entry.resource.localIndex != ref.localIndex) { continue; }
        if (entry.count < count) { return false; }
        entry.count -= count;
        return true;
    }
    return false;
}
