/** @file PlanetCatalog.h
 * @brief Bind original PLANET -> MISSION references and the authored map slots.
 */
#ifndef GUN_BROS_RE_PLANETCATALOG_H
#define GUN_BROS_RE_PLANETCATALOG_H
#include "gun_bros/Planet.h"
#include "gun_bros/Mission.h"
#include "gun_bros/CMissionScriptContext.h"
#include "gun_bros/CLevel.h"
#include "runtime/StoreCatalog.h"
#include "runtime/PackTables.h"
#include <map>

struct PlanetMissionInfo {
    std::string title, description, requirements, overview;
    std::vector<CMissionScriptContext::Requirement> prerequisites;
    int requiredLevel = 0;
    unsigned waveCount = 0;
    GameObjectRef map;
};

struct PlanetEntry {
    GameObjectRef resource;
    Planet data;
    std::vector<Mission> missions;
    std::vector<PlanetMissionInfo> missionInfo;
};

/** CMenuMission::Bind :162800 uses Planet.mapSlot, never a pack-name table.
 * Host progress slots retain retail order, followed by horde and empty planets.
 * Disk layouts: entries/planet_entry.bt and entries/mission_entry.bt. */
inline bool LoadPlanetCatalog(CResTOCManager &toc, PackTables &tables, std::vector<PlanetEntry> &result) {
    std::map<unsigned, PlanetEntry> slots;
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const unsigned hash = toc.GetPack(packIndex)->GetPackHash();
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(GameSection::Planet);
        for (unsigned index = 0; index < count; ++index) {
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(hash, GameSection::Planet, index, bytes)) { return false; }
            CArrayInputStream input(bytes);
            PlanetEntry entry;
            entry.resource.packHash = hash;
            entry.resource.localIndex = index;
            if (!entry.data.Init(input) || input.Available() != 0) { return false; }
            for (const auto &ref : entry.data.missions) {
                if (!tables.ReadSectionResource(ref.packHash, GameSection::Mission, ref.localIndex, bytes)) { return false; }
                CArrayInputStream missionInput(bytes);
                Mission mission;
                if (!mission.Init(missionInput) || missionInput.Available() != 0) { return false; }
                CMissionScriptContext context;
                if (!context.Bind(mission.script)) { return false; }
                PlanetMissionInfo info;
                info.requiredLevel = context.level;
                info.prerequisites = context.requirements;
                info.title = ReadGameString(toc, mission.title);
                info.description = ReadGameString(toc, mission.description);
                info.requirements = ReadGameString(toc, mission.requirements);
                info.overview = ReadGameString(toc, mission.overview);
                if (!tables.ReadSectionResource(mission.level.packHash, GameSection::Level, mission.level.localIndex, bytes)) { return false; }
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
