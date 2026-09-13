/** @file StoreCatalog.cpp
 * @brief Parse and validate progression and all store records independently.
 */
#include "gun_bros_re/data/StoreCatalog.h"
#include "gun_bros_re/data/CProfileManager.h"
#include "gun_bros_re/data/Planet.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>

int GetStoreDisplayOrder(const CStoreItem &item, const CProfileManager &profile) {
    // CStoreItemOverride::OverrideItem :233074 restores owned hidden entries
    // at order 10000. This is a native algorithm constant, not a resource value.
    if (item.displayOrder >= 0 || item.value242 != 0 || item.singlePurchase != 0) { return item.displayOrder; }
    for (const GameObjectTypeRef &object : item.objects) {
        if (profile.Owns(object.type, object.object)) { return 10000; }
        if (object.type == 17 && profile.GetPowerupCount(object.object) != 0) { return 10000; }
    }
    return item.displayOrder;
}

std::string ReadGameString(CResTOCManager &toc, const CGameAssetRef &ref) {
    if (ref.assetId < 0 || ref.IsNull()) { return {}; }
    CResPackTOC *pack = toc.GetPack(toc.GetPackIndexFromHash(ref.packHash));
    CGameObjectPack objects;
    if (!objects.Init(*pack)) { return {}; }
    std::vector<std::uint8_t> payload;
    const unsigned handle = objects.GetStringHandle(ref.assetId);
    if (handle == 0) { return {}; }
    if (!pack->GetResource(handle, payload)) { return {}; }
    std::string text;
    for (std::uint8_t byte : payload) {
        if (byte == 0) { break; }
        text.push_back(static_cast<char>(byte));
    }
    return text;
}

bool LoadStoreCatalog(CResTOCManager &toc, PackTables &tables, std::vector<StoreEntry> &catalog) {
    catalog.clear();
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC *pack = toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(GameSection::StoreItem);
        for (unsigned index = 0; index < count; ++index) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack->GetPackHash(), GameSection::StoreItem, index, payload)) { return false; }
            CArrayInputStream stream(payload);
            StoreEntry entry;
            entry.ref.packHash = pack->GetPackHash();
            entry.ref.localIndex = static_cast<std::uint8_t>(index);
            entry.data.resource = entry.ref;
            entry.owner = pack->GetShortName() + " store " + std::to_string(index);
            if (!entry.data.Init(stream) || stream.Available() != 0) {
                std::printf("[store] invalid record %s remaining=%zu\n", entry.owner.c_str(), stream.Available());
                return false;
            }
            entry.name = ReadGameString(toc, entry.data.assets[2]);
            if (entry.data.value32 == 1) { entry.productId = ReadGameString(toc, entry.data.assets[0]); }
            if (entry.name.empty()) { entry.name = entry.owner; }
            catalog.push_back(std::move(entry));
        }
    }
    return !catalog.empty();
}

int FindCurrencyOffer(const std::vector<StoreEntry> &catalog, unsigned currency, unsigned missing) {
    // CStoreAggregator::CacheLowestAppropriateIAPItem :155786 scans pack/store
    // order, requires the IAP flag and excludes conversions. No authored prices
    // or selected product IDs are copied into the host.
    int adequate = -1, largest = -1;
    unsigned adequateAmount = 0, largestAmount = 0;
    for (unsigned index = 0; index < catalog.size(); ++index) {
        const CStoreItem &item = catalog[index].data;
        if (item.value32 != 1 || item.type == 16) { continue; }
        unsigned amount = item.commonPrice;
        if (currency == 1) { amount = item.rarePrice; }
        if (amount >= missing && (adequateAmount == 0 || amount < adequateAmount)) {
            adequate = static_cast<int>(index);
            adequateAmount = amount;
        }
        if (largestAmount == 0 || amount > largestAmount) {
            largest = static_cast<int>(index);
            largestAmount = amount;
        }
    }
    if (adequate >= 0) { return adequate; }
    return largest;
}

bool LoadPlayerProgress(CResTOCManager &toc, PackTables &tables, CPlayerProgress::Template &data) {
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        // Progress is a singleton data table, not a script-spawned object;
        // OBJECT_SCRIPT_COUNTS can report zero while its section exists.
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(toc.GetPack(packIndex)->GetPackHash(), GameSection::PlayerProgress, 0, payload)) { continue; }
        if (payload.empty()) { continue; }
        CArrayInputStream stream(payload);
        if (!data.Init(stream) || stream.Available() != 0) { continue; }
        std::printf("[progress] levels=%u initial-health=%d tail=%.2f/%d\n",
            data.GetMaximumLevel(), data.health[1], data.value20, data.value24);
        return true;
    }
    std::printf("[progress] valid PLAYER_PROGRESS table missing\n");
    return false;
}

bool LoadRefinementTemplate(CResTOCManager &toc, PackTables &tables, CRefinementManager::Template &data) {
    std::vector<std::uint8_t> payload;
    const unsigned hash = toc.GetPack(toc.GetCorePackIndex())->GetPackHash();
    if (!tables.ReadSectionResource(hash, GameSection::RefinementManager, 0, payload)) { return false; }
    CArrayInputStream stream(payload);
    return data.Init(stream) && stream.Available() == 0;
}
