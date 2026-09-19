/** @file CPowerupResources.cpp
 * @brief Read the authoritative POWERUP templates from BIG.
 */
#include "gun_bros_re/data/objects/CGameObjectPackObjects.h"
#include <cstdio>

/** Original CGameObjectPack::InitGameObject :129772 caches one typed object.
 * Parsing follows entries/powerup_template.bt (Init :187947); no resource values are generated.
 */
const CPowerup::Template *CPowerup::Template::Load(CResTOCManager &toc, CGunBros &tables, const GameObjectRef &ref) {
    const int packIndex = toc.GetPackIndexFromHash(ref.packHash);
    if (ref.IsNull() || ref.localIndex == 255 || packIndex < 0) { return nullptr; }
    auto &objects = tables.GetObjectPack(packIndex).GetObjects().powerups;
    const auto found = objects.find(ref.localIndex);
    if (found != objects.end()) { return &found->second; }
    std::vector<std::uint8_t> bytes;
    if (!tables.ReadSectionResource(ref.packHash, ZGameSection::Powerup, ref.localIndex, bytes)) { return nullptr; }
    CArrayInputStream input(bytes);
    CPowerup::Template value;
    if (!value.Init(input) || input.Available() != 0) {
        std::printf("[object] invalid Powerup pack=%u ordinal=%u remaining=%zu\n", ref.packHash, unsigned(ref.localIndex), input.Available());
        return nullptr;
    }
    const auto inserted = objects.emplace(ref.localIndex, std::move(value));
    return &inserted.first->second;
}

bool CPowerup::LoadEntries(CResTOCManager &toc, CGunBros &tables, std::vector<CPowerup::Entry> &catalog) {
    catalog.clear();
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC &pack = *toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Powerup);
        for (unsigned index = 0; index < count; ++index) {
            CPowerup::Entry entry;
            entry.resource.packHash = pack.GetPackHash();
            entry.resource.localIndex = static_cast<std::uint8_t>(index);
            entry.owner = pack.GetShortName() + ":" + std::to_string(index);
            const auto *data = Template::Load(toc, tables, entry.resource);
            if (data == nullptr) {
                std::printf("[powerup] invalid %s\n", entry.owner.c_str());
                return false;
            }
            entry.data = *data;
            entry.name = tables.ReadString(entry.data.name);
            catalog.push_back(std::move(entry));
        }
    }
    std::printf("[powerup] catalog=%zu\n", catalog.size());
    return !catalog.empty();
}
