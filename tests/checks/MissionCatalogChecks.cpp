/** @file ZMissionCatalog.cpp
 * @brief Validate full records, level/map references and original objective text.
 */
#include "TestOutput.h"
#include "gun_bros_re/data/ZMissionCatalog.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "gun_bros_re/data/MissionObjective.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/map/CMap.h"
#include "gun_bros_re/gameplay/game/CGameSession.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include "gun_bros_re/data/ZMissionCatalogInternal.h"
using namespace MissionCatalogDetail;
#include "Checks.h"

int RunMissionCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    std::vector<ZMissionEntry> catalog;
    if (!LoadMissionCatalog(toc, tables, catalog)) { return 1; }
    std::filesystem::create_directories(TestOutput::Path(""));
    std::ofstream report(TestOutput::Path("mission-check.txt"));
    unsigned failures = 0, objectives = 0;
    for (const ZMissionEntry &entry : catalog) {
        const Mission &mission = entry.data;
        report << entry.owner << " title=" << std::quoted(entry.title) << " type=" << mission.type
            << " fields=" << mission.value64 << ',' << mission.value66 << " objectives=" << mission.objectives.size()
            << " description=" << std::quoted(ReadGameString(toc, mission.description)) << '\n';
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(mission.level.packHash, ZGameSection::Level, mission.level.localIndex, payload)) { ++failures; continue; }
        CArrayInputStream levelStream(payload);
        CLevel::Template level;
        if (!level.Init(levelStream) || levelStream.Available() != 0) { ++failures; continue; }
        report << " level=" << tables.GetPackName(mission.level.packHash) << ':' << unsigned(mission.level.localIndex)
            << " map=" << tables.GetPackName(level.mapRef.packHash) << ':' << unsigned(level.mapRef.localIndex)
            << " states=" << level.script.GetStates().size() << " waves=" << level.wavesPerRevolution << ',' << level.waveLimit << '\n';
        if (!tables.ReadSectionResource(level.mapRef.packHash, ZGameSection::TileLayer, level.mapRef.localIndex, payload)) { ++failures; }
        else if (mission.type == 0) {
            CArrayInputStream mapStream(payload);
            CMap map;
            if (!map.Init(mapStream) || mapStream.Available() != 0) { ++failures; }
            else { failures += CheckMissionMap(level, map, report); }
        }
        for (const GameObjectRef &ref : mission.objectives) {
            ++objectives;
            if (!tables.ReadSectionResource(ref.packHash, ZGameSection::MissionObjective, ref.localIndex, payload)) { ++failures; continue; }
            CArrayInputStream objectiveStream(payload);
            MissionObjective objective;
            if (!objective.Init(objectiveStream) || objectiveStream.Available() != 0) { ++failures; continue; }
            report << " objective=" << tables.GetPackName(ref.packHash) << ':' << unsigned(ref.localIndex)
                << " type=" << objective.type << " fields=" << objective.value32 << ',' << objective.value36
                << " title=" << std::quoted(ReadGameString(toc, objective.title))
                << " description=" << std::quoted(ReadGameString(toc, objective.description)) << '\n';
        }
    }
    std::printf("[mission-check] missions=%zu objectives=%u failures=%u\n", catalog.size(), objectives, failures);
    return failures != 0;
}
