/** @file CArmorResources.cpp
 * @brief Verify every armour script, model and texture from original archives.
 */
#include "gun_bros_re/data/objects/CGameObjectPackObjects.h"
#include <cstdio>

/** Original CGameObjectPack::InitGameObject :129772 caches one typed object.
 * Parsing follows entries/armor_template.bt (Init :176438); no resource values are generated.
 */
const CArmor::Template *CArmor::Template::Load(CResTOCManager &toc, CGunBros &tables, const GameObjectRef &ref) {
    const int packIndex = toc.GetPackIndexFromHash(ref.packHash);
    if (ref.IsNull() || ref.localIndex == 255 || packIndex < 0) { return nullptr; }
    auto &objects = tables.GetObjectPack(packIndex).GetObjects().armor;
    const auto found = objects.find(ref.localIndex);
    if (found != objects.end()) { return &found->second; }
    std::vector<std::uint8_t> bytes;
    if (!tables.ReadSectionResource(ref.packHash, ZGameSection::Armor, ref.localIndex, bytes)) { return nullptr; }
    CArrayInputStream input(bytes);
    CArmor::Template value;
    if (!value.Init(input) || input.Available() != 0) {
        std::printf("[object] invalid Armor pack=%u ordinal=%u remaining=%zu\n", ref.packHash, unsigned(ref.localIndex), input.Available());
        return nullptr;
    }
    const auto inserted = objects.emplace(ref.localIndex, std::move(value));
    return &inserted.first->second;
}

bool CArmor::LoadEntries(CResTOCManager &toc, CGunBros &tables,
    std::vector<CArmor::Entry> &armor) {
    armor.clear();
    for (std::uint32_t packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC *pack = toc.GetPack(packIndex);
        const std::uint32_t count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Armor);
        for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
            CArmor::Entry entry;
            entry.packHash = pack->GetPackHash();
            entry.ordinal = ordinal;
            entry.owner = pack->GetShortName() + " armor " + std::to_string(ordinal);
            const auto *data = Template::Load(toc, tables, GameObjectRef{entry.packHash, static_cast<std::uint8_t>(ordinal)});
            if (data == nullptr || data->GetSlot() >= kArmorSlotCount) {
                std::printf("[armor] invalid template: %s\n", entry.owner.c_str());
                return false;
            }
            entry.data = *data;
            armor.push_back(entry);
        }
    }
    return !armor.empty();
}
