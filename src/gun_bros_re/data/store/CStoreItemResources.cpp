/** @file CStoreItemResources.cpp
 * @brief Parse and validate progression and all store records independently.
 */
#include "gun_bros_re/data/objects/CGameObjectPackObjects.h"
#include <cstdio>

/** Original CGameObjectPack::InitGameObject :129772 caches one typed object.
 * Parsing follows entries/store_entry.bt (Init :159834); no resource values are generated.
 */
const CStoreItem *CStoreItem::Load(CResTOCManager &toc, CGunBros &tables, const GameObjectRef &ref) {
    const int packIndex = toc.GetPackIndexFromHash(ref.packHash);
    if (ref.IsNull() || ref.localIndex == 255 || packIndex < 0) { return nullptr; }
    auto &objects = tables.GetObjectPack(packIndex).GetObjects().store;
    const auto found = objects.find(ref.localIndex);
    if (found != objects.end()) { return &found->second; }
    std::vector<std::uint8_t> bytes;
    if (!tables.ReadSectionResource(ref.packHash, ZGameSection::StoreItem, ref.localIndex, bytes)) { return nullptr; }
    CArrayInputStream input(bytes);
    CStoreItem value;
    if (!value.Init(input) || input.Available() != 0) {
        std::printf("[object] invalid StoreItem pack=%u ordinal=%u remaining=%zu\n", ref.packHash, unsigned(ref.localIndex), input.Available());
        return nullptr;
    }
    value.resource = ref;
    const auto inserted = objects.emplace(ref.localIndex, std::move(value));
    return &inserted.first->second;
}

bool CStoreItem::LoadEntries(CResTOCManager &toc, CGunBros &tables, std::vector<CStoreItem::Entry> &catalog) {
    catalog.clear();
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC *pack = toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::StoreItem);
        for (unsigned index = 0; index < count; ++index) {
            CStoreItem::Entry entry;
            entry.ref.packHash = pack->GetPackHash();
            entry.ref.localIndex = static_cast<std::uint8_t>(index);
            entry.data.resource = entry.ref;
            entry.owner = pack->GetShortName() + " store " + std::to_string(index);
            const auto *data = Load(toc, tables, entry.ref);
            if (data == nullptr) {
                std::printf("[store] invalid record %s\n", entry.owner.c_str());
                return false;
            }
            entry.data = *data;
            entry.name = tables.ReadString(entry.data.assets[2]);
            if (entry.data.value32 == 1) { entry.productId = tables.ReadString(entry.data.assets[0]); }
            if (entry.name.empty()) { entry.name = entry.owner; }
            catalog.push_back(std::move(entry));
        }
    }
    return !catalog.empty();
}
