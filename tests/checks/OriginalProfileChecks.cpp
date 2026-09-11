#include "gameplay/SurvivalStudy.h"
/** @file OriginalProfile.cpp
 * @brief Native storage envelope and proven data layouts; source files stay read-only.
 */
#include "TestOutput.h"
#include "gun_bros_re/data/OriginalProfile.h"
#include "gun_bros_re/data/StoreCatalog.h"
#include "engine/core/CCrc32.h"
#include "gun_bros_re/data/CProfileManager.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "gun_bros_re/gameplay/SurvivalGameContext.h"
#include "gun_bros_re/gameplay/MapScene.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iterator>
#include "gun_bros_re/data/OriginalProfileInternal.h"
using namespace OriginalProfileDetail;
#include "Checks.h"

int RunOriginalProfileCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CPlayerProgress::Template progressData;
    if (!LoadPlayerProgress(toc, tables, progressData)) { return 1; }
    const std::filesystem::path source = TestOutput::Fixtures();
    const std::filesystem::path output = TestOutput::Path("original-saves");
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
        } else if (storeId == 1002 || storeId == 1003 || storeId == 1013) {
            const unsigned count = data.ReadUInt32();
            unsigned stride = 10;
            unsigned maximum = 512;
            if (storeId == 1003) { stride = 524; maximum = 64; }
            if (storeId == 1013) { stride = 14; maximum = 128; }
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
                } else if (storeId == 1013) {
                    data.Skip(2); // The native XP field is aligned at offset 8.
                    report << " mastery-xp=" << data.ReadUInt32();
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
    // Use the standard supplied record; .perfect was an optional old sample.
    failures += CheckStorageBoundaries(source / "-1_1000", output);
    failures += crcMismatches;
    report << "files=" << paths.size() << " crc-mismatches=" << crcMismatches << " failures=" << failures << '\n';
    std::printf("[original-save-check] files=%zu crc-mismatches=%u failures=%u\n", paths.size(), crcMismatches, failures);
    return failures != 0;
}

int RunOriginalProfilePlayCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    profile.tutorialCompleted = true;
    if (!ImportOriginalProfile(toc, tables, profile, TestOutput::Fixtures())) { return 1; }
    CProfileManager capCheck = profile;
    const GameObjectRef mainGun = profile.configuration.guns[0];
    const unsigned originalMastery = profile.GetWeaponExperience(mainGun);
    // Older template limits must not truncate imported progress on the next kill.
    capCheck.AddWeaponExperience(mainGun, 1, originalMastery - 1);
    if (capCheck.GetWeaponExperience(mainGun) != originalMastery) { return 1; }
    const std::filesystem::path save = TestOutput::Path("original-profile-check.dat");
    if (!profile.SaveToDisk(save)) { return 1; }
    CProfileManager restored;
    restored.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!restored.LoadFromDisk(save) || restored.experience != profile.experience ||
        restored.configuration.guns[0].localIndex != 64 || restored.configuration.armor[0].localIndex != 178 ||
        restored.inventory.size() != 8 || restored.weaponMastery.size() != 4 ||
        restored.GetWeaponExperience(restored.configuration.guns[0]) < 800000) { return 1; }
    for (unsigned waves : restored.clearedWaves) { if (waves != 500) { return 1; } }
    SurvivalGameContext context{restored, save, 0};
    if (RunSurvivalStudy(bigDirectory, "pack2", 7, 0, -1, "", 0, false, false, true, 2, 0, &context, true) != 0) { return 1; }
    if (!restored.LoadFromDisk(save)) { return 1; }
    for (const auto &entry : profile.weaponMastery) {
        if (restored.GetWeaponExperience(entry.resource) < entry.experience) { return 1; }
    }
    std::printf("[original-profile-check] original-equipment/play/reload failures=0\n");
    return 0;
}