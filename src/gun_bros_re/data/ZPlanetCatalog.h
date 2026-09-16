/** @file ZPlanetCatalog.h
 * @brief Bind original PLANET -> MISSION references and the authored map slots.
 */
#ifndef GUN_BROS_RE_ZPLANETCATALOG_H
#define GUN_BROS_RE_ZPLANETCATALOG_H
#include "gun_bros_re/data/Planet.h"
#include "gun_bros_re/data/Mission.h"
#include "gun_bros_re/gameplay/CMissionScriptContext.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "gun_bros_re/data/ZPackTables.h"
#include <map>

struct ZPlanetMissionInfo {
    std::string title, description, requirements, overview;
    std::vector<CMissionScriptContext::Requirement> prerequisites;
    int requiredLevel = 0;
    unsigned waveCount = 0;
    GameObjectRef map;
};

struct ZPlanetEntry {
    GameObjectRef resource;
    Planet data;
    std::vector<Mission> missions;
    std::vector<ZPlanetMissionInfo> missionInfo;
};

/** CMenuMission::Bind :162800 uses Planet.mapSlot, never a pack-name table.
 * Host progress slots retain retail order, followed by horde and empty planets.
 * Disk layouts: entries/planet_entry.bt and entries/mission_entry.bt. */
inline bool LoadPlanetCatalog(CResTOCManager &toc, ZPackTables &tables, std::vector<ZPlanetEntry> &result) {
    std::map<unsigned, ZPlanetEntry> slots;
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const unsigned hash = toc.GetPack(packIndex)->GetPackHash();
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Planet);
        for (unsigned index = 0; index < count; ++index) {
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(hash, ZGameSection::Planet, index, bytes)) { return false; }
            CArrayInputStream input(bytes);
            ZPlanetEntry entry;
            entry.resource.packHash = hash;
            entry.resource.localIndex = index;
            if (!entry.data.Init(input) || input.Available() != 0) { return false; }
            for (const auto &ref : entry.data.missions) {
                if (!tables.ReadSectionResource(ref.packHash, ZGameSection::Mission, ref.localIndex, bytes)) { return false; }
                CArrayInputStream missionInput(bytes);
                Mission mission;
                if (!mission.Init(missionInput) || missionInput.Available() != 0) { return false; }
                CMissionScriptContext context;
                if (!context.Bind(mission.script)) { return false; }
                ZPlanetMissionInfo info;
                info.requiredLevel = context.level;
                info.prerequisites = context.requirements;
                info.title = ReadGameString(toc, mission.title);
                info.description = ReadGameString(toc, mission.description);
                info.requirements = ReadGameString(toc, mission.requirements);
                info.overview = ReadGameString(toc, mission.overview);
                if (!tables.ReadSectionResource(mission.level.packHash, ZGameSection::Level, mission.level.localIndex, bytes)) { return false; }
                CArrayInputStream levelInput(bytes);
                CLevel::Template level;
                if (!level.Init(levelInput) || levelInput.Available() != 0) { return false; }
                info.waveCount = level.wavesPerRevolution;
                info.map = level.mapRef;
                entry.missionInfo.push_back(std::move(info));
                entry.missions.push_back(std::move(mission));
            }
            slots[entry.data.mapSlot] = std::move(entry);
        }
    }
    result.clear();
    for (unsigned type : {1u, 2u, 0u}) {
        for (const auto &slot : slots) {
            unsigned entryType = 0;
            if (!slot.second.missions.empty()) { entryType = slot.second.missions.front().type; }
            if (entryType == type) { result.push_back(slot.second); }
        }
    }
    return result.size() == slots.size();
}
#endif
