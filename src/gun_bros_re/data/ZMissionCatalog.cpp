/** @file ZMissionCatalog.cpp
 * @brief Validate full records, level/map references and original objective text.
 */
#include "gun_bros_re/data/ZMissionCatalog.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "gun_bros_re/data/MissionObjective.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/map/CMap.h"
#include "gun_bros_re/gameplay/CGameSession.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include "gun_bros_re/data/ZMissionCatalogInternal.h"
using namespace MissionCatalogDetail;

namespace MissionCatalogDetail {

}

bool LoadMissionCatalog(CResTOCManager &toc, ZPackTables &tables, std::vector<ZMissionEntry> &catalog) {
    catalog.clear();
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC &pack = *toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Mission);
        for (unsigned index = 0; index < count; ++index) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack.GetPackHash(), ZGameSection::Mission, index, payload)) { return false; }
            ZMissionEntry entry;
            entry.resource.packHash = pack.GetPackHash();
            entry.resource.localIndex = static_cast<std::uint8_t>(index);
            entry.owner = pack.GetShortName() + ":" + std::to_string(index);
            CArrayInputStream stream(payload);
            if (!entry.data.Init(stream) || stream.Available() != 0) {
                std::printf("[mission] invalid %s remaining=%zu\n", entry.owner.c_str(), stream.Available());
                return false;
            }
            entry.title = ReadGameString(toc, entry.data.title);
            catalog.push_back(std::move(entry));
        }
    }
    return !catalog.empty();
}
