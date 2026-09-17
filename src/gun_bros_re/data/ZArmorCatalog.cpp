/** @file ZArmorCatalog.cpp
 * @brief Verify every armour script, model and texture from original archives.
 */
#include "gun_bros_re/data/ZArmorCatalog.h"
#include "engine/graphics/ZPNG.h"
#include "engine/graphics/CMesh.h"
#include "gun_bros_re/gameplay/brother/ZPlayerModel.h"
#include "gun_bros_re/data/ZWeaponCatalog.h"
#include "engine/core/ZMatrix4d.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

bool LoadArmorCatalog(CResTOCManager &toc, ZPackTables &tables,
    std::vector<ZArmorEntry> &armor) {
    armor.clear();
    for (std::uint32_t packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC *pack = toc.GetPack(packIndex);
        const std::uint32_t count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Armor);
        for (std::uint32_t ordinal = 0; ordinal < count; ++ordinal) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack->GetPackHash(), ZGameSection::Armor, ordinal, payload)) {
                return false;
            }
            CArrayInputStream stream(payload);
            ZArmorEntry entry;
            entry.packHash = pack->GetPackHash();
            entry.ordinal = ordinal;
            entry.owner = pack->GetShortName() + " armor " + std::to_string(ordinal);
            if (!entry.data.Init(stream) || stream.Available() != 0 || entry.data.GetSlot() >= kArmorSlotCount) {
                std::printf("[armor] invalid template: %s\n", entry.owner.c_str());
                return false;
            }
            armor.push_back(entry);
        }
    }
    return !armor.empty();
}
