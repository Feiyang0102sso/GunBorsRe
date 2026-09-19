/** Native DataStore responsibilities; see saves/GB_save_profile.bt and save_payloads.bt. */
#include "gun_bros_re/data/profile/CProfileManagerStorage.h"
#include "gun_bros_re/host/ZProfileFiles.h"
#include "gun_bros_re/data/mission/Planet.h"
#include "engine/core/CCrc32.h"
#include <algorithm>
#include <cstdio>
using namespace ProfileStorageDetail;

bool CProfileManager::ReadRecord(const std::filesystem::path &path, unsigned id, CProfileManager::Record &record) {
    CProfileManager::Envelope envelope;
    unsigned version = 0;
    if (id == 1008) { version = 1; }
    if (id == 1017) { version = 5; }
    if (!CProfileManager::ReadEnvelope(path, envelope) || !envelope.crcMatches || envelope.version != version) {
        std::printf("[native-profile] invalid envelope/version id=%u file=%s\n", id, path.string().c_str());
        return false;
    }
    std::size_t size = 0;
    if (!PayloadSize(id, envelope.data, size) || (size & ~std::size_t(1)) != envelope.minimumSize) {
        std::printf("[native-profile] invalid payload id=%u file=%s\n", id, path.string().c_str());
        return false;
    }
    CProfileManager::Record candidate;
    candidate.version = envelope.version;
    candidate.owner = envelope.clientId;
    candidate.payload.assign(envelope.data.begin(), envelope.data.begin() + size);
    candidate.originalPayload = candidate.payload;
    if (!ZProfileFiles::Read(path, candidate.originalFile)) { return false; }
    record = std::move(candidate);
    return true;
}

bool CProfileManager::WriteArchive(const std::filesystem::path &directory, const CProfileManager::Archive &archive) {
    const auto destination = std::filesystem::weakly_canonical(directory);
    if (!ZProfileFiles::AllowsDestination(destination, archive.importDirectory)) { return false; }
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
        if (!ZProfileFiles::Replace(path, bytes)) { return false; }
    }
    const std::vector<std::uint8_t> status(archive.status.begin(), archive.status.end());
    if (!ZProfileFiles::Replace(destination / (std::to_string(archive.records[0].owner) + "_PDST"), status)) { return false; }
    // COptionsMgr::Write :51400 stores CRC followed by mem+16..47 in "p".
    std::vector<std::uint8_t> options(36, 0);
    std::copy(archive.options.payload.begin(), archive.options.payload.end(), options.begin() + 4);
    Put32(options, 0, CCrc32::Crc32(options.data() + 4, 32));
    return ZProfileFiles::Replace(destination / "p", options);
}

bool CProfileManager::CreateArchive(CResTOCManager &toc, CGunBros &tables, CProfileManager::Archive &archive) {
    CProfileManager::Archive candidate;
    candidate.toc = &toc;
    candidate.tables = &tables;
    candidate.status.fill(4); // CProfileManager::Initialize :203931.
    if (!CPlayerProgress::Template::Load(toc, tables, candidate.progression)) { return false; }
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
        ZGameSection section = ZGameSection::Armor;
        if (slot < 2) { section = ZGameSection::Gun; }
        else if (slot < 4) { section = ZGameSection::Bullet; }
        std::vector<std::uint8_t> bytes;
        if (!tables.ReadSectionResource(hash, section, ordinals[slot], bytes)) { return false; }
    }
    candidate.records[7].payload.resize(22, 0); // CTutorialManager :210464/:210615.
    candidate.records[8].version = 1;
    candidate.records[8].payload.resize(340, 0); // CRefinementManager :177641.
    CRefinementManager::Template refinement;
    if (!CRefinementManager::Template::Load(toc, tables, refinement)) { return false; }
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

    std::map<unsigned, GameObjectRef> retailLevels;
    if (!Planet::LoadSurvivalLevels(toc, tables, retailLevels)) { return false; }
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

bool CProfileManager::ReadEnvelope(const std::filesystem::path &path, CProfileManager::Envelope &record) {
    std::vector<std::uint8_t> bytes;
    if (!ZProfileFiles::Read(path, bytes)) { return false; }
    // CProfileManager::SaveToDisk :203163 rounds to the next 512-byte block
    // and adds 24 bytes. Existing fixtures use the second-header-word == 0 form.
    if (bytes.size() < 24 || (bytes.size() - 24) % 512 != 0) { return false; }
    CArrayInputStream stream(bytes);
    CProfileManager::Envelope candidate;
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
namespace ProfileStorageDetail {
bool ReadArchive(const std::filesystem::path &directory, CProfileManager::Archive &archive) {
    const auto optionsPath = directory / "p";
    if (std::filesystem::exists(optionsPath)) {
        std::vector<std::uint8_t> options;
        if (!ZProfileFiles::Read(optionsPath, options) || options.size() != 36 ||
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
        if (!CProfileManager::ReadRecord(path, id, archive.records[id - kFirstStore]) || archive.records[id - kFirstStore].owner != -1) { return false; }
    }
    const auto statusPath = directory / "-1_PDST";
    if (std::filesystem::exists(statusPath)) {
        std::vector<std::uint8_t> status;
        if (!ZProfileFiles::Read(statusPath, status) || status.size() != archive.status.size()) { return false; }
        std::copy(status.begin(), status.end(), archive.status.begin());
    }
    return true;
}
}
