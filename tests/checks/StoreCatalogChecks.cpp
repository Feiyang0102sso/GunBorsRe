/** @file StoreCatalog.cpp
 * @brief Parse and validate progression and all store records independently.
 */
#include "TestOutput.h"
#include "gun_bros_re/data/StoreCatalog.h"
#include "gun_bros_re/data/CProfileManager.h"
#include "gun_bros_re/data/Planet.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include "Checks.h"

int RunProgressCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CPlayerProgress::Template data;
    std::vector<StoreEntry> catalog;
    if (!LoadPlayerProgress(toc, tables, data) || !LoadStoreCatalog(toc, tables, catalog)) { return 1; }
    std::filesystem::create_directories(TestOutput::Path(""));
    std::ofstream report(TestOutput::Path("progress-check.txt"));
    std::ofstream storeReport(TestOutput::Path("store-check.txt"));
    if (!report || !storeReport) { return 1; }
    int failures = 0;
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC *pack = toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(GameSection::Planet);
        for (unsigned index = 0; index < count; ++index) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack->GetPackHash(), GameSection::Planet, index, payload)) { ++failures; continue; }
            CArrayInputStream stream(payload);
            Planet planet;
            if (!planet.Init(stream) || stream.Available() != 0) { ++failures; continue; }
            report << "planet=" << pack->GetShortName() << ':' << index << " name=" << ReadGameString(toc, planet.name)
                << " large=" << tables.GetPackName(planet.largeImage.packHash) << ':' << unsigned(planet.largeImage.archetype)
                << ':' << unsigned(planet.largeImage.animation) << " missions=" << planet.missions.size() << '\n';
        }
    }
    CRefinementManager::Template refinementData;
    if (!LoadRefinementTemplate(toc, tables, refinementData)) { return 1; }
    CRefinementManager refinery;
    refinery.Bind(refinementData);
    for (unsigned index = 0; index < kRefinementSlotCount; ++index) {
        report << "refinery=" << index << " minutes=" << refinementData.minutes[index]
            << " efficiency=" << refinementData.efficiencyPercent[index]
            << " common=" << refinementData.commonPrice[index] << " rare=" << refinementData.rarePrice[index]
            << " gate=" << unsigned(refinementData.gate[index]) << " state=" << refinery.slots[index].state << '\n';
    }
    // Commit, premature collection, elapsed real time and double collection.
    std::uint64_t xplodium = 1000;
    std::uint64_t coins = 0;
    refinery.slots[0].state = 1;
    if (!refinery.BeginRefinement(0, 0, 1000, xplodium, 100) || xplodium != 0) { ++failures; }
    if (refinementData.minutes[0] > 0 && refinery.CollectResources(0, coins)) { ++failures; }
    refinery.UpdateRefinement(100 + static_cast<std::int64_t>(refinementData.minutes[0]) * 60);
    if (!refinery.CollectResources(0, coins) || coins != 10ULL * refinementData.efficiencyPercent[0] ||
        refinery.CollectResources(0, coins)) { ++failures; }
    xplodium = 100;
    std::uint64_t warbucks = 0;
    if (refinery.BeginRefinement(0, 1, 100, xplodium, 0) || xplodium != 100 ||
        refinery.UnlockSlot(7, coins, warbucks)) { ++failures; }
    if (!refinery.BeginRefinement(6, 6, 100, xplodium, 0) || refinery.slots[7].state != 1 ||
        refinery.slots[8].state != 0) { ++failures; }
    CProfileManager profile;
    const unsigned coreHash = toc.GetPack(toc.GetCorePackIndex())->GetPackHash();
    profile.Reset(coreHash, refinementData);
    bool testedPurchase = false;
    // User regression: a consumable-containing, one-time package must never
    // charge again. Select by the original flag, not by a translated name.
    for (const StoreEntry &entry : catalog) {
        if (entry.data.singlePurchase == 0) { continue; }
        CProfileManager packageBuyer;
        packageBuyer.Reset(coreHash, refinementData);
        packageBuyer.coins = entry.data.commonPrice * 2ULL;
        packageBuyer.warbucks = entry.data.rarePrice * 2ULL;
        const auto first = packageBuyer.AcquireItem(entry.data, data.GetMaximumLevel());
        const auto balance = packageBuyer.warbucks;
        const auto second = packageBuyer.AcquireItem(entry.data, data.GetMaximumLevel());
        const bool passed = first == PurchaseResult::Purchased && second == PurchaseResult::Owned && packageBuyer.warbucks == balance;
        std::printf("[package-once-check] resource=%u:%u first=%u second=%u unchanged-balance=%d passed=%d\n",
            entry.ref.packHash, entry.ref.localIndex, unsigned(first), unsigned(second), packageBuyer.warbucks == balance, passed);
        if (!passed) { ++failures; }
    }
    for (const StoreEntry &entry : catalog) {
        if (entry.name != "Mad Dogs") { continue; }
        const CStoreItem &item = entry.data;
        profile.coins = item.commonPrice - 1;
        if (profile.AcquireItem(item, 1) != PurchaseResult::InsufficientCoins ||
            profile.coins != item.commonPrice - 1) { ++failures; }
        profile.coins = item.commonPrice;
        if (profile.AcquireItem(item, 1) != PurchaseResult::Purchased || profile.coins != 0 ||
            profile.AcquireItem(item, 1) != PurchaseResult::Owned) { ++failures; }
        profile.configuration.guns[1] = item.objects[0].object;
        testedPurchase = true;
    }
    if (!testedPurchase) { ++failures; }
    CProfileManager consumableBuyer;
    consumableBuyer.Reset(coreHash, refinementData);
    bool testedBundle = false;
    for (const StoreEntry &entry : catalog) {
        if (entry.name != "F.R.A.G. Grenade" || entry.data.objects.size() != 10) { continue; }
        const auto &ref = entry.data.objects[0].object;
        consumableBuyer.warbucks = entry.data.rarePrice - 1;
        if (consumableBuyer.AcquireItem(entry.data, 1) != PurchaseResult::InsufficientWarbucks ||
            consumableBuyer.GetPowerupCount(ref) != 0) { ++failures; }
        consumableBuyer.warbucks = entry.data.rarePrice * 2;
        if (consumableBuyer.AcquireItem(entry.data, 1) != PurchaseResult::Purchased ||
            consumableBuyer.AcquireItem(entry.data, 1) != PurchaseResult::Purchased ||
            consumableBuyer.GetPowerupCount(ref) != 20 || consumableBuyer.warbucks != 0 ||
            !consumableBuyer.ConsumePowerup(ref) || consumableBuyer.GetPowerupCount(ref) != 19 ||
            consumableBuyer.ConsumePowerup(ref, 20)) { ++failures; }
        testedBundle = true;
    }
    if (!testedBundle) { ++failures; }
    profile.experience = 105;
    profile.xplodium = 321;
    profile.clearedWaves[2] = 50;
    profile.stat42Bits = 0x80000012u;
    profile.refinery.slots[1].state = 1;
    if (!profile.refinery.BeginRefinement(1, 1, 100, profile.xplodium, 1000)) { ++failures; }
    const std::filesystem::path savePath = TestOutput::Path("profile-check.dat");
    if (!profile.SaveToDisk(savePath)) { ++failures; }
    CProfileManager reloaded;
    reloaded.Reset(coreHash, refinementData);
    if (!reloaded.LoadFromDisk(savePath) || reloaded.experience != 105 || reloaded.xplodium != 221 ||
        reloaded.clearedWaves[2] != 50 || reloaded.stat42Bits != 0x80000012u || reloaded.inventory.size() != profile.inventory.size() ||
        reloaded.configuration.guns[1].packHash != profile.configuration.guns[1].packHash) { ++failures; }
    reloaded.refinery.UpdateRefinement(1300);
    if (!reloaded.refinery.CollectResources(1, reloaded.coins) || reloaded.coins != 120) { ++failures; }
    // A second save exercises replacement; malformed input must leave it intact.
    if (!reloaded.SaveToDisk(savePath)) { ++failures; }
    // A genuine v1 shape has no consumable-count line before END.
    std::ifstream currentProfile(savePath);
    std::vector<std::string> lines;
    std::string savedLine;
    while (std::getline(currentProfile, savedLine)) { lines.push_back(savedLine); }
    // v3-v5 append settings and Horde records after the consumable section.
    // The v1 boundary is the end of the twelve refinery records, not END - 1.
    const std::size_t legacyLineCount = 4 + reloaded.configuration.guns.size() +
        reloaded.configuration.armor.size() + reloaded.inventory.size() + reloaded.refinery.slots.size();
    if (lines.size() <= legacyLineCount || lines[legacyLineCount] != "0") { ++failures; }
    else {
        lines[0] = "GUNBROS_RE_PROFILE 1";
        lines.resize(legacyLineCount);
        lines.push_back("END");
        std::ofstream legacyProfile(TestOutput::Path("profile-check-v1.dat"));
        for (const std::string &line : lines) { legacyProfile << line << '\n'; }
        legacyProfile.close();
        if (!reloaded.LoadFromDisk(TestOutput::Path("profile-check-v1.dat")) || reloaded.experience != 105 ||
            reloaded.coins != 120 || !reloaded.powerups.empty()) { ++failures; }
    }
    std::ofstream corrupt(TestOutput::Path("profile-check-truncated.dat"));
    corrupt << "GUNBROS_RE_PROFILE 1\n999 999";
    corrupt.close();
    if (reloaded.LoadFromDisk(TestOutput::Path("profile-check-truncated.dat")) || reloaded.experience != 105 || reloaded.coins != 120) { ++failures; }
    CPlayerProgress progress;
    progress.Bind(data);
    for (unsigned level = 1; level <= data.GetMaximumLevel(); ++level) {
        const std::uint64_t threshold = data.GetExperienceForLevel(level);
        progress.SetExperience(threshold);
        if (progress.GetLevel() != level || progress.GetHealth() <= 0) { ++failures; }
        if (level > 1) {
            progress.SetExperience(threshold - 1);
            if (progress.GetLevel() != level - 1 || !progress.AddExperience(1) || progress.GetLevel() != level) { ++failures; }
        }
        report << "level=" << level << " total=" << threshold << " delta=" << data.experience[level]
            << " health=" << data.health[level] << '\n';
    }
    unsigned references = 0;
    for (const StoreEntry &entry : catalog) {
        const CStoreItem &item = entry.data;
        storeReport << entry.owner << " name=" << std::quoted(entry.name) << " type=" << unsigned(item.type)
            << " flags=" << unsigned(item.flags) << " level=" << item.requiredLevel
            << " common=" << item.commonPrice << " rare=" << item.rarePrice
            << " value8=" << item.value8 << " value32=" << unsigned(item.value32)
            << " order=" << item.displayOrder << " value242=" << unsigned(item.value242)
            << " single=" << unsigned(item.singlePurchase) << " value244=" << unsigned(item.value244)
            << " refs=" << item.objects.size();
        for (const GameObjectTypeRef &ref : item.objects) {
            if (ref.object.IsNull()) { continue; }
            ++references;
            storeReport << " [" << unsigned(ref.type) << ':' << tables.GetPackName(ref.object.packHash)
                << ':' << unsigned(ref.object.localIndex) << ']';
            std::vector<std::uint8_t> payload;
            if (ref.type >= kObjectTypeCount || !tables.ReadSectionResource(ref.object.packHash,
                static_cast<GameSection>(ref.type + 1), ref.object.localIndex, payload)) { ++failures; }
        }
        for (unsigned group = 0; group < item.statGroups.size(); ++group) {
            storeReport << " stat" << group << '=';
            for (std::int32_t value : item.statGroups[group]) { storeReport << value << ','; }
        }
        // Asset slots also carry the display strings; naming them needs the
        // actual text, not a guess about which index holds the description.
        for (unsigned asset = 0; asset < 6; ++asset) {
            std::string text = ReadGameString(toc, item.assets[asset]);
            for (char &letter : text) {
                if (letter == '\n' || letter == '\r') { letter = ' '; }
            }
            storeReport << " asset" << asset << '=' << std::quoted(text);
        }
        storeReport << '\n';
    }
    std::printf("[progress-check] levels=%u store=%zu references=%u failures=%d\n",
        data.GetMaximumLevel(), catalog.size(), references, failures);
    return failures != 0;
}