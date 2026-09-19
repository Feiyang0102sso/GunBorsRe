/** @file CGunResources.cpp
 * @brief Weapon identity and store metadata, read directly from BIG archives.
 */
#include "gun_bros_re/data/objects/CGameObjectPackObjects.h"
#include <cstdio>

/** Original CGameObjectPack::InitGameObject :129772 caches one typed object.
 * Parsing follows entries/gun_template.bt (Init :127712); no resource values are generated.
 */
const CGun::Template *CGun::Template::Load(CResTOCManager &toc, CGunBros &tables, const GameObjectRef &ref) {
    const int packIndex = toc.GetPackIndexFromHash(ref.packHash);
    if (ref.IsNull() || ref.localIndex == 255 || packIndex < 0) { return nullptr; }
    auto &objects = tables.GetObjectPack(packIndex).GetObjects().guns;
    const auto found = objects.find(ref.localIndex);
    if (found != objects.end()) { return &found->second; }
    std::vector<std::uint8_t> bytes;
    if (!tables.ReadSectionResource(ref.packHash, ZGameSection::Gun, ref.localIndex, bytes)) { return nullptr; }
    CArrayInputStream input(bytes);
    CGun::Template value;
    if (!value.Init(input) || input.Available() != 0) {
        std::printf("[object] invalid Gun pack=%u ordinal=%u remaining=%zu\n", ref.packHash, unsigned(ref.localIndex), input.Available());
        return nullptr;
    }
    const auto inserted = objects.emplace(ref.localIndex, std::move(value));
    return &inserted.first->second;
}

bool CGun::LoadEntries(CResTOCManager &toc, CGunBros &tables,
                       std::vector<CGun::Entry> &weapons) {
    weapons.clear();
    for (std::uint32_t i = 0; i < toc.GetPackCount(); ++i) {
        CResPackTOC *pack = toc.GetPack(i);
        const std::uint32_t count = tables.GetObjectPack(i).GetObjectCount(ZGameSection::Gun);
        for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
            CGun::Entry entry;
            entry.packHash = pack->GetPackHash();
            entry.ordinal = ordinal;
            const auto *data = Template::Load(toc, tables, GameObjectRef{entry.packHash, static_cast<std::uint8_t>(ordinal)});
            if (data == nullptr) { return false; }
            entry.data = *data;
            entry.category = entry.data.GetCategory();
            // Retail classification confirmed by the original game's inventory.
            // Preserve the template's category and holding moves separately.
            // Correction: the old ordinal-specific overrides were not evidence.
            // STORE category is resolved from its actual typed reference below;
            // absent store metadata remains unknown, never an "unused" verdict.
            bool hasProjectile = !entry.data.GetBulletRef().IsNull();
            for (const ZScriptResourceRef &ref : entry.data.GetScript().GetResources()) {
                // Script type 3 is CBullet (section 4); sound-only references
                // cannot supply a missing default projectile.
                if (ref.sectionOrType == 3 && ref.packHash != 0) { hasProjectile = true; }
            }
            entry.visualOnly = entry.data.GetScript().GetExportFunctions().empty() || !hasProjectile;
            entry.owner = pack->GetShortName() + " gun " + std::to_string(ordinal);
            entry.name = entry.owner;
            weapons.push_back(entry);
        }
    }
// CStoreItem::Init (:159834): type, flags, value, typed object references,
// price fields, then six asset references. The third asset is the name.
// CStoreAggregator::CreateItemCategoryString :157379 reads +4,
// distinct from CGun::Template+104 (gun_template/store_entry.bt).
    std::vector<CStoreItem::Entry> store;
    if (!CStoreItem::LoadEntries(toc, tables, store)) { return false; }
    for (const CStoreItem::Entry &record : store) {
        const CStoreItem &item = record.data;
        // Bundles must not rename their weapons.
        if (item.objects.size() != 1 || item.objects.front().type != 6) { continue; }
        const GameObjectRef &reference = item.objects.front().object;
        for (Entry &weapon : weapons) {
            if (weapon.packHash != reference.packHash || weapon.ordinal != reference.localIndex) { continue; }
            if (weapon.hasStoreEntry && weapon.category != item.type) {
                std::printf("[weapon] conflicting store category pack=%u ordinal=%u\n", reference.packHash, reference.localIndex);
                return false;
            }
            weapon.hasStoreEntry = true;
            weapon.category = item.type;
            if (!record.name.empty()) { weapon.name = record.name; }
        }
    }
    return !weapons.empty();
}
