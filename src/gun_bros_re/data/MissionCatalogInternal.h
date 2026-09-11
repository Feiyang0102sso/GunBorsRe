#pragma once
/** @file MissionCatalog.cpp
 * @brief Validate full records, level/map references and original objective text.
 */
#include "gun_bros_re/data/MissionCatalog.h"
#include "gun_bros_re/data/StoreCatalog.h"
#include "gun_bros_re/data/MissionObjective.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "gun_bros_re/gameplay/CMap.h"
#include "gun_bros_re/gameplay/MapScene.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace MissionCatalogDetail {

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

#if GB_ENABLE_TESTS

unsigned CheckMissionMap(const CLevel::Template &data, CMap &map, std::ofstream &report);
#endif

}
