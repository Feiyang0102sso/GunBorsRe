#include "gun_bros_re/ui/menus/CMenuMission.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include <map>
namespace MenuDetail {
/** CMenuMission::Bind :162800 uses Planet.mapSlot, never a pack-name table.
 * Host progress slots retain retail order, followed by horde and empty planets.
 * Disk layouts: entries/planet_entry.bt and entries/mission_entry.bt. */
bool CMenuMission::LoadPlanets(CResTOCManager &toc, CGunBros &tables, std::vector<MenuDetail::CMenuMission::PlanetEntry> &result) {
    std::map<unsigned, MenuDetail::CMenuMission::PlanetEntry> slots;
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const unsigned hash = toc.GetPack(packIndex)->GetPackHash();
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Planet);
        for (unsigned index = 0; index < count; ++index) {
            std::vector<std::uint8_t> bytes;
            MenuDetail::CMenuMission::PlanetEntry entry;
            entry.resource.packHash = hash;
            entry.resource.localIndex = index;
            const Planet *planet = Planet::Load(toc, tables, entry.resource);
            if (planet == nullptr) { return false; }
            entry.data = *planet;
            for (const auto &ref : entry.data.missions) {
                const Mission *sourceMission = Mission::Load(toc, tables, ref);
                if (sourceMission == nullptr) { return false; }
                Mission mission = *sourceMission;
                CMissionScriptContext context;
                if (!context.Bind(mission.script)) { return false; }
                MenuDetail::CMenuMission::MissionInfo info;
                info.requiredLevel = context.level;
                info.prerequisites = context.requirements;
                info.title = tables.ReadString(mission.title);
                info.description = tables.ReadString(mission.description);
                info.requirements = tables.ReadString(mission.requirements);
                info.overview = tables.ReadString(mission.overview);
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
}
