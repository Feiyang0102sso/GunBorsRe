/** @file WeaponCatalog.cpp
 * @brief Weapon identity and store metadata, read directly from BIG archives.
 */
#include "gun_bros_re/data/WeaponCatalog.h"
#include "gun_bros_re/data/CStoreItem.h"
#include <cstdio>

namespace {
constexpr const char *kCategoryNames[] = {
    "pistol", "rifle", "shotgun", "spread", "heavy", "special", "laser"
};
constexpr GameSection kStoreSection = static_cast<GameSection>(23);

/** Resolve the string ordinal in the game keyset (following its 33 bases). */
std::string ReadWeaponName(CResTOCManager &toc, const CGameAssetRef &ref) {
    if (ref.assetId < 0) { return {}; }
    CResPackTOC *pack = toc.GetPack(toc.GetPackIndexFromHash(ref.packHash));
    CGameObjectPack objects;
    if (!objects.Init(*pack)) { return {}; }
    std::vector<std::uint8_t> payload;
    const std::uint32_t handle = objects.GetStringHandle(ref.assetId);
    if (handle == 0) { return {}; }
    if (!pack->GetResource(handle, payload)) { return {}; }
    // Localized strings use UTF-8 bytes in this archive's English locale.
    std::string name;
    for (std::uint8_t byte : payload) {
        if (byte == 0) { break; }
        name.push_back(static_cast<char>(byte));
    }
    return name;
}
}

const char *WeaponCategoryName(int category) {
    if (category < 0 || category >= kWeaponCategoryCount) { return "unclassified"; }
    return kCategoryNames[category];
}

bool LoadWeaponCatalog(CResTOCManager &toc, PackTables &tables,
                       std::vector<WeaponEntry> &weapons) {
    weapons.clear();
    for (std::uint32_t i = 0; i < toc.GetPackCount(); ++i) {
        CResPackTOC *pack = toc.GetPack(i);
        const std::uint32_t count = tables.GetObjectPack(i).GetObjectCount(GameSection::Gun);
        for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack->GetPackHash(), GameSection::Gun, ordinal, payload)) { return false; }
            CArrayInputStream stream(payload);
            WeaponEntry entry;
            entry.packHash = pack->GetPackHash();
            entry.ordinal = ordinal;
            if (!entry.data.Init(stream) || stream.Available() != 0) { return false; }
            entry.category = entry.data.GetCategory();
            // Retail classification confirmed by the original game's inventory.
            // Preserve the template's category and holding moves separately.
            // Correction: the old ordinal-specific overrides were not evidence.
            // STORE category is resolved from its actual typed reference below;
            // absent store metadata remains unknown, never an "unused" verdict.
            bool hasProjectile = !entry.data.GetBulletRef().IsNull();
            for (const ScriptResourceRef &ref : entry.data.GetScript().GetResources()) {
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
    for (std::uint32_t i = 0; i < toc.GetPackCount(); ++i) {
        CResPackTOC *pack = toc.GetPack(i);
        const std::uint32_t count = tables.GetObjectPack(i).GetObjectCount(kStoreSection);
        for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack->GetPackHash(), kStoreSection, ordinal, payload)) { return false; }
            CArrayInputStream stream(payload);
            CStoreItem item;
            if (!item.Init(stream) || stream.Available() != 0) { return false; }
            if (item.objects.size() != 1) { continue; } // Bundles must not rename their weapons.
            const auto &reference = item.objects.front();
            if (reference.type != 6) { continue; }
            for (WeaponEntry &weapon : weapons) {
                if (weapon.packHash != reference.object.packHash || weapon.ordinal != reference.object.localIndex) { continue; }
                // CStoreAggregator::CreateItemCategoryString :157379 reads +4,
                // distinct from CGun::Template+104 (gun_template/store_entry.bt).
                if (weapon.hasStoreEntry && weapon.category != item.type) {
                    std::printf("[weapon-catalog] conflicting store category gun=%u:%u store=%u:%u values=%d/%u\n",
                        weapon.packHash, weapon.ordinal, pack->GetPackHash(), ordinal, weapon.category, item.type);
                    return false;
                }
                weapon.hasStoreEntry = true;
                weapon.category = item.type;
                const std::string title = ReadWeaponName(toc, item.assets[2]);
                if (!title.empty()) { weapon.name = title; }
            }
        }
    }
    return !weapons.empty();
}

std::size_t SelectWeaponKey(const std::vector<WeaponEntry> &weapons,
                            std::size_t current, KeyCode key) {
    if (weapons.empty()) { return current; }
    const int categoryKey = static_cast<int>(key) - static_cast<int>(KeyCode::Digit1);
    if (categoryKey >= 0 && categoryKey < kWeaponCategoryCount) {
        if (weapons[current].category == categoryKey) { return current; }
        for (std::size_t i = 0; i < weapons.size(); ++i) {
            if (weapons[i].category == categoryKey) { return i; }
        }
    }
    if (key == KeyCode::N || key == KeyCode::M) {
        std::size_t next = current;
        for (std::size_t i = 0; i < weapons.size(); ++i) {
            if (key == KeyCode::M) { next = (next + 1) % weapons.size(); }
            else { next = (next + weapons.size() - 1) % weapons.size(); }
            if (weapons[next].category == weapons[current].category) { return next; }
        }
    }
    return current;
}

std::string WeaponSelectionLabel(const std::vector<WeaponEntry> &weapons,
                                 std::size_t current) {
    const WeaponEntry &weapon = weapons[current];
    int position = 0;
    int count = 0;
    for (std::size_t i = 0; i < weapons.size(); ++i) {
        if (weapons[i].category == weapon.category) {
            ++count;
            if (i == current) { position = count; }
        }
    }
    std::string label = std::string(WeaponCategoryName(weapon.category)) + " " +
        std::to_string(position) + "/" + std::to_string(count) + " | " + weapon.name;
    if (weapon.visualOnly) { label += " [visual only: no firing data]"; }
    if (!weapon.hasStoreEntry) { label += " [no single-item STORE reference]"; }
    return label;
}

