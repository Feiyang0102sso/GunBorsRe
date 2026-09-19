#include "tests/research/SurvivalStudyHost.h"
/** @file MissionStudyHost.cpp
 * @brief Validate full records, level/map references and original objective text.
 */
#include "gun_bros_re/data/mission/Mission.h"
#include "gun_bros_re/data/store/CStoreItem.h"
#include "gun_bros_re/data/mission/MissionObjective.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/map/CMap.h"
#include "gun_bros_re/gameplay/game/CGameSession.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include "tests/research/MissionStudyInternal.h"
using namespace MissionCatalogDetail;
int RunMissionPlay(const std::string &bigDirectory, const std::string &packName, int missionIndex,
    unsigned weaponIndex, int armorIndex, const std::string &screenshot, unsigned advanceMs, bool fire, bool check) {
    CResTOCManager toc;
    if (!toc.InitAuto(bigDirectory) || !toc.Bind()) { return 1; }
    CGunBros tables(toc);
    std::vector<Mission::Entry> catalog;
    if (!Mission::LoadEntries(toc, tables, catalog)) { return 1; }
    std::string selectedPack = packName;
    if (missionIndex < 0) {
        std::printf("\nCAMPAIGN ARCHIVE - unfinished original missions; isolated progress\n");
        for (const Mission::Entry &entry : catalog) {
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
    for (const Mission::Entry &entry : catalog) {
        if (entry.resource.packHash != toc.GetPack(packIndex)->GetPackHash() || entry.resource.localIndex != missionIndex) { continue; }
        if (entry.data.type != 0 && entry.data.type != 2) { std::printf("[mission] unsupported mission type\n"); return 1; }
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(entry.data.level.packHash, ZGameSection::Level, entry.data.level.localIndex, payload)) { return 1; }
        CArrayInputStream stream(payload);
        CLevel::Template level;
        if (!level.Init(stream)) { return 1; }
        std::printf("[campaign] %s %s\n", entry.owner.c_str(), entry.title.c_str());
        return RunViewerSurvival(bigDirectory, tables.GetPackName(level.mapRef.packHash), level.mapRef.localIndex,
            weaponIndex, armorIndex, screenshot, advanceMs, fire, false, check, 2, entry.data.value64, nullptr, false, false, &entry);
    }
    return 1;
}
