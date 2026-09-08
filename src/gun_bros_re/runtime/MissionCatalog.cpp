/** @file MissionCatalog.cpp
 * @brief Validate full records, level/map references and original objective text.
 */
#include "runtime/MissionCatalog.h"
#include "runtime/StoreCatalog.h"
#include "gun_bros/MissionObjective.h"
#include "gun_bros/CLevel.h"
#include "gun_bros/CMap.h"
#include "milestones/M3Map.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace {
// This records script requests without pretending to run enemy combat.
class MissionProbeWorld : public IEnemySpawnWorld {
public:
    bool SpawnEnemy(const GameObjectRef &, int, int, int) override { return true; }
    int CountEnemies(const GameObjectRef *, int) const override { return 0; }
    bool SpawnMapObject(const PlacedObject &, int objectId) override {
        if (std::find(spawned.begin(), spawned.end(), objectId) != spawned.end()) { ++duplicates; }
        spawned.push_back(objectId);
        return true;
    }
    std::vector<int> spawned;
    unsigned duplicates = 0;
};

unsigned CheckMissionMap(const CLevel::Template &data, CMap &map, std::ofstream &report) {
    CLevel level;
    MissionProbeWorld world;
    level.Bind(data, map, &world);
    unsigned failures = 0;
    std::int16_t args[3] = {1, 0, 0};
    for (int tag = 0; tag < 256; ++tag) {
        args[1] = static_cast<std::int16_t>(tag);
        level.FunctionResolver(15, args, 2);
    }
    const std::size_t initialCount = world.spawned.size();
    level.UpdateProximitySpawns(-100000, -100000, 200000, 200000);
    if (world.spawned.size() != initialCount) { ++failures; }
    args[0] = 0;
    for (int tag = 0; tag < 256; ++tag) {
        args[1] = static_cast<std::int16_t>(tag);
        level.FunctionResolver(15, args, 2);
    }
    level.UpdateProximitySpawns(-100000, -100000, 1, 1);
    const std::size_t remoteProps = world.spawned.size();
    level.UpdateProximitySpawns(-100000, -100000, 200000, 200000);
    const std::size_t allObjects = world.spawned.size();
    level.UpdateProximitySpawns(-100000, -100000, 200000, 200000);
    if (world.spawned.size() != allObjects || world.duplicates != 0) { ++failures; }
    // Native timing units and trigger switches are tested with an unused group.
    args[0] = 31;
    level.FunctionResolver(8, args, 1);
    if (level.OnTrigger(31)) { ++failures; }
    level.FunctionResolver(9, args, 1);
    args[1] = 256;
    level.FunctionResolver(34, args, 2);
    if (level.OnTrigger(31)) { ++failures; }
    level.Update(999);
    if (level.OnTrigger(31)) { ++failures; }
    level.Update(1);
    if (!level.OnTrigger(31)) { ++failures; }
    report << " script-contract initial=" << initialCount << " remote-props=" << remoteProps
        << " total=" << allObjects << " failures=" << failures << '\n';
    for (unsigned index = 0; index < map.GetObjectLayerCount(); ++index) {
        const CLayerObject &layer = map.GetObjectLayer(index);
        if (static_cast<int>(layer.GetLayerIndex()) != level.GetObjectLayer()) { continue; }
        unsigned objectId = 0;
        for (const PlacedObject &object : layer.GetObjects()) {
            report << " object=" << objectId++ << " type=" << unsigned(object.objectType)
                << " tag=" << unsigned(object.spawnTag) << " xy=" << object.x << ',' << object.y
                << " path=" << unsigned(object.pathLayer) << '\n';
        }
    }
    for (unsigned index = 0; index < map.GetCollisionLayerCount(); ++index) {
        const CLayerCollision &layer = map.GetCollisionLayer(index);
        if (static_cast<int>(layer.GetLayerIndex()) != level.GetTriggerLayer()) { continue; }
        for (const CollisionEdge &edge : layer.GetCollision().GetEdges()) {
            const auto &a = layer.GetCollision().GetVertices()[edge.firstVertex];
            const auto &b = layer.GetCollision().GetVertices()[edge.secondVertex];
            report << " trigger=" << unsigned(edge.group) << " from=" << a.x << ',' << a.y << " to=" << b.x << ',' << b.y << '\n';
        }
    }
    return failures;
}
}

bool LoadMissionCatalog(CResTOCManager &toc, PackTables &tables, std::vector<MissionEntry> &catalog) {
    catalog.clear();
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC &pack = *toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(GameSection::Mission);
        for (unsigned index = 0; index < count; ++index) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack.GetPackHash(), GameSection::Mission, index, payload)) { return false; }
            MissionEntry entry;
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

int RunMissionCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    std::vector<MissionEntry> catalog;
    if (!LoadMissionCatalog(toc, tables, catalog)) { return 1; }
    std::filesystem::create_directories("out");
    std::ofstream report("out/mission-check.txt");
    unsigned failures = 0, objectives = 0;
    for (const MissionEntry &entry : catalog) {
        const Mission &mission = entry.data;
        report << entry.owner << " title=" << std::quoted(entry.title) << " type=" << mission.type
            << " fields=" << mission.value64 << ',' << mission.value66 << " objectives=" << mission.objectives.size()
            << " description=" << std::quoted(ReadGameString(toc, mission.description)) << '\n';
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(mission.level.packHash, GameSection::Level, mission.level.localIndex, payload)) { ++failures; continue; }
        CArrayInputStream levelStream(payload);
        CLevel::Template level;
        if (!level.Init(levelStream) || levelStream.Available() != 0) { ++failures; continue; }
        report << " level=" << tables.GetPackName(mission.level.packHash) << ':' << unsigned(mission.level.localIndex)
            << " map=" << tables.GetPackName(level.mapRef.packHash) << ':' << unsigned(level.mapRef.localIndex)
            << " states=" << level.script.GetStates().size() << " waves=" << level.wavesPerRevolution << ',' << level.waveLimit << '\n';
        if (!tables.ReadSectionResource(level.mapRef.packHash, GameSection::TileLayer, level.mapRef.localIndex, payload)) { ++failures; }
        else if (mission.type == 0) {
            CArrayInputStream mapStream(payload);
            CMap map;
            if (!map.Init(mapStream) || mapStream.Available() != 0) { ++failures; }
            else { failures += CheckMissionMap(level, map, report); }
        }
        for (const GameObjectRef &ref : mission.objectives) {
            ++objectives;
            if (!tables.ReadSectionResource(ref.packHash, GameSection::MissionObjective, ref.localIndex, payload)) { ++failures; continue; }
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

int RunMissionPlay(const std::string &bigDirectory, const std::string &packName, int missionIndex,
    unsigned weaponIndex, int armorIndex, const std::string &screenshot, unsigned advanceMs, bool fire, bool check) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    std::vector<MissionEntry> catalog;
    if (!LoadMissionCatalog(toc, tables, catalog)) { return 1; }
    std::string selectedPack = packName;
    if (missionIndex < 0) {
        std::printf("\nCAMPAIGN ARCHIVE - unfinished original missions; isolated progress\n");
        for (const MissionEntry &entry : catalog) {
            if (entry.data.type == 0) { std::printf("  %s  %s\n", entry.owner.c_str(), entry.title.c_str()); }
        }
        std::printf("pack and mission [pack2 11]: ");
        std::fflush(stdout);
        char line[64] = {};
        selectedPack = "pack2";
        missionIndex = 11;
        if (std::fgets(line, sizeof(line), stdin) != nullptr && line[0] != '\n') {
            std::istringstream input(line);
            if (!(input >> selectedPack >> missionIndex)) { return 1; }
        }
    }
    const int packIndex = toc.GetPackIndexFromName(selectedPack.c_str());
    if (packIndex < 0 || missionIndex < 0) { return 1; }
    for (const MissionEntry &entry : catalog) {
        if (entry.resource.packHash != toc.GetPack(packIndex)->GetPackHash() || entry.resource.localIndex != missionIndex) { continue; }
        if (entry.data.type != 0) { std::printf("[campaign] entry is not an archived campaign mission\n"); return 1; }
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(entry.data.level.packHash, GameSection::Level, entry.data.level.localIndex, payload)) { return 1; }
        CArrayInputStream stream(payload);
        CLevel::Template level;
        if (!level.Init(stream)) { return 1; }
        std::printf("[campaign] %s %s\n", entry.owner.c_str(), entry.title.c_str());
        return RunSurvival(bigDirectory, tables.GetPackName(level.mapRef.packHash), level.mapRef.localIndex,
            weaponIndex, armorIndex, screenshot, advanceMs, fire, false, check, 2, 0, nullptr, false, false, &entry);
    }
    return 1;
}
