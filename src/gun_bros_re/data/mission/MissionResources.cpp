/** @file MissionResources.cpp
 * @brief Validate full records, level/map references and original objective text.
 */
#include "gun_bros_re/data/objects/CGameObjectPackObjects.h"
#include <cstdio>

/** Original CGameObjectPack::InitGameObject :129772 caches one typed object.
 * Parsing follows entries/mission_entry.bt (Init :164402); no resource values are generated.
 */
const Mission *Mission::Load(CResTOCManager &toc, CGunBros &tables, const GameObjectRef &ref) {
    const int packIndex = toc.GetPackIndexFromHash(ref.packHash);
    if (ref.IsNull() || ref.localIndex == 255 || packIndex < 0) { return nullptr; }
    auto &objects = tables.GetObjectPack(packIndex).GetObjects().missions;
    const auto found = objects.find(ref.localIndex);
    if (found != objects.end()) { return &found->second; }
    std::vector<std::uint8_t> bytes;
    if (!tables.ReadSectionResource(ref.packHash, ZGameSection::Mission, ref.localIndex, bytes)) { return nullptr; }
    CArrayInputStream input(bytes);
    Mission value;
    if (!value.Init(input) || input.Available() != 0) {
        std::printf("[object] invalid Mission pack=%u ordinal=%u remaining=%zu\n", ref.packHash, unsigned(ref.localIndex), input.Available());
        return nullptr;
    }
    const auto inserted = objects.emplace(ref.localIndex, std::move(value));
    return &inserted.first->second;
}

bool Mission::LoadEntries(CResTOCManager &toc, CGunBros &tables, std::vector<Mission::Entry> &catalog) {
    catalog.clear();
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC &pack = *toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Mission);
        for (unsigned index = 0; index < count; ++index) {
            Mission::Entry entry;
            entry.resource.packHash = pack.GetPackHash();
            entry.resource.localIndex = static_cast<std::uint8_t>(index);
            entry.owner = pack.GetShortName() + ":" + std::to_string(index);
            const auto *data = Load(toc, tables, entry.resource);
            if (data == nullptr) {
                std::printf("[mission] invalid %s\n", entry.owner.c_str());
                return false;
            }
            entry.data = *data;
            entry.title = tables.ReadString(entry.data.title);
            catalog.push_back(std::move(entry));
        }
    }
    return !catalog.empty();
}
