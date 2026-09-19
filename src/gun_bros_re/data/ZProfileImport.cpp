/** @file ZProfileImport.cpp
 * @brief Native storage envelope and proven data layouts; source files stay read-only.
 */
#include "gun_bros_re/data/ZProfileImport.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "engine/core/CCrc32.h"
#include "gun_bros_re/data/CProfileManager.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/game/CGameSession.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iterator>
#include "gun_bros_re/data/ZProfileImportInternal.h"
using namespace ProfileImportDetail;

namespace ProfileImportDetail {

std::uint64_t ReadUInt64(CArrayInputStream &stream) {
    const std::uint64_t low = stream.ReadUInt32();
    return low | (static_cast<std::uint64_t>(stream.ReadUInt32()) << 32);
}
}

namespace ProfileImportDetail {

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
}

namespace ProfileImportDetail {

bool ReportReference(ZPackTables &tables, CResTOCManager &toc, const GameObjectRef &ref,
    unsigned type, std::ofstream &report) {
    if (ref.IsNull() || ref.localIndex == 255) { report << " none"; return true; }
    const int packIndex = toc.GetPackIndexFromHash(ref.packHash);
    if (packIndex < 0) { report << " missing-pack=" << ref.packHash; return false; }
    report << ' ' << tables.GetPackName(ref.packHash) << ':' << unsigned(ref.localIndex) << " type=" << type;
    std::vector<std::uint8_t> payload;
    return tables.ReadSectionResource(ref.packHash, static_cast<ZGameSection>(type + 1), ref.localIndex, payload);
}
}

namespace ProfileImportDetail {

// Exercise malformed copies only. A failed envelope read must not replace the
// caller's last valid record; checksum failure remains visible to the inspector.

}

bool ReadDataStore(const std::filesystem::path &path, ZImportedDataStore &record) {
    std::ifstream input(path, std::ios::binary);
    if (!input) { return false; }
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    // CProfileManager::SaveToDisk :203163 rounds to the next 512-byte block
    // and adds 24 bytes. Existing fixtures use the second-header-word == 0 form.
    if (bytes.size() < 24 || (bytes.size() - 24) % 512 != 0) { return false; }
    CArrayInputStream stream(bytes);
    ZImportedDataStore candidate;
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

bool ImportProfile(CResTOCManager &toc, ZPackTables &tables, CProfileManager &profile, const std::filesystem::path &source) {
    // The legacy check importer uses the same standard samples as NativeProfile.
    const char *files[] = {"-1_1000", "-1_1001", "-1_1002", "-1_1003", "-1_1013"};
    ZImportedDataStore records[5];
    for (unsigned index = 0; index < 5; ++index) {
        if (!ReadDataStore(source / files[index], records[index]) || !records[index].crcMatches || records[index].version != 0) {
            std::printf("[original-profile] invalid source: %s\n", files[index]);
            return false;
        }
    }
    if (records[0].minimumSize != 48 || records[1].minimumSize != 120) { return false; }
    CProfileManager candidate = profile;
    candidate.tutorialCompleted = true;
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
        if (!tables.ReadSectionResource(ref.packHash, static_cast<ZGameSection>(type + 1), ref.localIndex, payload)) { return false; }
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
    for (auto &waves : candidate.perfectedWaves) { waves.reset(); }
    for (unsigned index = 0; index < missionCount; ++index) {
        GameObjectRef level;
        level.packHash = missions.ReadUInt32();
        level.localIndex = missions.ReadUInt8();
        const unsigned type = missions.ReadUInt8();
        missions.Skip(2);
        const unsigned waveProgress = missions.ReadUInt16();
        // WasWavePerfected :192487 reads a 4096-bit array after the secondary
        // wave value. The disk reference occupies two more bytes than RAM.
        missions.Skip(2);
        std::array<std::uint8_t, 512> perfectBits{};
        for (auto &byte : perfectBits) { byte = missions.ReadUInt8(); }
        if (type != 7 || toc.GetPackIndexFromHash(level.packHash) < 0) { return false; }
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(level.packHash, ZGameSection::Level, level.localIndex, payload)) { return false; }
        CArrayInputStream stream(payload);
        CLevel::Template data;
        if (!data.Init(stream)) { return false; }
        // BOKOR is retained in the archive but is not a retail 500-wave planet.
        if (data.waveLimit != 500 || data.script.GetStates().size() <= 100) { continue; }
        for (unsigned planet = 0; planet < 4; ++planet) {
            const int packIndex = toc.GetPackIndexFromName(planetPacks[planet]);
            if (packIndex >= 0 && toc.GetPack(packIndex)->GetPackHash() == level.packHash) {
                candidate.clearedWaves[planet] = std::min(waveProgress, 500u);
                for (unsigned wave = 0; wave < 500; ++wave) {
                    candidate.perfectedWaves[planet].set(wave, (perfectBits[wave / 8] & (1u << (wave % 8))) != 0);
                }
            }
        }
    }
    // SaveRestore registration :80431 and CWeaponMastery::SaveToServer :192239
    // identify 1013 as weapons3. The older 1005 collection is not current XP.
    CArrayInputStream mastery(records[4].data);
    const unsigned masteryCount = mastery.ReadUInt32();
    if (masteryCount > 128 || 4 + masteryCount * 14 != records[4].minimumSize) { return false; }
    candidate.weaponMastery.clear();
    for (unsigned index = 0; index < masteryCount; ++index) {
        ZWeaponMasteryEntry entry;
        entry.resource.packHash = mastery.ReadUInt32();
        entry.resource.localIndex = mastery.ReadUInt8();
        if (mastery.ReadUInt8() != 6 || toc.GetPackIndexFromHash(entry.resource.packHash) < 0) { return false; }
        mastery.Skip(4); // Native dirty flag and padding, following the disk reference.
        entry.experience = mastery.ReadUInt32();
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(entry.resource.packHash, ZGameSection::Gun, entry.resource.localIndex, payload)) { return false; }
        candidate.weaponMastery.push_back(entry);
        std::printf("[original-profile] mastery pack=%u gun=%u xp=%u\n", entry.resource.packHash, entry.resource.localIndex, entry.experience);
    }
    if (progress.Overran() || equipment.Overran() || inventory.Overran() || missions.Overran() || mastery.Overran()) { return false; }
    profile = std::move(candidate);
    std::printf("[original-profile] imported xp=%llu coins=%llu warbucks=%llu xplodium=%llu owned=%zu\n",
        profile.experience, profile.coins, profile.warbucks, profile.xplodium, profile.inventory.size());
    return true;
}
