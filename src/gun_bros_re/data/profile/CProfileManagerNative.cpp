/** Native DataStore responsibilities; see saves/GB_save_profile.bt and save_payloads.bt. */
#include "gun_bros_re/data/profile/CProfileManagerStorage.h"
#include "gun_bros_re/gameplay/weapon/CGun.h"
#include "gun_bros_re/data/profile/CDailyBonusTracking.h"
#include <algorithm>
#include <cstdio>
using namespace ProfileStorageDetail;

namespace {
bool InitializeNativeProfile(CResTOCManager &toc, CGunBros &tables, CProfileManager &profile, CProfileManager::Archive archive) {
    if (!ApplyArchive(profile, std::move(archive))) { return false; }
    if (!InitializeContentSeen(*profile.nativeArchive)) { return false; }
    if (profile.firstLaunch) {
        // AcquireDefaultGear :79911 visits the core STORE entries, then ensures
        // both configured guns are present. It does not invent an inventory list.
        const int core = toc.GetCorePackIndex();
        const unsigned hash = toc.GetPack(core)->GetPackHash();
        const unsigned count = tables.GetObjectPack(core).GetObjectCount(ZGameSection::StoreItem);
        for (unsigned ordinal = 0; ordinal < count; ++ordinal) {
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(hash, ZGameSection::StoreItem, ordinal, bytes)) { return false; }
            CArrayInputStream input(bytes);
            CStoreItem item;
            if (!item.Init(input) || input.Available() != 0) { return false; }
            const auto acquired = profile.AcquireItem(item, 1);
            if (acquired == CProfileManager::PurchaseResult::Unsupported) {
                std::printf("[native-profile] unsupported default item hash=%u ordinal=%u\n", hash, ordinal);
                return false;
            }
            // AcquireDefaultGear marks the first referenced object, not the
            // STORE ordinal, after attempting the original acquisition.
            if (!item.objects.empty() && !MarkContentSeen(*profile.nativeArchive, item.objects[0])) { return false; }
        }
        for (const auto &gun : profile.configuration.guns) { profile.Grant(6, gun); }
    }
    return true;
}
}
bool CProfileManager::CreateTransient(CResTOCManager &toc, CGunBros &tables) {
    CProfileManager &profile = *this;

    CProfileManager::Archive archive;
    if (!CProfileManager::CreateArchive(toc, tables, archive)) { return false; }
    return InitializeNativeProfile(toc, tables, profile, std::move(archive));
}

bool CProfileManager::LoadNative(CResTOCManager &toc, CGunBros &tables, const std::filesystem::path &directory, const std::filesystem::path &sourceDirectory) {
    CProfileManager &profile = *this;

    CProfileManager::Archive archive;
    if (!CProfileManager::CreateArchive(toc, tables, archive)) { return false; }
    if (std::filesystem::exists(directory)) {
        if (!ReadArchive(directory, archive)) { return false; }
    } else if (!sourceDirectory.empty() && std::filesystem::exists(sourceDirectory)) {
        if (!ReadArchive(sourceDirectory, archive)) { return false; }
        archive.importDirectory = std::filesystem::weakly_canonical(sourceDirectory);
        std::printf("[native-profile] import source=%s destination=%s\n", sourceDirectory.string().c_str(), directory.string().c_str());
    } else { std::printf("[native-profile] new offline profile from original constructors\n"); }
    if (!InitializeNativeProfile(toc, tables, profile, std::move(archive))) { return false; }
    return (profile).SaveNative(directory);
}

bool CProfileManager::ReloadNative(const std::filesystem::path &directory) {
    CProfileManager &profile = *this;

    if (!profile.nativeArchive) { return false; }
    CProfileManager::Archive archive;
    if (!CProfileManager::CreateArchive(*profile.nativeArchive->toc, *profile.nativeArchive->tables, archive) ||
        !ReadArchive(directory, archive)) { return false; }
    archive.importDirectory = profile.nativeArchive->importDirectory;
    return ApplyArchive(profile, std::move(archive));
}

bool CProfileManager::SaveNative(const std::filesystem::path &directory) const {
    const CProfileManager &profile = *this;

    if (!profile.nativeArchive || profile.warbucks > UINT32_MAX || profile.activeWeaponSlot > 1 || profile.playerBrother > 1) { return false; }
    CProfileManager::Archive archive = *profile.nativeArchive;
    archive.options = profile.options;
    archive.options.SetSoundEnabled(profile.soundEnabled);
    archive.options.SetMusicEnabled(profile.musicEnabled);
    CPlayerProgress::WriteProfileData(profile, archive.records[0].payload, archive.progression, archive.loadedExperience);
    auto &equipment = archive.records[1].payload;
    for (unsigned slot = 0; slot < 2; ++slot) {
        const GameObjectRef &gun = profile.configuration.guns[slot];
        if (!SameRef(MemoryRef(equipment, slot * 8), gun)) {
            std::vector<std::uint8_t> bytes;
            if (!archive.tables->ReadSectionResource(gun.packHash, ZGameSection::Gun, gun.localIndex, bytes)) { return false; }
            CArrayInputStream input(bytes);
            CGun::Template weapon;
            if (!weapon.Init(input) || input.Available() != 0 || !WriteMemoryRef(archive, equipment, slot * 8, gun) ||
                !WriteMemoryRef(archive, equipment, 16 + slot * 8, weapon.GetBulletRef())) { return false; }
        }
    }
    for (unsigned slot = 0; slot < 4; ++slot) {
        if (!WriteMemoryRef(archive, equipment, 32 + slot * 8, profile.configuration.armor[slot])) { return false; }
    }
    equipment[64] = static_cast<std::uint8_t>(profile.activeWeaponSlot);
    // CPowerUpSelector::OptionEquip :184626 writes configuration mem+128/129.
    equipment[116] = profile.configuration.powerups[0];
    equipment[117] = profile.configuration.powerups[1];
    equipment[65] = static_cast<std::uint8_t>(profile.playerBrother);
    auto &purchases = archive.records[2].payload;
    for (const auto &item : profile.inventory) {
        if (!CheckRef(archive, item.object, item.type)) { return false; }
        const std::size_t offset = EnsureRecord(purchases, 10, item.type, item.object);
        if (purchases[offset + 8] == 0) { purchases[offset + 8] = 1; purchases[offset + 6] = 1; }
    }
    for (const auto &item : profile.powerups) {
        if (item.count > 255 || !CheckRef(archive, item.resource, 17)) { return false; }
        const std::size_t offset = EnsureRecord(purchases, 10, 17, item.resource);
        if (purchases[offset + 8] != item.count) {
            purchases[offset + 8] = static_cast<std::uint8_t>(item.count);
            purchases[offset + 6] = 1;
        }
    }
    auto &missions = archive.records[3].payload;
    for (unsigned slot = 0; slot < archive.survivalLevels.size(); ++slot) {
        const auto &ref = archive.survivalLevels[slot];
        std::size_t offset = FindRecord(missions, 524, 7, ref);
        if (offset == missions.size() && profile.clearedWaves[slot] == 0 && profile.perfectedWaves[slot].none()) { continue; }
        if (profile.clearedWaves[slot] > UINT16_MAX) { return false; }
        offset = EnsureRecord(missions, 524, 7, ref);
        const auto previous = missions;
        Put16(missions, offset + 8, profile.clearedWaves[slot]);
        unsigned maxPerfect = missions[offset + 10] | (missions[offset + 11] << 8);
        for (unsigned wave = 0; wave < profile.perfectedWaves[slot].size(); ++wave) {
            const auto mask = static_cast<std::uint8_t>(1u << (wave % 8));
            if (profile.perfectedWaves[slot].test(wave)) {
                missions[offset + 12 + wave / 8] |= mask;
                maxPerfect = std::max(maxPerfect, wave);
            } else { missions[offset + 12 + wave / 8] &= static_cast<std::uint8_t>(~mask); }
        }
        Put16(missions, offset + 10, maxPerfect);
        if (missions != previous) { missions[offset + 6] = 1; }
    }
    archive.records[7].payload.assign(profile.tutorialSeen.begin(), profile.tutorialSeen.end());
    if (!profile.refinery.WriteSavedData(archive.records[8].payload)) { return false; }
    CDailyBonusTracking::WriteProfileData(profile, archive.records[9].payload);
    auto &statistics = archive.records[10].payload;
    const unsigned statCount = Get32(statistics, 0);
    for (unsigned index = 0; index < std::min(statCount, static_cast<unsigned>(profile.statistics.size())); ++index) {
        Put32(statistics, 4 + index * 4, profile.statistics[index]);
    }
    if (statCount > 42) { Put32(statistics, 4 + 42 * 4, profile.stat42Bits); }
    auto &mastery = archive.records[13].payload;
    for (const auto &weapon : profile.weaponMastery) {
        if (!CheckRef(archive, weapon.resource, 6)) { return false; }
        const std::size_t offset = EnsureRecord(mastery, 14, 6, weapon.resource);
        if (Get32(mastery, offset + 10) != weapon.experience) { Put32(mastery, offset + 10, weapon.experience); mastery[offset + 6] = 1; }
    }
    auto &packages = archive.records[18].payload;
    for (const GameObjectRef &ref : profile.purchasedPackages) {
        if (!CheckRef(archive, ref, 22)) { return false; }
        // Existing values and padding survive; new AddItem values are zero.
        EnsureRecord(packages, 14, 22, ref);
    }
    for (unsigned index = 0; index < archive.records.size(); ++index) {
        if (archive.records[index].payload != archive.records[index].originalPayload && archive.status[index] != 4) { archive.status[index] = 1; }
    }
    return CProfileManager::WriteArchive(directory, archive);
}

/** CGame::OnWaveCleared :76192 applies to both survival and horde LEVEL refs.
 * Disk layout: saves/save_payloads.bt MissionWaveSnapshot, 524-byte records. */
bool CProfileManager::RecordMissionWaves(const GameObjectRef &level, unsigned waveProgress, const std::vector<bool> &perfectResults) {
    CProfileManager &profile = *this;

    if (!profile.nativeArchive || level.IsNull() || !CheckRef(*profile.nativeArchive, level, 7) ||
        waveProgress > UINT16_MAX || perfectResults.size() > waveProgress) { return false; }
    if (perfectResults.empty()) { return true; }
    auto &records = profile.nativeArchive->records[3].payload;
    const std::size_t offset = EnsureRecord(records, 524, 7, level);
    const unsigned previous = records[offset + 8] | (records[offset + 9] << 8);
    unsigned maximumPerfect = records[offset + 10] | (records[offset + 11] << 8);
    bool changed = false;
    if (waveProgress > previous) { Put16(records, offset + 8, waveProgress); changed = true; }
    const unsigned firstWave = waveProgress - static_cast<unsigned>(perfectResults.size());
    for (unsigned index = 0; index < perfectResults.size(); ++index) {
        const unsigned wave = firstWave + index;
        if (!perfectResults[index] || wave >= 4096) { continue; }
        const auto mask = static_cast<std::uint8_t>(1u << (wave % 8));
        if ((records[offset + 12 + wave / 8] & mask) == 0) {
            records[offset + 12 + wave / 8] |= mask;
            maximumPerfect = std::max(maximumPerfect, wave);
            changed = true;
        }
    }
    Put16(records, offset + 10, maximumPerfect);
    if (changed) { records[offset + 6] = 1; }
    return true;
}

/** CMissionHighScore::AddScore :233360; death of type 2 is a result too
 * (CGame::OnMissionFailure :76278). Preserve lower scores and unknown padding. */
bool CProfileManager::RecordMissionScore(const GameObjectRef &mission, unsigned score) {
    CProfileManager &profile = *this;

    if (!profile.nativeArchive || mission.IsNull() || !CheckRef(*profile.nativeArchive, mission, 9)) { return false; }
    auto &records = profile.nativeArchive->records[16].payload;
    const std::size_t offset = EnsureRecord(records, 14, 9, mission);
    if (score > Get32(records, offset + 10)) {
        Put32(records, offset + 10, score);
        records[offset + 6] = 1;
    }
    return true;
}

bool CProfileManager::ImportNative(CResTOCManager &toc, CGunBros &tables, const std::filesystem::path &source) {
    Archive archive;
    if (!CreateArchive(toc, tables, archive) || !ReadArchive(source, archive)) { return false; }
    archive.importDirectory = std::filesystem::weakly_canonical(source);
    CProfileManager candidate = *this;
    if (!InitializeNativeProfile(toc, tables, candidate, std::move(archive))) { return false; }
    *this = std::move(candidate);
    return true;
}
