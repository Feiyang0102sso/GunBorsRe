/** @file OriginalProfile.cpp
 * @brief Native storage envelope and proven data layouts; source files stay read-only.
 */
#include "runtime/OriginalProfile.h"
#include "runtime/StoreCatalog.h"
#include "engine/CCrc32.h"
#include "gun_bros/CProfileManager.h"
#include "gun_bros/CLevel.h"
#include "runtime/SurvivalGameContext.h"
#include "milestones/M3Map.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iterator>

namespace {
std::uint64_t ReadUInt64(CArrayInputStream &stream) {
    const std::uint64_t low = stream.ReadUInt32();
    return low | (static_cast<std::uint64_t>(stream.ReadUInt32()) << 32);
}

GameObjectRef ReadMemoryRef(CArrayInputStream &stream) {
    GameObjectRef ref;
    ref.packHash = stream.ReadUInt32();
    stream.Skip(2); // Cached runtime pack index is reconciled from the hash.
    ref.localIndex = stream.ReadUInt8();
    stream.Skip(1); // Native structure alignment, not a wire-format field.
    // Original empty slots may retain a core-pack hash with local index 255.
    // Normalize that native sentinel to the rebuilt profile's empty reference.
    if (ref.localIndex == 255) { ref = {}; }
    return ref;
}

bool ReportReference(PackTables &tables, CResTOCManager &toc, const GameObjectRef &ref,
    unsigned type, std::ofstream &report) {
    if (ref.IsNull() || ref.localIndex == 255) { report << " none"; return true; }
    const int packIndex = toc.GetPackIndexFromHash(ref.packHash);
    if (packIndex < 0) { report << " missing-pack=" << ref.packHash; return false; }
    report << ' ' << tables.GetPackName(ref.packHash) << ':' << unsigned(ref.localIndex) << " type=" << type;
    std::vector<std::uint8_t> payload;
    return tables.ReadSectionResource(ref.packHash, static_cast<GameSection>(type + 1), ref.localIndex, payload);
}

// Exercise malformed copies only. A failed envelope read must not replace the
// caller's last valid record; checksum failure remains visible to the inspector.
unsigned CheckStorageBoundaries(const std::filesystem::path &source, const std::filesystem::path &output) {
    std::ifstream input(source, std::ios::binary);
    const std::vector<std::uint8_t> original{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (original.size() < 536) { return 1; }
    unsigned failures = 0;
    for (unsigned test = 0; test < 5; ++test) {
        std::vector<std::uint8_t> bytes = original;
        if (test == 0) { bytes.resize(23); }
        if (test == 1) { bytes.pop_back(); }
        if (test == 2) { bytes[4] = 1; }
        if (test == 3) {
            for (unsigned index = 8; index < 12; ++index) { bytes[index] = 255; }
        }
        if (test == 4) { bytes.back() ^= 1; }
        const auto path = output / ("boundary-" + std::to_string(test) + ".dat");
        std::ofstream fixture(path, std::ios::binary);
        fixture.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        fixture.close();
        if (!fixture) { ++failures; continue; }
        OriginalDataStore record;
        record.version = 123;
        const bool loaded = ReadOriginalDataStore(path, record);
        if (test < 4 && (loaded || record.version != 123)) { ++failures; }
        if (test == 4 && (!loaded || record.crcMatches)) { ++failures; }
    }
    std::printf("[original-save-check] malformed-copies=5 failures=%u\n", failures);
    return failures;
}
}

bool ReadOriginalDataStore(const std::filesystem::path &path, OriginalDataStore &record) {
    std::ifstream input(path, std::ios::binary);
    if (!input) { return false; }
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    // CProfileManager::SaveToDisk :203163 rounds to the next 512-byte block
    // and adds 24 bytes. Existing fixtures use the second-header-word == 0 form.
    if (bytes.size() < 24 || (bytes.size() - 24) % 512 != 0) { return false; }
    CArrayInputStream stream(bytes);
    OriginalDataStore candidate;
    candidate.version = stream.ReadUInt32();
    if (stream.ReadUInt32() != 0) { return false; }
    candidate.padding = stream.ReadUInt32();
    if (candidate.padding >= stream.Available() / 2 || candidate.padding * 2 > bytes.size() - 24) { return false; }
    candidate.minimumSize = static_cast<unsigned>(bytes.size() - 24 - candidate.padding * 2);
    stream.Skip(candidate.padding);
    // Keep the possible odd final byte plus trailing fill available. Each
    // typed reader determines its exact size; the envelope only gives a lower bound.
    candidate.data.assign(bytes.begin() + stream.Position(), bytes.end() - 8);
    CArrayInputStream tail(bytes.data() + bytes.size() - 8, 8);
    candidate.clientId = tail.ReadInt32();
    const unsigned expectedCrc = tail.ReadUInt32();
    const unsigned actualCrc = CCrc32::Crc32(bytes.data(), bytes.size() - 4);
    candidate.crcMatches = expectedCrc == actualCrc;
    record = std::move(candidate);
    return true;
}

int RunOriginalProfileCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CPlayerProgress::Template progressData;
    if (!LoadPlayerProgress(toc, tables, progressData)) { return 1; }
    const std::filesystem::path source = std::filesystem::path(ASSET_ROOT) / "saves";
    const std::filesystem::path output = "out/original-saves";
    std::filesystem::create_directories(output);
    std::ofstream report(output / "report.txt");
    if (!report) { return 1; }
    std::vector<std::filesystem::path> paths;
    for (const auto &entry : std::filesystem::directory_iterator(source)) {
        if (entry.is_regular_file() && entry.path().filename().string().find("-1_10") == 0) { paths.push_back(entry.path()); }
    }
    std::sort(paths.begin(), paths.end());
    if (paths.empty()) { std::printf("[original-save-check] no source records\n"); return 1; }
    unsigned failures = 0, crcMismatches = 0;
    for (const auto &path : paths) {
        OriginalDataStore record;
        if (!ReadOriginalDataStore(path, record)) { ++failures; report << path.filename().string() << " invalid envelope\n"; continue; }
        if (!record.crcMatches) { ++crcMismatches; }
        const std::string name = path.filename().string();
        report << name << " version=" << record.version << " size-even-lower-bound=" << record.minimumSize
            << " padding=" << record.padding << " client=" << record.clientId << " crc=" << record.crcMatches << '\n';
        const int storeId = std::stoi(name.substr(3, 4));
        CArrayInputStream data(record.data);
        unsigned consumed = 0;
        if (storeId == 1000) {
            // CPlayerProgress::LoadFromDisk :194502 reads 48 bytes at this+40.
            data.Skip(12);
            const std::uint64_t coins = ReadUInt64(data);
            const unsigned rare = data.ReadUInt32();
            const std::uint64_t xp = ReadUInt64(data);
            const unsigned level = data.ReadUInt16();
            CPlayerProgress derived;
            derived.Bind(progressData);
            derived.SetExperience(xp);
            report << " CPlayerProgress coins=" << coins << " warbucks=" << rare << " xp=" << xp
                << " saved-level=" << level << " level-from-xp=" << derived.GetLevel() << '\n';
            consumed = 48;
        } else if (storeId == 1001) {
            // CPlayerConfiguration :170602 reads 120 bytes; first eight refs
            // are two guns, two bullet caches, then four armor slots.
            for (unsigned index = 0; index < 8; ++index) {
                const GameObjectRef ref = ReadMemoryRef(data);
                unsigned type = 2;
                if (index < 2) { type = 6; }
                else if (index < 4) { type = 3; }
                report << " CPlayerConfiguration ref=" << index;
                if (!ReportReference(tables, toc, ref, type, report)) { ++failures; }
                report << '\n';
            }
            consumed = 120;
        } else if (storeId == 1002 || storeId == 1003) {
            const unsigned count = data.ReadUInt32();
            unsigned stride = 10;
            unsigned maximum = 512;
            if (storeId == 1003) { stride = 524; maximum = 64; }
            if (count > maximum || 4 + count * stride > record.data.size()) { ++failures; continue; }
            consumed = 4 + count * stride;
            report << " collection-count=" << count << '\n';
            for (unsigned index = 0; index < count; ++index) {
                GameObjectRef ref;
                ref.packHash = data.ReadUInt32();
                ref.localIndex = data.ReadUInt8();
                const unsigned type = data.ReadUInt8();
                const unsigned status = data.ReadUInt8();
                data.Skip(1);
                report << " entry=" << index << " status=" << status;
                if (!ReportReference(tables, toc, ref, type, report)) { ++failures; }
                if (storeId == 1002) {
                    const unsigned quantity = data.ReadUInt8();
                    data.Skip(1);
                    report << " quantity=" << quantity;
                } else {
                    const unsigned progress = data.ReadUInt16();
                    const unsigned value8 = data.ReadUInt16();
                    data.Skip(512);
                    report << " wave-progress=" << progress << " value8=" << value8;
                }
                report << '\n';
            }
        }
        if (consumed != 0 && (data.Overran() || consumed > record.data.size() ||
            (consumed & ~1u) != record.minimumSize)) {
            ++failures;
            report << " invalid typed payload size\n";
            continue;
        }
        if (consumed == 0) { consumed = record.minimumSize; }
        // Unmapped stores explicitly export the even lower bound, not a claim
        // that every byte of their native structure has been interpreted.
        std::ofstream payload(output / (name + ".payload.bin"), std::ios::binary);
        payload.write(reinterpret_cast<const char *>(record.data.data()), consumed);
        if (!payload) { ++failures; }
    }
    failures += CheckStorageBoundaries(source / "-1_1000.perfect", output);
    failures += crcMismatches;
    report << "files=" << paths.size() << " crc-mismatches=" << crcMismatches << " failures=" << failures << '\n';
    std::printf("[original-save-check] files=%zu crc-mismatches=%u failures=%u\n", paths.size(), crcMismatches, failures);
    return failures != 0;
}

bool ImportOriginalProfile(CResTOCManager &toc, PackTables &tables, CProfileManager &profile) {
    const std::filesystem::path source = std::filesystem::path(ASSET_ROOT) / "saves";
    const char *files[] = {"-1_1000.perfect", "-1_1001", "-1_1002", "-1_1003.perfect"};
    OriginalDataStore records[4];
    for (unsigned index = 0; index < 4; ++index) {
        if (!ReadOriginalDataStore(source / files[index], records[index]) || !records[index].crcMatches || records[index].version != 0) {
            std::printf("[original-profile] invalid source: %s\n", files[index]);
            return false;
        }
    }
    if (records[0].minimumSize != 48 || records[1].minimumSize != 120) { return false; }
    CProfileManager candidate = profile;
    CArrayInputStream progress(records[0].data);
    progress.Skip(4);
    // CRefinementManager::BeginRefinement :178519 consumes the QWORD this+44.
    candidate.xplodium = ReadUInt64(progress);
    candidate.coins = ReadUInt64(progress);
    candidate.warbucks = progress.ReadUInt32();
    candidate.experience = ReadUInt64(progress);
    CArrayInputStream equipment(records[1].data);
    for (GameObjectRef &gun : candidate.configuration.guns) { gun = ReadMemoryRef(equipment); }
    equipment.Skip(16); // Bullet caches are reconstructed from the equipped guns.
    for (GameObjectRef &armor : candidate.configuration.armor) { armor = ReadMemoryRef(equipment); }
    CArrayInputStream inventory(records[2].data);
    const unsigned count = inventory.ReadUInt32();
    if (count > 512 || 4 + count * 10 != records[2].minimumSize) { return false; }
    candidate.inventory.clear();
    candidate.powerups.clear();
    for (unsigned index = 0; index < count; ++index) {
        GameObjectRef ref;
        ref.packHash = inventory.ReadUInt32();
        ref.localIndex = inventory.ReadUInt8();
        const unsigned type = inventory.ReadUInt8();
        inventory.Skip(2); // Server dirty flag and native alignment.
        const unsigned quantity = inventory.ReadUInt8();
        inventory.Skip(1);
        if (quantity == 0) { continue; }
        if (toc.GetPackIndexFromHash(ref.packHash) < 0) { return false; }
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(ref.packHash, static_cast<GameSection>(type + 1), ref.localIndex, payload)) { return false; }
        if (type == 2 || type == 6) { candidate.Grant(type, ref); }
        else if (type == 17) { candidate.AddPowerup(ref, quantity); }
    }
    for (const GameObjectRef &gun : candidate.configuration.guns) {
        if (!candidate.Owns(6, gun)) { return false; }
    }
    for (const GameObjectRef &armor : candidate.configuration.armor) {
        if (!armor.IsNull() && armor.localIndex != 255 && !candidate.Owns(2, armor)) { return false; }
    }
    CArrayInputStream missions(records[3].data);
    const unsigned missionCount = missions.ReadUInt32();
    if (missionCount > 64 || 4 + missionCount * 524 != records[3].minimumSize) { return false; }
    const char *planetPacks[] = {"pack2", "pack7", "pack9", "pack12"};
    candidate.clearedWaves.fill(0);
    for (unsigned index = 0; index < missionCount; ++index) {
        GameObjectRef level;
        level.packHash = missions.ReadUInt32();
        level.localIndex = missions.ReadUInt8();
        const unsigned type = missions.ReadUInt8();
        missions.Skip(2);
        const unsigned waveProgress = missions.ReadUInt16();
        missions.Skip(514); // Secondary wave value and per-wave bit data remain archived.
        if (type != 7 || toc.GetPackIndexFromHash(level.packHash) < 0) { return false; }
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(level.packHash, GameSection::Level, level.localIndex, payload)) { return false; }
        CArrayInputStream stream(payload);
        CLevel::Template data;
        if (!data.Init(stream)) { return false; }
        // BOKOR is retained in the archive but is not a retail 500-wave planet.
        if (data.waveLimit != 500 || data.script.GetStates().size() <= 100) { continue; }
        for (unsigned planet = 0; planet < 4; ++planet) {
            const int packIndex = toc.GetPackIndexFromName(planetPacks[planet]);
            if (packIndex >= 0 && toc.GetPack(packIndex)->GetPackHash() == level.packHash) {
                candidate.clearedWaves[planet] = std::min(waveProgress, 500u);
            }
        }
    }
    if (progress.Overran() || equipment.Overran() || inventory.Overran() || missions.Overran()) { return false; }
    profile = std::move(candidate);
    std::printf("[original-profile] imported xp=%llu coins=%llu warbucks=%llu xplodium=%llu owned=%zu\n",
        profile.experience, profile.coins, profile.warbucks, profile.xplodium, profile.inventory.size());
    return true;
}

int RunOriginalProfilePlayCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!ImportOriginalProfile(toc, tables, profile)) { return 1; }
    const std::filesystem::path save = "out/original-profile-check.dat";
    if (!profile.SaveToDisk(save)) { return 1; }
    CProfileManager restored;
    restored.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!restored.LoadFromDisk(save) || restored.experience != profile.experience ||
        restored.configuration.guns[0].localIndex != 64 || restored.configuration.armor[0].localIndex != 178 ||
        restored.inventory.size() != 8) { return 1; }
    for (unsigned waves : restored.clearedWaves) { if (waves != 500) { return 1; } }
    SurvivalGameContext context{restored, save, 0};
    if (RunSurvival(bigDirectory, "pack2", 7, 0, -1, "", 0, false, false, true, 2, 0, &context, true) != 0) { return 1; }
    if (!restored.LoadFromDisk(save)) { return 1; }
    std::printf("[original-profile-check] original-equipment/play/reload failures=0\n");
    return 0;
}
