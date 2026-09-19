#include "gameplay/SurvivalStudy.h"
/** @file ZProfileStorage.cpp
 * @brief Original serialized layouts; Windows file replacement is the host boundary.
 * Sources: saves/GB_save_profile.bt, save_payloads.bt and CProfileManager :202880/:203163.
 */
#define NOMINMAX
#include "TestOutput.h"
#include "gun_bros_re/data/ZProfileStorage.h"
#include "gun_bros_re/host/ZHostSettings.h"
#include "gun_bros_re/data/CChallengeManager.h"
#include "gun_bros_re/data/ZProfileImport.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "gun_bros_re/gameplay/game/CGameSession.h"
#include "gun_bros_re/gameplay/game/CGameFlow.h"
#include "gun_bros_re/data/ZPlanetCatalog.h"
#include "gun_bros_re/data/ZMissionCatalog.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/data/Planet.h"
#include "gun_bros_re/data/Mission.h"
#include "gun_bros_re/data/CProfileManager.h"
#include "gun_bros_re/gameplay/weapon/CGun.h"
#include "gun_bros_re/data/CDailyBonusTracking.h"
#include "engine/core/CCrc32.h"
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
#include "gun_bros_re/data/ZProfileStorageInternal.h"
using namespace ProfileStorageDetail;
#include "Checks.h"

int RunNativeProfileCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    ZProfileArchive defaults;
    if (!CreateProfileArchive(toc, tables, defaults)) { return 1; }
    const std::filesystem::path root = TestOutput::Path("ui-original-2026-09-09/native-profile");
    const auto empty = root / "new";
    if (!WriteProfileArchive(empty, defaults)) { return 1; }
    ZProfileArchive imported = defaults;
    const auto source = TestOutput::Fixtures();
    unsigned count = 0;
    for (unsigned id = kFirstStore; id <= kLastStore; ++id) {
        if (id == kUnregisteredStore) { continue; }
        const std::string filename = "-1_" + std::to_string(id);
        ZProfileRecord restored;
        if (!ReadProfileRecord(empty / filename, id, restored) || restored.payload != defaults.records[id - kFirstStore].payload ||
            !ReadProfileRecord(source / filename, id, imported.records[id - kFirstStore])) { return 1; }
        ++count;
    }
    std::vector<std::uint8_t> status;
    if (!ReadBytes(source / "-1_PDST", status) || status.size() != imported.status.size()) { return 1; }
    std::copy(status.begin(), status.end(), imported.status.begin());
    if (!WriteProfileArchive(root / "imported", imported)) { return 1; }
    for (unsigned id = kFirstStore; id <= kLastStore; ++id) {
        if (id == kUnregisteredStore) { continue; }
        std::vector<std::uint8_t> written;
        if (!ReadBytes(root / "imported" / ("-1_" + std::to_string(id)), written) ||
            written != imported.records[id - kFirstStore].originalFile) { return 1; }
    }
    // Corrupt a copy. Loading must fail and retain the last valid record.
    ZProfileRecord preserved = imported.records[0];
    auto broken = preserved.originalFile;
    broken.back() ^= 1;
    if (!ReplaceFile(root / "bad-crc", broken) || ReadProfileRecord(root / "bad-crc", 1000, preserved) ||
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
    changed.configuration.powerups = {13, 5};
    changed.tutorialSeen[0] = 1 - changed.tutorialSeen[0];
    changed.dailyLastCommit += 1;
    changed.statistics[2] += 17;
    changed.clearedWaves[0] = 10;
    changed.perfectedWaves[0].reset();
    changed.perfectedWaves[0].set(5);
    if (changed.powerups.empty()) {
        // A test-only consumable, selected from an actual BIG type index.
        for (unsigned pack = 0; pack < toc.GetPackCount(); ++pack) {
            if (tables.GetObjectPack(pack).GetObjectCount(ZGameSection::Powerup) == 0) { continue; }
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
    if (changed.configuration.powerups != std::array<std::uint8_t, 2>{13, 5} ||
        changed.nativeArchive->records[1].payload[116] != 13 ||
        changed.nativeArchive->records[1].payload[117] != 5) {
        std::printf("[powerup-equipment-check] native slot save/reload failed\n");
        return 1;
    }
    // Uninterpreted configuration tail and unrelated clients remain original.
    for (unsigned offset = 84; offset < nativeEquipment.size(); ++offset) {
        if (offset == 116 || offset == 117) { continue; }
        if (changed.nativeArchive->records[1].payload[offset] != nativeEquipment[offset]) { return 1; }
    }
    for (unsigned index : {4u, 5u, 6u, 11u, 12u, 14u, 16u, 17u, 18u}) {
        if (changed.nativeArchive->records[index].payload != imported.records[index].payload) { return 1; }
    }
    std::printf("[native-profile-check] semantic-load save-reload balances equipment tips daily stats waves mastery unknown-preserved failures=0\n");
    std::vector<ZStoreEntry> store;
    if (!LoadStoreCatalog(toc, tables, store)) { return 1; }
    unsigned packagesTested = 0;
    for (const ZStoreEntry &entry : store) {
        if (entry.data.singlePurchase == 0) { continue; }
        CProfileManager buyer = profile;
        buyer.purchasedPackages.clear();
        buyer.nativeArchive->records[18].payload.assign(4, 0);
        buyer.coins = static_cast<std::uint64_t>(entry.data.commonPrice) * 2;
        buyer.warbucks = static_cast<std::uint64_t>(entry.data.rarePrice) * 2;
        if (buyer.AcquireItem(entry.data, entry.data.requiredLevel) != ZPurchaseResult::Purchased ||
            !buyer.SaveToDisk(root / "package")) { return 1; }
        // Clear memory so this proves the serialized 1018 record gates buying.
        buyer.purchasedPackages.clear();
        if (!buyer.LoadFromDisk(root / "package") || !buyer.IsPackagePurchased(entry.ref)) { return 1; }
        const auto coins = buyer.coins, warbucks = buyer.warbucks;
        if (buyer.AcquireItem(entry.data, entry.data.requiredLevel) != ZPurchaseResult::Owned ||
            buyer.coins != coins || buyer.warbucks != warbucks) { return 1; }
        const auto &payload = buyer.nativeArchive->records[18].payload;
        if (payload.size() != 18 || Get32(payload, 0) != 1 || payload[9] != 22 || Get32(payload, 14) != 0) { return 1; }
        ++packagesTested;
    }
    if (packagesTested == 0) { return 1; }
    std::printf("[native-profile-check] packages=%u save-1018-reload second-charge-rejected zero-value-is-owned failures=0\n", packagesTested);
    CProfileManager fresh;
    fresh.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto freshPath = root / ("launch-" + std::to_string(GetTickCount64()));
    if (!LoadProfile(toc, tables, fresh, freshPath, root / "absent-source") || !fresh.firstLaunch ||
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
    std::vector<ZPlanetEntry> planets;
    if (!LoadPlanetCatalog(toc, tables, planets)) { return 1; }
    unsigned hordeRecords = 0;
    for (const auto &planet : planets) {
        for (unsigned index = 0; index < planet.missions.size(); ++index) {
            const auto &mission = planet.missions[index];
            if (mission.type != 2) { continue; }
            const auto &ref = planet.data.missions[index];
            // Isolated new-record fixture; source saves have no nonempty 1016.
            if (!RecordMissionWaves(fresh, mission.level, mission.value64 + 2, {true, false}) ||
                !RecordMissionScore(fresh, ref, 24000) || !RecordMissionScore(fresh, ref, 12000) ||
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
    struct RestoreConnection {
        bool previous = GameHostSettings().isConnected;
        ~RestoreConnection() { GameHostSettings().isConnected = previous; }
    } restoreConnection;
    GameHostSettings().isConnected = true;
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) / ("native-play-" + std::to_string(GetTickCount64()));
    if (!LoadProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
    for (unsigned planet = 0; planet < profile.nativeArchive->survivalLevels.size(); ++planet) {
        const CProfileManager before = profile;
        const auto planetPath = path / std::to_string(planet);
        // Only the isolated copy's wave progress is reset to test an actual update.
        profile.clearedWaves[planet] = 0;
        profile.perfectedWaves[planet].reset();
        const auto &level = profile.nativeArchive->survivalLevels[planet];
        std::vector<std::uint8_t> bytes;
        if (!tables.ReadSectionResource(level.packHash, ZGameSection::Level, level.localIndex, bytes)) { return 1; }
        CArrayInputStream input(bytes);
        CLevel::Template data;
        if (!data.Init(input) || input.Available() != 0 || !profile.SaveToDisk(planetPath)) { return 1; }
        CGameFlow context{profile, planetPath, planet};
        if (RunSurvivalStudy(bigDirectory, tables.GetPackName(data.mapRef.packHash), data.mapRef.localIndex, 0, -1,
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
        CChallengeManager challenges;
        if (!challenges.Bind(toc, tables, profile, 0) || challenges.current.empty() || challenges.cycleDay == 0) { return 1; }
        std::printf("[native-profile-play-check] planet=%u original-equipment active-slot=%u kills=%u waves=2 saved-reloaded failures=0\n",
            planet, profile.activeWeaponSlot, context.result.kills);
    }
    std::vector<ZPlanetEntry> planets;
    if (!LoadPlanetCatalog(toc, tables, planets)) { return 1; }
    for (const auto &planet : planets) {
        if (planet.missions.empty() || planet.missions[0].type != 2) { continue; }
        ZMissionEntry mission;
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
        CGameFlow context{profile, hordePath};
        context.hordeStart = 0;
        if (RunSurvivalStudy(bigDirectory, tables.GetPackName(map.packHash), map.localIndex, 0, -1,
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
