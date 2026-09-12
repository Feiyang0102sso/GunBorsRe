/** @file PickupCatalog.cpp
 * @brief Verify the entire wire payload and every real collection export.
 */
#include "gun_bros_re/data/PickupCatalog.h"
#include "gun_bros_re/data/StoreCatalog.h"
#include "gun_bros_re/gameplay/PickupScene.h"
#include "gun_bros_re/gameplay/CParticleEffect.h"
#include "gun_bros_re/gameplay/WeaponEffects.h"
#include "engine/platform/CWindow.h"
#include "engine/core/CMatrix4d.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>

bool LoadPickupCatalog(CResTOCManager &toc, PackTables &tables, std::vector<PickupEntry> &catalog) {
    catalog.clear();
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC *pack = toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(GameSection::Pickup);
        for (unsigned index = 0; index < count; ++index) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack->GetPackHash(), GameSection::Pickup, index, payload)) { return false; }
            PickupEntry entry;
            entry.ref.packHash = pack->GetPackHash();
            entry.ref.localIndex = static_cast<std::uint8_t>(index);
            entry.owner = pack->GetShortName() + " pickup " + std::to_string(index);
            CArrayInputStream stream(payload);
            if (!entry.data.Init(stream) || stream.Available() != 0) {
                std::printf("[pickup] invalid %s remaining=%zu overran=%d\n",
                    entry.owner.c_str(), stream.Available(), stream.Overran());
                return false;
            }
            entry.name = ReadGameString(toc, entry.data.name);
            catalog.push_back(std::move(entry));
        }
    }
    std::printf("[pickup] catalog=%zu\n", catalog.size());
    return !catalog.empty();
}
