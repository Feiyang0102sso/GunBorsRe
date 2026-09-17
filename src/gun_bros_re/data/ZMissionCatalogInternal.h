#pragma once
/** @file ZMissionCatalogInternal.h
 * @brief Validate full records, level/map references and original objective text.
 */
#include "gun_bros_re/data/ZMissionCatalog.h"
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "gun_bros_re/data/MissionObjective.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/CMap.h"
#include "gun_bros_re/gameplay/ZMapScene.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace MissionCatalogDetail {

// This records script requests without pretending to run enemy combat.
class ZMissionProbeWorld : public ZLevelWorld {
public:
    bool SpawnEnemy(const GameObjectRef &, int, int, int) override { return true; }
    int CountEnemies(const GameObjectRef *, int) const override { return 0; }
    bool SpawnMapObject(const ZPlacedObject &, int objectId) override {
        if (std::find(spawned.begin(), spawned.end(), objectId) != spawned.end()) { ++duplicates; }
        spawned.push_back(objectId);
        return true;
    }
    std::vector<int> spawned;
    unsigned duplicates = 0;
};

unsigned CheckMissionMap(const CLevel::Template &data, CMap &map, std::ofstream &report);

}
