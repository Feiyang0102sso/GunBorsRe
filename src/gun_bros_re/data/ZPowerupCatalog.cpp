/** @file ZPowerupCatalog.cpp
 * @brief Read authoritative records and enumerate original use actions.
 */
#include "gun_bros_re/data/ZPowerupCatalog.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "engine/core/CStringToKey.h"
#include "gun_bros_re/gameplay/CBullet.h"
#include "gun_bros_re/gameplay/CTargetingController.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <iomanip>

bool LoadPowerupCatalog(CResTOCManager &toc, ZPackTables &tables, std::vector<ZPowerupEntry> &catalog) {
    catalog.clear();
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC &pack = *toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Powerup);
        for (unsigned index = 0; index < count; ++index) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack.GetPackHash(), ZGameSection::Powerup, index, payload)) { return false; }
            CArrayInputStream stream(payload);
            ZPowerupEntry entry;
            entry.resource.packHash = pack.GetPackHash();
            entry.resource.localIndex = static_cast<std::uint8_t>(index);
            entry.owner = pack.GetShortName() + ":" + std::to_string(index);
            if (!entry.data.Init(stream) || stream.Available() != 0) {
                std::printf("[powerup] invalid %s remaining=%zu\n", entry.owner.c_str(), stream.Available());
                return false;
            }
            entry.name = ReadGameString(toc, entry.data.name);
            catalog.push_back(std::move(entry));
        }
    }
    std::printf("[powerup] catalog=%zu\n", catalog.size());
    return !catalog.empty();
}

bool IsPlayablePowerup(const GameObjectRef &resource) {
    if (resource.packHash != CStringToKey("pack5")) { return false; }
    const unsigned index = resource.localIndex;
    return index == 0 || index == 1 || (index >= 5 && index <= 19);
}
