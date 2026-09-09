/** @file NativeProfile.cpp
 * @brief Original serialized layouts; Windows file replacement is the host boundary.
 * Sources: saves/GB_save_profile.bt, save_payloads.bt and CProfileManager :202880/:203163.
 */
#define NOMINMAX
#include "runtime/NativeProfile.h"
#include "runtime/OriginalProfile.h"
#include "runtime/StoreCatalog.h"
#include "runtime/MapScene.h"
#include "runtime/SurvivalGameContext.h"
#include "runtime/PlanetCatalog.h"
#include "runtime/MissionCatalog.h"
#include "gun_bros/CLevel.h"
#include "gun_bros/Planet.h"
#include "gun_bros/Mission.h"
#include "gun_bros/CProfileManager.h"
#include "gun_bros/CGun.h"
#include "gun_bros/CDailyBonusTracking.h"
#include "engine/CCrc32.h"
#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <chrono>
#include <cmath>
#include <cstring>
#include <random>

namespace {
constexpr unsigned kFirstStore = 1000;
constexpr unsigned kLastStore = 1018;
constexpr unsigned kUnregisteredStore = 1015;
constexpr unsigned kBlockSize = 512;
constexpr unsigned kWrapperSize = 24;
constexpr unsigned kStatisticCount = 47; // CPlayerStatistics::ResetData :221159, 0xBC bytes.
constexpr unsigned kChallengeSlots = 8; // CChallengeProgressData::Save :298860.
constexpr unsigned kFriendSlots = 30;

void Put16(std::vector<std::uint8_t> &bytes, std::size_t offset, unsigned value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void Put32(std::vector<std::uint8_t> &bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned index = 0; index < 4; ++index) { bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8)); }
}

bool ReadBytes(const std::filesystem::path &path, std::vector<std::uint8_t> &bytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input) { return false; }
    bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return !input.bad();
}

/** CContentTracker::UserData::Init :225636, PerPackData::Serialize :225374. */
bool InitializeContentSeen(NativeProfileArchive &archive) {
    auto &bytes = archive.records[14].payload;
    if (bytes.size() != 1 || bytes[0] != 0) { return true; }
    const unsigned packs = archive.toc->GetPackCount();
    if (packs > 255) { return false; }
    bytes[0] = static_cast<std::uint8_t>(packs);
    const GameSection sections[] = {GameSection::Armor, GameSection::Gun, GameSection::Planet, GameSection::Powerup};
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
bool MarkContentSeen(NativeProfileArchive &archive, const GameObjectTypeRef &object) {
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

/** Determine the real end before padding; odd payloads cannot use a size guess. */
bool PayloadSize(unsigned id, const std::vector<std::uint8_t> &bytes, std::size_t &size) {
    CArrayInputStream stream(bytes);
    switch (id) {
    case 1000: stream.Skip(48); break; // CPlayerProgress::LoadFromDisk :194502.
    case 1001: stream.Skip(120); break; // CPlayerConfiguration::LoadFromDisk :170602.
    case 1002: case 1003: case 1004: case 1005: case 1013: case 1016: case 1018: {
        unsigned stride = 14;
        if (id == 1002) { stride = 10; }
        if (id == 1003) { stride = 524; }
        if (id == 1004) { stride = 8; }
        const unsigned count = stream.ReadUInt32();
        if (count > stream.Available() / stride) { return false; }
        stream.Skip(static_cast<std::size_t>(count) * stride);
        break;
    }
    case 1006: {
        stream.ReadUInt16();
        const unsigned count = stream.ReadUInt16();
        if (count > 20) { return false; }
        stream.Skip(count * 8);
        break;
    }
    case 1007: stream.Skip(22); break; // CTutorialManager::SaveToDisk :210615.
    case 1008: stream.Skip(340); break; // CRefinementManager::SaveToDisk :177115.
    case 1009: stream.Skip(12); break;
    case 1010: {
        const unsigned count = stream.ReadUInt32();
        if (count > stream.Available() / 4) { return false; }
        stream.Skip(count * 4);
        break;
    }
    case 1011: stream.Skip(4); break;
    case 1012: stream.Skip(1052); break;
    case 1014: {
        const unsigned count = stream.ReadUInt8();
        for (unsigned pack = 0; pack < count; ++pack) {
            stream.ReadUInt32();
            const unsigned length = stream.ReadUInt32();
            const std::size_t start = stream.Position();
            const unsigned categories = stream.ReadUInt8();
            if (categories > 4) { return false; }
            for (unsigned category = 0; category < categories; ++category) {
                if (stream.ReadUInt8() >= 4) { return false; }
                const unsigned bits = stream.ReadUInt8();
                stream.Skip((bits + 7) / 8);
            }
            if (stream.Position() - start != length) { return false; }
        }
        break;
    }
    case 1017: {
        stream.Skip(6);
        unsigned count = stream.ReadUInt8();
        if (count > kChallengeSlots) { return false; }
        stream.Skip(count);
        count = stream.ReadUInt8();
        if (count > kChallengeSlots) { return false; }
        stream.Skip(count);
        count = stream.ReadUInt8();
        if (count > kChallengeSlots) { return false; }
        for (unsigned group = 0; group < count; ++group) {
            const unsigned friends = stream.ReadUInt8();
            if (friends > kFriendSlots) { return false; }
            stream.Skip(friends * 8);
        }
        count = stream.ReadUInt8();
        if (count > kChallengeSlots) { return false; }
        stream.Skip(count * 20);
        break;
    }
    default: return false;
    }
    size = stream.Position();
    return !stream.Overran();
}

/** Native random fill consists of GetRand(0x7FFF) words and zero remainder. */
void FillPadding(std::vector<std::uint8_t> &bytes, std::size_t offset, std::size_t length, std::mt19937 &random) {
    for (std::size_t index = 0; index + 4 <= length; index += 4) { Put32(bytes, offset + index, random() % 0x7FFF); }
}

std::vector<std::uint8_t> EncodeRecord(const NativeProfileRecord &record) {
    const std::size_t payloadSize = record.payload.size();
    const std::size_t aligned = payloadSize + kBlockSize - payloadSize % kBlockSize;
    const std::size_t prefix = (aligned >> 1) - (payloadSize >> 1);
    std::vector<std::uint8_t> bytes(aligned + kWrapperSize, 0);
    Put32(bytes, 0, record.version);
    Put32(bytes, 8, static_cast<unsigned>(prefix));
    std::random_device seed;
    std::mt19937 random(seed());
    FillPadding(bytes, 12, prefix, random);
    std::copy(record.payload.begin(), record.payload.end(), bytes.begin() + 12 + prefix);
    const std::size_t suffixStart = 12 + prefix + payloadSize;
    FillPadding(bytes, suffixStart, bytes.size() - 8 - suffixStart, random);
    Put32(bytes, bytes.size() - 8, static_cast<std::uint32_t>(record.owner));
    Put32(bytes, bytes.size() - 4, CCrc32::Crc32(bytes.data(), bytes.size() - 4));
    return bytes;
}

bool ReplaceFile(const std::filesystem::path &path, const std::vector<std::uint8_t> &bytes) {
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) { return false; }
    output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    output.close();
    if (!output || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { return false; }
    return true;
}
}

bool ReadNativeProfileRecord(const std::filesystem::path &path, unsigned id, NativeProfileRecord &record) {
    OriginalDataStore envelope;
    unsigned version = 0;
    if (id == 1008) { version = 1; }
    if (id == 1017) { version = 5; }
    if (!ReadOriginalDataStore(path, envelope) || !envelope.crcMatches || envelope.version != version) {
        std::printf("[native-profile] invalid envelope/version id=%u file=%s\n", id, path.string().c_str());
        return false;
    }
    std::size_t size = 0;
    if (!PayloadSize(id, envelope.data, size) || (size & ~std::size_t(1)) != envelope.minimumSize) {
        std::printf("[native-profile] invalid payload id=%u file=%s\n", id, path.string().c_str());
        return false;
    }
    NativeProfileRecord candidate;
    candidate.version = envelope.version;
    candidate.owner = envelope.clientId;
    candidate.payload.assign(envelope.data.begin(), envelope.data.begin() + size);
    candidate.originalPayload = candidate.payload;
    if (!ReadBytes(path, candidate.originalFile)) { return false; }
    record = std::move(candidate);
    return true;
}

bool WriteNativeProfileArchive(const std::filesystem::path &directory, const NativeProfileArchive &archive) {
    const auto destination = std::filesystem::weakly_canonical(directory);
    const auto source = std::filesystem::weakly_canonical(std::filesystem::path(ASSET_ROOT) / "saves");
    const auto relative = destination.lexically_relative(source);
    if (destination == source || (!relative.empty() && *relative.begin() != "..")) {
        std::printf("[native-profile] source archive is read-only: %s\n", directory.string().c_str());
        return false;
    }
    // Validate every payload before touching any file. Per-file replacement is
    // atomic; the original format has no transaction across all client records.
    for (unsigned id = kFirstStore; id <= kLastStore; ++id) {
        if (id == kUnregisteredStore) { continue; }
        const auto &record = archive.records[id - kFirstStore];
        std::size_t size = 0;
        if (!PayloadSize(id, record.payload, size) || size != record.payload.size() || record.owner != archive.records[0].owner) {
            std::printf("[native-profile] invalid output record id=%u\n", id);
            return false;
        }
    }
    std::filesystem::create_directories(destination);
    for (unsigned id = kFirstStore; id <= kLastStore; ++id) {
        if (id == kUnregisteredStore) { continue; }
        const auto &record = archive.records[id - kFirstStore];
        std::vector<std::uint8_t> bytes;
        if (!record.originalFile.empty() && record.payload == record.originalPayload) { bytes = record.originalFile; }
        else { bytes = EncodeRecord(record); }
        const auto path = destination / (std::to_string(record.owner) + "_" + std::to_string(id));
        if (!ReplaceFile(path, bytes)) { return false; }
    }
    const std::vector<std::uint8_t> status(archive.status.begin(), archive.status.end());
    if (!ReplaceFile(destination / (std::to_string(archive.records[0].owner) + "_PDST"), status)) { return false; }
    // COptionsMgr::Write :51400 stores CRC followed by mem+16..47 in "p".
    std::vector<std::uint8_t> options(36, 0);
    std::copy(archive.options.payload.begin(), archive.options.payload.end(), options.begin() + 4);
    Put32(options, 0, CCrc32::Crc32(options.data() + 4, 32));
    return ReplaceFile(destination / "p", options);
}

bool CreateNativeProfileArchive(CResTOCManager &toc, PackTables &tables, NativeProfileArchive &archive) {
    NativeProfileArchive candidate;
    candidate.toc = &toc;
    candidate.tables = &tables;
    candidate.status.fill(4); // CProfileManager::Initialize :203931.
    if (!LoadPlayerProgress(toc, tables, candidate.progression)) { return false; }
    for (unsigned id = kFirstStore; id <= kLastStore; ++id) {
        if (id == kUnregisteredStore) { continue; }
        auto &record = candidate.records[id - kFirstStore];
        // Empty collection count, not invented inventory or unlocked progress.
        record.payload.resize(4, 0);
    }
    auto &progress = candidate.records[0].payload;
    progress.resize(48, 0);
    progress[0] = 1;
    Put16(progress, 32, 1);
    progress[46] = 1; // CPlayerProgress::ResetData :194379, first launch/level/push.

    auto &equipment = candidate.records[1].payload;
    equipment.resize(120, 0);
    equipment[116] = 255;
    equipment[117] = 255; // CPlayerConfiguration constructor :171487.
    const int core = toc.GetCorePackIndex();
    const int pack5 = toc.GetPackIndexFromName("pack5");
    if (core < 0 || pack5 < 0) { return false; }
    // CPlayerConfiguration::Reset :171865 explicitly chooses these references.
    // The meshes, bullets and attributes themselves still come from the BIG entries.
    const unsigned ordinals[] = {0, 4, 0, 45, 2, 1, 0, 255};
    for (unsigned slot = 0; slot < 8; ++slot) {
        int pack = core;
        if (slot == 1 || slot == 3) { pack = pack5; }
        const unsigned hash = toc.GetPack(pack)->GetPackHash();
        Put32(equipment, slot * 8, hash);
        Put16(equipment, slot * 8 + 4, pack);
        equipment[slot * 8 + 6] = static_cast<std::uint8_t>(ordinals[slot]);
        if (ordinals[slot] == 255) { continue; }
        GameSection section = GameSection::Armor;
        if (slot < 2) { section = GameSection::Gun; }
        else if (slot < 4) { section = GameSection::Bullet; }
        std::vector<std::uint8_t> bytes;
        if (!tables.ReadSectionResource(hash, section, ordinals[slot], bytes)) { return false; }
    }
    candidate.records[7].payload.resize(22, 0); // CTutorialManager :210464/:210615.
    candidate.records[8].version = 1;
    candidate.records[8].payload.resize(340, 0); // CRefinementManager :177641.
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return false; }
    CRefinementManager refinery;
    refinery.Bind(refinement); // Init :178415 uses the BIG prices/gates to open free slots.
    for (unsigned slot = 0; slot < refinery.slots.size(); ++slot) {
        Put32(candidate.records[8].payload, 4 + slot * 28, refinery.slots[slot].state);
    }
    candidate.records[9].payload.resize(12, 0); // CDailyBonusTracking :209116.
    candidate.records[10].payload.resize(4 + kStatisticCount * 4, 0);
    Put32(candidate.records[10].payload, 0, kStatisticCount);
    candidate.records[12].payload.resize(1052, 0); // COfferDataManager :222200.
    candidate.records[14].payload.assign(1, 0); // CContentTracker::UserData::Reset :225050.

    auto &challenges = candidate.records[17];
    challenges.version = 5;
    challenges.payload.assign(6, 0);
    for (unsigned group = 0; group < 2; ++group) {
        challenges.payload.push_back(kChallengeSlots);
        challenges.payload.insert(challenges.payload.end(), kChallengeSlots, 0);
    }
    challenges.payload.push_back(kChallengeSlots);
    for (unsigned slot = 0; slot < kChallengeSlots; ++slot) {
        challenges.payload.push_back(kFriendSlots);
        for (unsigned friendSlot = 0; friendSlot < kFriendSlots; ++friendSlot) {
            const std::size_t offset = challenges.payload.size();
            challenges.payload.resize(offset + 8, 0);
            Put32(challenges.payload, offset, UINT32_MAX); // Reset :297936 empty client=-1.
        }
    }
    challenges.payload.push_back(kChallengeSlots);
    challenges.payload.insert(challenges.payload.end(), kChallengeSlots * 20, 0);

    // Original CMenuMission::Bind :162800 orders planets by mapSlot; retail
    // Mission.type=1 separates the four survival entries from tutorial/BOKOR.
    // The host's four-element progress array is this filtered order, not mapSlot.
    std::map<unsigned, GameObjectRef> retailLevels;
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const unsigned hash = toc.GetPack(packIndex)->GetPackHash();
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(GameSection::Planet);
        for (unsigned index = 0; index < count; ++index) {
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(hash, GameSection::Planet, index, bytes)) { return false; }
            CArrayInputStream input(bytes);
            Planet planet;
            if (!planet.Init(input) || input.Available() != 0) { return false; }
            for (const GameObjectRef &missionRef : planet.missions) {
                if (!tables.ReadSectionResource(missionRef.packHash, GameSection::Mission, missionRef.localIndex, bytes)) { return false; }
                CArrayInputStream missionInput(bytes);
                Mission mission;
                if (!mission.Init(missionInput) || missionInput.Available() != 0) { return false; }
                if (mission.type != 1) { continue; }
                GameObjectRef &level = retailLevels[planet.mapSlot];
                if (!level.IsNull() && (level.packHash != mission.level.packHash || level.localIndex != mission.level.localIndex)) {
                    std::printf("[native-profile] multiple levels in retail slot=%u\n", planet.mapSlot);
                    return false;
                }
                level = mission.level;
            }
        }
    }
    if (retailLevels.size() != candidate.survivalLevels.size()) {
        std::printf("[native-profile] retail level count=%zu unsupported by current host\n", retailLevels.size());
        return false;
    }
    unsigned slot = 0;
    for (const auto &entry : retailLevels) {
        candidate.survivalLevels[slot++] = entry.second;
        std::printf("[native-profile] retail mapSlot=%u level=%u:%u\n", entry.first, entry.second.packHash, entry.second.localIndex);
    }
    archive = std::move(candidate);
    return true;
}

namespace {
std::uint64_t Get64(CArrayInputStream &input) {
    const std::uint64_t low = input.ReadUInt32();
    return low | (static_cast<std::uint64_t>(input.ReadUInt32()) << 32);
}

void Put64(std::vector<std::uint8_t> &bytes, std::size_t offset, std::uint64_t value) {
    Put32(bytes, offset, static_cast<std::uint32_t>(value));
    Put32(bytes, offset + 4, static_cast<std::uint32_t>(value >> 32));
}

std::uint32_t Get32(const std::vector<std::uint8_t> &bytes, std::size_t offset) {
    CArrayInputStream input(bytes.data() + offset, bytes.size() - offset);
    return input.ReadUInt32();
}

GameObjectRef MemoryRef(const std::vector<std::uint8_t> &bytes, unsigned offset) {
    GameObjectRef ref;
    ref.packHash = Get32(bytes, offset);
    ref.localIndex = bytes[offset + 6];
    if (ref.localIndex == 255) { return {}; }
    return ref;
}

bool SameRef(const GameObjectRef &first, const GameObjectRef &second) {
    return first.packHash == second.packHash && first.localIndex == second.localIndex;
}

GameObjectRef CollectionRef(const std::vector<std::uint8_t> &bytes, std::size_t offset) {
    GameObjectRef ref;
    ref.packHash = Get32(bytes, offset);
    ref.localIndex = bytes[offset + 4];
    return ref;
}

bool CheckRef(const NativeProfileArchive &archive, const GameObjectRef &ref, unsigned type) {
    if (ref.IsNull()) { return true; }
    std::vector<std::uint8_t> bytes;
    if (!archive.tables->ReadSectionResource(ref.packHash, static_cast<GameSection>(type + 1), ref.localIndex, bytes)) {
        std::printf("[native-profile] unresolved ref hash=%u type=%u ordinal=%u\n", ref.packHash, type, ref.localIndex);
        return false;
    }
    return true;
}

/** Only changed references are reconciled. Untouched native alignment survives. */
bool WriteMemoryRef(NativeProfileArchive &archive, std::vector<std::uint8_t> &bytes, unsigned offset, const GameObjectRef &ref) {
    if (SameRef(MemoryRef(bytes, offset), ref)) { return true; }
    if (ref.IsNull()) { bytes[offset + 6] = 255; return true; }
    const int pack = archive.toc->GetPackIndexFromHash(ref.packHash);
    if (pack < 0) { return false; }
    Put32(bytes, offset, ref.packHash);
    Put16(bytes, offset + 4, pack);
    bytes[offset + 6] = ref.localIndex;
    return true;
}

std::size_t FindRecord(const std::vector<std::uint8_t> &bytes, unsigned stride, unsigned type, const GameObjectRef &ref) {
    for (std::size_t offset = 4; offset < bytes.size(); offset += stride) {
        if (bytes[offset + 5] == type && SameRef(CollectionRef(bytes, offset), ref)) { return offset; }
    }
    return bytes.size();
}

std::size_t EnsureRecord(std::vector<std::uint8_t> &bytes, unsigned stride, unsigned type, const GameObjectRef &ref) {
    const std::size_t offset = FindRecord(bytes, stride, type, ref);
    if (offset != bytes.size()) { return offset; }
    const unsigned count = Get32(bytes, 0);
    bytes.resize(offset + stride, 0);
    Put32(bytes, 0, count + 1);
    Put32(bytes, offset, ref.packHash);
    bytes[offset + 4] = ref.localIndex;
    bytes[offset + 5] = static_cast<std::uint8_t>(type);
    bytes[offset + 6] = 1; // Collection::WriteSavedData :81309 changes dirty 0 to local 1.
    return offset;
}

/** Decode proven fields without replacing unknown values or unsupported records. */
bool ApplyArchive(CProfileManager &profile, NativeProfileArchive archive) {
    CProfileManager candidate = profile;
    candidate.options = archive.options;
    candidate.soundEnabled = candidate.options.SoundEnabled();
    candidate.musicEnabled = candidate.options.MusicEnabled();
    candidate.brotherEnabled = candidate.options.AutoBro() != 0;
    CArrayInputStream progress(archive.records[0].payload);
    // EnterShell :79673 selects first-game flow from this original flag.
    candidate.firstLaunch = progress.ReadUInt8() != 0;
    candidate.pushChallenges = archive.records[0].payload[46] != 0;
    candidate.tutorialCompleted = false;
    candidate.tutorialSteps = 0; // Host trace, not the 22 original menu-tip flags.
    progress.Skip(3);
    candidate.xplodium = Get64(progress);
    candidate.coins = Get64(progress);
    candidate.warbucks = progress.ReadUInt32();
    candidate.experience = Get64(progress);
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
    candidate.playerBrother = equipment[65];
    if (candidate.activeWeaponSlot > 1 || candidate.playerBrother > 1) {
        std::printf("[native-profile] unsupported player selection slot=%u brother=%u\n", candidate.activeWeaponSlot, candidate.playerBrother);
        return false;
    }
    candidate.inventory.clear();
    candidate.powerups.clear();
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
    const auto &refinement = archive.records[8].payload;
    const unsigned checkpoint = Get32(refinement, 0);
    for (unsigned slot = 0; slot < candidate.refinery.slots.size(); ++slot) {
        CArrayInputStream entry(refinement.data() + 4 + slot * 28, 28);
        auto &value = candidate.refinery.slots[slot];
        value.state = entry.ReadUInt32();
        const unsigned rawEfficiency = entry.ReadUInt32();
        std::memcpy(&value.efficiency, &rawEfficiency, 4);
        const int remainingMs = entry.ReadInt32();
        value.startTimeSeconds = entry.ReadUInt32();
        value.totalDurationMs = entry.ReadInt32();
        value.amount = Get64(entry);
        value.finishTimeMs = 0;
        value.finishTime = 0;
        if (value.state == 2) {
            value.finishTimeMs = static_cast<std::int64_t>(checkpoint) * 1000 + remainingMs;
            value.finishTime = (value.finishTimeMs + 999) / 1000;
        }
        if (value.state > 3 || !std::isfinite(value.efficiency)) { return false; }
    }
    candidate.dailyLastLaunchSeconds = Get32(archive.records[9].payload, 0);
    candidate.dailyConsecutiveSeconds = Get32(archive.records[9].payload, 4);
    candidate.dailyLastCommit = Get32(archive.records[9].payload, 8);
    candidate.dailyConsecutiveDays = candidate.dailyConsecutiveSeconds / 86400 + 1;
    candidate.dailyLastClaimDay = -1; // Never reinterpret the native seconds as a host date.
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

bool ReadArchive(const std::filesystem::path &directory, NativeProfileArchive &archive) {
    const auto optionsPath = directory / "p";
    if (std::filesystem::exists(optionsPath)) {
        std::vector<std::uint8_t> options;
        if (!ReadBytes(optionsPath, options) || options.size() != 36 ||
            Get32(options, 0) != CCrc32::Crc32(options.data() + 4, 32)) {
            // Keep the source intact. Original Read resets corrupt options;
            // this host reports corruption for diagnosis instead of replacing it.
            std::printf("[native-options] invalid p size or CRC: %s\n", optionsPath.string().c_str());
            return false;
        }
        std::copy(options.begin() + 4, options.end(), archive.options.payload.begin());
    }
    // This reconstruction has one offline client. Never silently select an
    // arbitrary account, .perfect copy, or corrupt record as the current user.
    for (unsigned id = kFirstStore; id <= kLastStore; ++id) {
        if (id == kUnregisteredStore) { continue; }
        const auto path = directory / ("-1_" + std::to_string(id));
        if (!std::filesystem::exists(path)) {
            std::printf("[native-profile] missing client id=%u uses original ResetData\n", id);
            continue;
        }
        if (!ReadNativeProfileRecord(path, id, archive.records[id - kFirstStore]) || archive.records[id - kFirstStore].owner != -1) { return false; }
    }
    const auto statusPath = directory / "-1_PDST";
    if (std::filesystem::exists(statusPath)) {
        std::vector<std::uint8_t> status;
        if (!ReadBytes(statusPath, status) || status.size() != archive.status.size()) { return false; }
        std::copy(status.begin(), status.end(), archive.status.begin());
    }
    return true;
}
}

bool ReloadNativeProfile(CProfileManager &profile, const std::filesystem::path &directory) {
    if (!profile.nativeArchive) { return false; }
    NativeProfileArchive archive;
    if (!CreateNativeProfileArchive(*profile.nativeArchive->toc, *profile.nativeArchive->tables, archive) ||
        !ReadArchive(directory, archive)) { return false; }
    return ApplyArchive(profile, std::move(archive));
}

bool LoadNativeProfile(CResTOCManager &toc, PackTables &tables, CProfileManager &profile,
    const std::filesystem::path &directory, const std::filesystem::path &sourceDirectory) {
    NativeProfileArchive archive;
    if (!CreateNativeProfileArchive(toc, tables, archive)) { return false; }
    if (std::filesystem::exists(directory)) {
        if (!ReadArchive(directory, archive)) { return false; }
    } else if (!sourceDirectory.empty() && std::filesystem::exists(sourceDirectory)) {
        if (!ReadArchive(sourceDirectory, archive)) { return false; }
        std::printf("[native-profile] import source=%s destination=%s\n", sourceDirectory.string().c_str(), directory.string().c_str());
    } else { std::printf("[native-profile] new offline profile from original constructors\n"); }
    if (!ApplyArchive(profile, std::move(archive))) { return false; }
    if (!InitializeContentSeen(*profile.nativeArchive)) { return false; }
    if (profile.firstLaunch) {
        // AcquireDefaultGear :79911 visits the core STORE entries, then ensures
        // both configured guns are present. It does not invent an inventory list.
        const int core = toc.GetCorePackIndex();
        const unsigned hash = toc.GetPack(core)->GetPackHash();
        const unsigned count = tables.GetObjectPack(core).GetObjectCount(GameSection::StoreItem);
        for (unsigned ordinal = 0; ordinal < count; ++ordinal) {
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(hash, GameSection::StoreItem, ordinal, bytes)) { return false; }
            CArrayInputStream input(bytes);
            CStoreItem item;
            if (!item.Init(input) || input.Available() != 0) { return false; }
            const auto acquired = profile.AcquireItem(item, 1);
            if (acquired == PurchaseResult::Unsupported) {
                std::printf("[native-profile] unsupported default item hash=%u ordinal=%u\n", hash, ordinal);
                return false;
            }
            // AcquireDefaultGear marks the first referenced object, not the
            // STORE ordinal, after attempting the original acquisition.
            if (!item.objects.empty() && !MarkContentSeen(*profile.nativeArchive, item.objects[0])) { return false; }
        }
        for (const auto &gun : profile.configuration.guns) { profile.Grant(6, gun); }
    }
    return SaveNativeProfile(profile, directory);
}

/** CGame::OnWaveCleared :76192 applies to both survival and horde LEVEL refs.
 * Disk layout: saves/save_payloads.bt MissionWaveSnapshot, 524-byte records. */
bool RecordNativeMissionWaves(CProfileManager &profile, const GameObjectRef &level,
    unsigned waveProgress, const std::vector<bool> &perfectResults) {
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
bool RecordNativeMissionScore(CProfileManager &profile, const GameObjectRef &mission, unsigned score) {
    if (!profile.nativeArchive || mission.IsNull() || !CheckRef(*profile.nativeArchive, mission, 9)) { return false; }
    auto &records = profile.nativeArchive->records[16].payload;
    const std::size_t offset = EnsureRecord(records, 14, 9, mission);
    if (score > Get32(records, offset + 10)) {
        Put32(records, offset + 10, score);
        records[offset + 6] = 1;
    }
    return true;
}

bool SaveNativeProfile(const CProfileManager &profile, const std::filesystem::path &directory) {
    if (!profile.nativeArchive || profile.warbucks > UINT32_MAX || profile.activeWeaponSlot > 1 || profile.playerBrother > 1) { return false; }
    NativeProfileArchive archive = *profile.nativeArchive;
    archive.options = profile.options;
    archive.options.SetSoundEnabled(profile.soundEnabled);
    archive.options.SetMusicEnabled(profile.musicEnabled);
    auto &progress = archive.records[0].payload;
    progress[0] = profile.firstLaunch;
    progress[46] = profile.pushChallenges;
    Put64(progress, 4, profile.xplodium);
    Put64(progress, 12, profile.coins);
    Put32(progress, 20, static_cast<unsigned>(profile.warbucks));
    Put64(progress, 24, profile.experience);
    if (profile.experience != archive.loadedExperience) {
        CPlayerProgress value;
        value.Bind(archive.progression);
        value.SetExperience(profile.experience);
        Put16(progress, 32, value.GetLevel());
    }
    auto &equipment = archive.records[1].payload;
    for (unsigned slot = 0; slot < 2; ++slot) {
        const GameObjectRef &gun = profile.configuration.guns[slot];
        if (!SameRef(MemoryRef(equipment, slot * 8), gun)) {
            std::vector<std::uint8_t> bytes;
            if (!archive.tables->ReadSectionResource(gun.packHash, GameSection::Gun, gun.localIndex, bytes)) { return false; }
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
    auto &refinement = archive.records[8].payload;
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    bool activeRefinement = false;
    for (unsigned slot = 0; slot < profile.refinery.slots.size(); ++slot) {
        const auto &value = profile.refinery.slots[slot];
        const std::size_t offset = 4 + slot * 28;
        const unsigned oldState = Get32(refinement, offset);
        Put32(refinement, offset, value.state);
        unsigned efficiency = 0;
        std::memcpy(&efficiency, &value.efficiency, 4);
        Put32(refinement, offset + 4, efficiency);
        if (value.state == 2) {
            const std::int64_t remaining = std::max<std::int64_t>(0, value.finishTimeMs - now * 1000);
            if (remaining > INT32_MAX) { return false; }
            Put32(refinement, offset + 8, static_cast<unsigned>(remaining));
            activeRefinement = true;
        } else if (oldState != value.state) { Put32(refinement, offset + 8, 0); }
        Put32(refinement, offset + 12, value.startTimeSeconds);
        Put32(refinement, offset + 16, static_cast<unsigned>(value.totalDurationMs));
        Put64(refinement, offset + 20, value.amount);
    }
    if (activeRefinement) { Put32(refinement, 0, static_cast<unsigned>(now)); }
    Put32(archive.records[9].payload, 0, profile.dailyLastLaunchSeconds);
    Put32(archive.records[9].payload, 4, profile.dailyConsecutiveSeconds);
    Put32(archive.records[9].payload, 8, profile.dailyLastCommit);
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
    for (unsigned index = 0; index < archive.records.size(); ++index) {
        if (archive.records[index].payload != archive.records[index].originalPayload && archive.status[index] != 4) { archive.status[index] = 1; }
    }
    return WriteNativeProfileArchive(directory, archive);
}

int RunNativeProfileCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    NativeProfileArchive defaults;
    if (!CreateNativeProfileArchive(toc, tables, defaults)) { return 1; }
    const std::filesystem::path root = "out/ui-original-2026-09-09/native-profile";
    const auto empty = root / "new";
    if (!WriteNativeProfileArchive(empty, defaults)) { return 1; }
    NativeProfileArchive imported = defaults;
    const auto source = std::filesystem::path(ASSET_ROOT) / "saves";
    unsigned count = 0;
    for (unsigned id = kFirstStore; id <= kLastStore; ++id) {
        if (id == kUnregisteredStore) { continue; }
        const std::string filename = "-1_" + std::to_string(id);
        NativeProfileRecord restored;
        if (!ReadNativeProfileRecord(empty / filename, id, restored) || restored.payload != defaults.records[id - kFirstStore].payload ||
            !ReadNativeProfileRecord(source / filename, id, imported.records[id - kFirstStore])) { return 1; }
        ++count;
    }
    std::vector<std::uint8_t> status;
    if (!ReadBytes(source / "-1_PDST", status) || status.size() != imported.status.size()) { return 1; }
    std::copy(status.begin(), status.end(), imported.status.begin());
    if (!WriteNativeProfileArchive(root / "imported", imported)) { return 1; }
    for (unsigned id = kFirstStore; id <= kLastStore; ++id) {
        if (id == kUnregisteredStore) { continue; }
        std::vector<std::uint8_t> written;
        if (!ReadBytes(root / "imported" / ("-1_" + std::to_string(id)), written) ||
            written != imported.records[id - kFirstStore].originalFile) { return 1; }
    }
    // Corrupt a copy. Loading must fail and retain the last valid record.
    NativeProfileRecord preserved = imported.records[0];
    auto broken = preserved.originalFile;
    broken.back() ^= 1;
    if (!ReplaceFile(root / "bad-crc", broken) || ReadNativeProfileRecord(root / "bad-crc", 1000, preserved) ||
        preserved.payload != imported.records[0].payload) { return 1; }
    std::printf("[native-profile-check] records=%u defaults-roundtrip=1 source-byte-identical=1 odd-payload=1 crc-rejection=1 failures=0\n", count);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!ApplyArchive(profile, imported) || !profile.SaveToDisk(root / "semantic")) { return 1; }
    const auto nativeEquipment = profile.nativeArchive->records[1].payload;
    if (profile.firstLaunch != (imported.records[0].payload[0] != 0) ||
        profile.playerBrother != nativeEquipment[65] || profile.activeWeaponSlot != nativeEquipment[64]) { return 1; }
    CProfileManager changed = profile;
    changed.coins += 123;
    changed.warbucks += 2;
    changed.experience += 10;
    changed.playerBrother = 1 - changed.playerBrother;
    changed.activeWeaponSlot = 1;
    changed.tutorialSeen[0] = 1 - changed.tutorialSeen[0];
    changed.dailyLastCommit += 1;
    changed.statistics[2] += 17;
    changed.clearedWaves[0] = 10;
    changed.perfectedWaves[0].reset();
    changed.perfectedWaves[0].set(5);
    if (changed.powerups.empty()) {
        // A test-only consumable, selected from an actual BIG type index.
        for (unsigned pack = 0; pack < toc.GetPackCount(); ++pack) {
            if (tables.GetObjectPack(pack).GetObjectCount(GameSection::Powerup) == 0) { continue; }
            GameObjectRef ref;
            ref.packHash = toc.GetPack(pack)->GetPackHash();
            ref.localIndex = 0;
            changed.AddPowerup(ref, 7);
            break;
        }
    }
    if (changed.powerups.empty() || changed.weaponMastery.empty()) { return 1; }
    changed.powerups[0].count = 7;
    changed.weaponMastery[0].experience += 99;
    if (!changed.SaveToDisk(root / "changed") || !changed.LoadFromDisk(root / "changed") ||
        changed.coins != profile.coins + 123 || changed.warbucks != profile.warbucks + 2 ||
        changed.experience != profile.experience + 10 || changed.playerBrother == profile.playerBrother ||
        changed.activeWeaponSlot != 1 || changed.tutorialSeen[0] == profile.tutorialSeen[0] ||
        changed.dailyLastCommit != profile.dailyLastCommit + 1 || changed.statistics[2] != profile.statistics[2] + 17 ||
        changed.clearedWaves[0] != 10 || changed.perfectedWaves[0].count() != 1 || !changed.perfectedWaves[0].test(5) ||
        changed.powerups[0].count != 7 || changed.weaponMastery[0].experience != profile.weaponMastery[0].experience + 99) { return 1; }
    // Uninterpreted configuration tail and unrelated clients remain original.
    for (unsigned offset = 84; offset < nativeEquipment.size(); ++offset) {
        if (changed.nativeArchive->records[1].payload[offset] != nativeEquipment[offset]) { return 1; }
    }
    for (unsigned index : {4u, 5u, 6u, 11u, 12u, 14u, 16u, 17u, 18u}) {
        if (changed.nativeArchive->records[index].payload != imported.records[index].payload) { return 1; }
    }
    std::printf("[native-profile-check] semantic-load save-reload balances equipment tips daily stats waves mastery unknown-preserved failures=0\n");
    CProfileManager fresh;
    fresh.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto freshPath = root / ("launch-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, fresh, freshPath, root / "absent-source") || !fresh.firstLaunch ||
        fresh.coins != 0 || fresh.warbucks != 0 || fresh.experience != 0 || fresh.inventory.empty() ||
        !SameRef(fresh.configuration.guns[1], MemoryRef(defaults.records[1].payload, 8))) { return 1; }
    const unsigned freshInventory = static_cast<unsigned>(fresh.inventory.size());
    fresh.playerBrother = 1;
    fresh.firstLaunch = false;
    if (!fresh.SaveToDisk(freshPath) || !fresh.LoadFromDisk(freshPath) || fresh.firstLaunch || fresh.playerBrother != 1 ||
        fresh.inventory.size() != freshInventory) { return 1; }
    std::printf("[native-profile-check] missing-source creates-original-files default-gear=%u player-select-persists failures=0\n", freshInventory);
    if (!fresh.soundEnabled || !fresh.musicEnabled || fresh.options.AutoBro() != 2) { return 1; }
    fresh.options.payload[31] = 0xA5; // Isolated unknown-byte preservation fixture.
    for (unsigned mode = 0; mode < 3; ++mode) {
        fresh.options.CycleAutoBro();
        fresh.soundEnabled = mode == 1;
        fresh.musicEnabled = mode == 2;
        if (!fresh.SaveToDisk(freshPath) || !fresh.LoadFromDisk(freshPath) ||
            fresh.options.AutoBro() != mode || fresh.brotherEnabled != (mode != 0) ||
            fresh.soundEnabled != (mode == 1) || fresh.musicEnabled != (mode == 2) ||
            fresh.options.payload[31] != 0xA5) { return 1; }
    }
    std::vector<std::uint8_t> optionBytes;
    if (!ReadBytes(freshPath / "p", optionBytes) || optionBytes.size() != 36 ||
        Get32(optionBytes, 0) != CCrc32::Crc32(optionBytes.data() + 4, 32)) { return 1; }
    optionBytes[0] ^= 1;
    if (!ReplaceFile(freshPath / "p", optionBytes) || fresh.LoadFromDisk(freshPath) ||
        !fresh.musicEnabled || fresh.options.payload[31] != 0xA5 || !fresh.SaveToDisk(freshPath)) { return 1; }
    std::printf("[native-options-check] original-p defaults sound music three-auto-bro-states CRC unknown-byte reload failures=0\n");
    std::vector<PlanetEntry> planets;
    if (!LoadPlanetCatalog(toc, tables, planets)) { return 1; }
    unsigned hordeRecords = 0;
    for (const auto &planet : planets) {
        for (unsigned index = 0; index < planet.missions.size(); ++index) {
            const auto &mission = planet.missions[index];
            if (mission.type != 2) { continue; }
            const auto &ref = planet.data.missions[index];
            // Isolated new-record fixture; source saves have no nonempty 1016.
            if (!RecordNativeMissionWaves(fresh, mission.level, mission.value64 + 2, {true, false}) ||
                !RecordNativeMissionScore(fresh, ref, 24000) || !RecordNativeMissionScore(fresh, ref, 12000) ||
                !fresh.SaveToDisk(freshPath) || !fresh.LoadFromDisk(freshPath)) { return 1; }
            const auto &scores = fresh.nativeArchive->records[16].payload;
            const std::size_t scoreOffset = FindRecord(scores, 14, 9, ref);
            const auto &waves = fresh.nativeArchive->records[3].payload;
            const std::size_t waveOffset = FindRecord(waves, 524, 7, mission.level);
            if (scoreOffset == scores.size() || Get32(scores, scoreOffset + 10) != 24000 ||
                waveOffset == waves.size() || (waves[waveOffset + 12 + mission.value64 / 8] & (1u << (mission.value64 % 8))) == 0) { return 1; }
            ++hordeRecords;
        }
    }
    std::printf("[native-profile-check] horde mission-score refs=%u original-strides lower-score-preserved perfect-bit reload failures=0\n", hordeRecords);
    CDailyBonusTracking daily;
    std::vector<StoreEntry> store;
    if (!daily.Load(toc, tables) || !LoadStoreCatalog(toc, tables, store)) { return 1; }
    // Deliberate test clock: below/exactly one day and exactly the reset gap.
    constexpr std::int64_t firstLaunchTime = 200000;
    const unsigned dailyCount = fresh.statistics[32];
    if (!daily.CommitBonus(fresh, firstLaunchTime, store) || daily.IsBonusAvailable(fresh, firstLaunchTime + 86399) ||
        !daily.CommitBonus(fresh, firstLaunchTime + 86400, store) || fresh.dailyLastCommit != 2 ||
        !daily.CommitBonus(fresh, firstLaunchTime + 86400 + 172800, store) || fresh.dailyLastCommit != 1 ||
        fresh.dailyConsecutiveSeconds != 0 || daily.CommitBonus(fresh, firstLaunchTime + 86400 + 172800, store) ||
        !fresh.SaveToDisk(freshPath) || !fresh.LoadFromDisk(freshPath) || fresh.statistics[32] != dailyCount + 3 ||
        fresh.dailyLastLaunchSeconds != firstLaunchTime + 86400 + 172800) { return 1; }
    std::printf("[native-profile-check] daily-original-seconds 86400-boundary 172800-reset one-claim reload failures=0\n");
    // Visits without claims must keep a streak alive across more than two days.
    daily.RefreshUsageData(fresh, firstLaunchTime + 86400 + 172800 + 80000);
    daily.RefreshUsageData(fresh, firstLaunchTime + 86400 + 172800 + 160000);
    daily.RefreshUsageData(fresh, firstLaunchTime + 86400 + 172800 + 240000);
    if (fresh.dailyConsecutiveSeconds != 240000 || fresh.dailyConsecutiveDays != 3 ||
        !daily.CommitBonus(fresh, firstLaunchTime + 86400 + 172800 + 240000, store) || fresh.dailyLastCommit != 3) { return 1; }
    std::printf("[native-profile-check] unclaimed-visits keep-streak failures=0\n");
    return 0;
}

int RunNativeProfilePlayCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path("out/ui-original-2026-09-09") / ("native-play-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, profile, path, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    for (unsigned planet = 0; planet < profile.nativeArchive->survivalLevels.size(); ++planet) {
        const CProfileManager before = profile;
        const auto planetPath = path / std::to_string(planet);
        // Only the isolated copy's wave progress is reset to test an actual update.
        profile.clearedWaves[planet] = 0;
        profile.perfectedWaves[planet].reset();
        const auto &level = profile.nativeArchive->survivalLevels[planet];
        std::vector<std::uint8_t> bytes;
        if (!tables.ReadSectionResource(level.packHash, GameSection::Level, level.localIndex, bytes)) { return 1; }
        CArrayInputStream input(bytes);
        CLevel::Template data;
        if (!data.Init(input) || input.Available() != 0 || !profile.SaveToDisk(planetPath)) { return 1; }
        SurvivalGameContext context{profile, planetPath, planet};
        if (RunSurvival(bigDirectory, tables.GetPackName(data.mapRef.packHash), data.mapRef.localIndex, 0, -1,
            "", 0, false, false, true, 2, 0, &context, true) != 0) { return 1; }
        if (!profile.LoadFromDisk(planetPath) || profile.clearedWaves[planet] != 2 || context.result.kills == 0 ||
            profile.coins != before.coins || profile.warbucks != before.warbucks || profile.playerBrother != before.playerBrother ||
            profile.activeWeaponSlot != before.activeWeaponSlot || profile.inventory.size() != before.inventory.size()) { return 1; }
        for (unsigned slot = 0; slot < 2; ++slot) {
            if (!SameRef(profile.configuration.guns[slot], before.configuration.guns[slot])) { return 1; }
        }
        for (unsigned slot = 0; slot < 4; ++slot) {
            if (!SameRef(profile.configuration.armor[slot], before.configuration.armor[slot])) { return 1; }
        }
        std::printf("[native-profile-play-check] planet=%u original-equipment active-slot=%u kills=%u waves=2 saved-reloaded failures=0\n",
            planet, profile.activeWeaponSlot, context.result.kills);
    }
    std::vector<PlanetEntry> planets;
    if (!LoadPlanetCatalog(toc, tables, planets)) { return 1; }
    for (const auto &planet : planets) {
        if (planet.missions.empty() || planet.missions[0].type != 2) { continue; }
        MissionEntry mission;
        mission.resource = planet.data.missions[0];
        mission.data = planet.missions[0];
        mission.title = planet.missionInfo[0].title;
        const auto &map = planet.missionInfo[0].map;
        const auto hordePath = path / "horde";
        // Reset only this isolated LEVEL record to prove a newly saved update.
        auto &testWaves = profile.nativeArchive->records[3].payload;
        const std::size_t previousWave = FindRecord(testWaves, 524, 7, mission.data.level);
        if (previousWave != testWaves.size()) {
            testWaves.erase(testWaves.begin() + previousWave, testWaves.begin() + previousWave + 524);
            Put32(testWaves, 0, Get32(testWaves, 0) - 1);
        }
        SurvivalGameContext context{profile, hordePath};
        context.hordeStart = 0;
        if (RunSurvival(bigDirectory, tables.GetPackName(map.packHash), map.localIndex, 0, -1,
            "", 0, false, false, true, 2, mission.data.value64, &context, true, false, &mission) != 0 ||
            !profile.LoadFromDisk(hordePath)) { return 1; }
        const auto &scores = profile.nativeArchive->records[16].payload;
        const auto &waves = profile.nativeArchive->records[3].payload;
        const std::size_t scoreOffset = FindRecord(scores, 14, 9, mission.resource);
        const std::size_t waveOffset = FindRecord(waves, 524, 7, mission.data.level);
        if (scoreOffset == scores.size() || Get32(scores, scoreOffset + 10) == 0 || waveOffset == waves.size() ||
            (waves[waveOffset + 8] | (waves[waveOffset + 9] << 8)) < 2) { return 1; }
        std::printf("[native-profile-play-check] horde original-mission score=%u wave=%u saved-reloaded failures=0\n",
            Get32(scores, scoreOffset + 10), waves[waveOffset + 8] | (waves[waveOffset + 9] << 8));
        break;
    }
    return 0;
}
