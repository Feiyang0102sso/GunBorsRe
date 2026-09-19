/** Native DataStore responsibilities; see saves/GB_save_profile.bt and save_payloads.bt. */
#include "gun_bros_re/data/profile/CProfileManagerStorage.h"
#include "gun_bros_re/data/profile/CDailyBonusTracking.h"
#include <algorithm>
#include <cstdio>
using namespace ProfileStorageDetail;

namespace ProfileStorageDetail {
/** CContentTracker::UserData::Init :225636, PerPackData::Serialize :225374. */
bool InitializeContentSeen(CProfileManager::Archive &archive) {
    auto &bytes = archive.records[14].payload;
    if (bytes.size() != 1 || bytes[0] != 0) { return true; }
    const unsigned packs = archive.toc->GetPackCount();
    if (packs > 255) { return false; }
    bytes[0] = static_cast<std::uint8_t>(packs);
    const ZGameSection sections[] = {ZGameSection::Armor, ZGameSection::Gun, ZGameSection::Planet, ZGameSection::Powerup};
    for (unsigned pack = 0; pack < packs; ++pack) {
        const std::size_t header = bytes.size();
        bytes.resize(header + 9, 0);
        Put32(bytes, header, archive.toc->GetPack(pack)->GetPackHash());
        for (unsigned category = 0; category < 4; ++category) {
            const unsigned count = archive.tables->GetObjectPack(pack).GetObjectCount(sections[category]);
            if (count == 0) { continue; }
            if (count > 255) { return false; }
            ++bytes[header + 8];
            bytes.push_back(static_cast<std::uint8_t>(category));
            bytes.push_back(static_cast<std::uint8_t>(count));
            bytes.insert(bytes.end(), (count + 7) / 8, 0);
        }
        Put32(bytes, header + 4, static_cast<unsigned>(bytes.size() - header - 8));
    }
    return true;
}

/** Set only the authored object's seen bit; preserve unrelated packs and bits. */
bool MarkContentSeen(CProfileManager::Archive &archive, const GameObjectTypeRef &object) {
    unsigned category = 4;
    if (object.type == 2) { category = 0; }
    else if (object.type == 6) { category = 1; }
    else if (object.type == 13) { category = 2; }
    else if (object.type == 17) { category = 3; }
    if (category == 4) { return true; }
    auto &bytes = archive.records[14].payload;
    CArrayInputStream input(bytes);
    const unsigned packs = input.ReadUInt8();
    for (unsigned pack = 0; pack < packs; ++pack) {
        const unsigned hash = input.ReadUInt32();
        input.ReadUInt32();
        const unsigned categories = input.ReadUInt8();
        for (unsigned index = 0; index < categories; ++index) {
            const unsigned id = input.ReadUInt8();
            const unsigned count = input.ReadUInt8();
            if (hash == object.object.packHash && id == category) {
                if (object.object.localIndex >= count || input.Available() < (count + 7) / 8) { return false; }
                bytes[input.Position() + object.object.localIndex / 8] |= 1u << (object.object.localIndex % 8);
                return true;
            }
            input.Skip((count + 7) / 8);
        }
    }
    std::printf("[native-profile] missing content bit type=%u hash=%u ordinal=%u\n", object.type, object.object.packHash, object.object.localIndex);
    return false;
}

/** Decode proven fields without replacing unknown values or unsupported records. */
bool ApplyArchive(CProfileManager &profile, CProfileManager::Archive archive) {
    CProfileManager candidate = profile;
    candidate.options = archive.options;
    candidate.soundEnabled = candidate.options.SoundEnabled();
    candidate.musicEnabled = candidate.options.MusicEnabled();
    candidate.brotherEnabled = candidate.options.AutoBro() != 0;
    CPlayerProgress::ReadProfileData(candidate, archive.records[0].payload);
    archive.loadedExperience = candidate.experience;
    const auto &equipment = archive.records[1].payload;
    for (unsigned slot = 0; slot < 2; ++slot) {
        candidate.configuration.guns[slot] = MemoryRef(equipment, slot * 8);
        if (candidate.configuration.guns[slot].IsNull() || !CheckRef(archive, candidate.configuration.guns[slot], 6) ||
            !CheckRef(archive, MemoryRef(equipment, 16 + slot * 8), 3)) { return false; }
    }
    for (unsigned slot = 0; slot < 4; ++slot) {
        candidate.configuration.armor[slot] = MemoryRef(equipment, 32 + slot * 8);
        if (!CheckRef(archive, candidate.configuration.armor[slot], 2)) { return false; }
    }
    candidate.activeWeaponSlot = equipment[64];
    candidate.configuration.powerups = {equipment[116], equipment[117]};
    candidate.playerBrother = equipment[65];
    if (candidate.activeWeaponSlot > 1 || candidate.playerBrother > 1) {
        std::printf("[native-profile] unsupported player selection slot=%u brother=%u\n", candidate.activeWeaponSlot, candidate.playerBrother);
        return false;
    }
    candidate.inventory.clear();
    candidate.powerups.clear();
    candidate.purchasedPackages.clear();
    // saves/save_payloads.bt: 1018 entries have an 8-byte collection key,
    // two preserved alignment bytes and a uint32 value. AddItem initializes
    // that value to zero; existence, not value, means purchased (:396345).
    const auto &packages = archive.records[18].payload;
    for (std::size_t offset = 4; offset < packages.size(); offset += 14) {
        if (packages[offset + 5] != 22) { continue; }
        const GameObjectRef ref = CollectionRef(packages, offset);
        if (!CheckRef(archive, ref, 22)) { return false; }
        candidate.purchasedPackages.push_back(ref);
    }
    const auto &purchases = archive.records[2].payload;
    for (std::size_t offset = 4; offset < purchases.size(); offset += 10) {
        const GameObjectRef ref = CollectionRef(purchases, offset);
        const unsigned type = purchases[offset + 5], quantity = purchases[offset + 8];
        if (!CheckRef(archive, ref, type)) { return false; }
        if (quantity == 0) { continue; }
        if (type == 2 || type == 6) { candidate.Grant(type, ref); }
        if (type == 17) { candidate.AddPowerup(ref, quantity); }
    }
    candidate.clearedWaves.fill(0);
    for (auto &bits : candidate.perfectedWaves) { bits.reset(); }
    const auto &missions = archive.records[3].payload;
    for (std::size_t offset = 4; offset < missions.size(); offset += 524) {
        const GameObjectRef ref = CollectionRef(missions, offset);
        if (missions[offset + 5] != 7 || !CheckRef(archive, ref, 7)) { return false; }
        for (unsigned slot = 0; slot < archive.survivalLevels.size(); ++slot) {
            if (!SameRef(ref, archive.survivalLevels[slot])) { continue; }
            CArrayInputStream entry(missions.data() + offset + 8, 516);
            candidate.clearedWaves[slot] = entry.ReadUInt16();
            for (unsigned wave = 0; wave < candidate.perfectedWaves[slot].size(); ++wave) {
                candidate.perfectedWaves[slot].set(wave, (missions[offset + 12 + wave / 8] & (1u << (wave % 8))) != 0);
            }
        }
    }
    std::copy(archive.records[7].payload.begin(), archive.records[7].payload.end(), candidate.tutorialSeen.begin());
    if (!candidate.refinery.ReadSavedData(archive.records[8].payload)) { return false; }
    CDailyBonusTracking::ReadProfileData(candidate, archive.records[9].payload);
    candidate.statistics.fill(0);
    const auto &statistics = archive.records[10].payload;
    const unsigned statCount = Get32(statistics, 0);
    for (unsigned index = 0; index < std::min(statCount, static_cast<unsigned>(candidate.statistics.size())); ++index) {
        candidate.statistics[index] = Get32(statistics, 4 + index * 4);
    }
    candidate.stat42Bits = candidate.statistics[42];
    candidate.weaponMastery.clear();
    const auto &mastery = archive.records[13].payload;
    for (std::size_t offset = 4; offset < mastery.size(); offset += 14) {
        const GameObjectRef ref = CollectionRef(mastery, offset);
        if (mastery[offset + 5] != 6 || !CheckRef(archive, ref, 6)) { return false; }
        candidate.weaponMastery.push_back({ref, Get32(mastery, offset + 10)});
    }
    candidate.nativeArchive = std::move(archive);
    profile = std::move(candidate);
    std::printf("[native-profile] loaded xp=%llu coins=%llu warbucks=%llu inventory=%zu brother=%u slot=%u first-launch=%u\n",
        profile.experience, profile.coins, profile.warbucks, profile.inventory.size(), profile.playerBrother,
        profile.activeWeaponSlot, profile.firstLaunch);
    return true;
}

}
