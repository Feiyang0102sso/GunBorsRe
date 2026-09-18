/** @file CEnemyMap.cpp
 * @brief Map viewer placement using the same CEnemy template and instance.
 * Original: src/gunbros/level.cpp StartLayer/SpawnObject, enemy.cpp Spawn :73284.
 * Windows adapter: permanent static map previews, not the live level object pool.
 */
#include "gun_bros_re/gameplay/ZMapWorldInternal.h"
#include <cstdio>
namespace MapDetail {
/** One enemy template standing on the map, with the model it draws as. */

void LoadPlacedEnemies(CResTOCManager &tocManager, const ZShaderProgram &program,
                       ZLoadedMap &loaded) {
    ZPackTables tables(tocManager);
    unsigned failed = 0;

    for (std::uint32_t layer = 0; layer < loaded.map.GetObjectLayerCount();
         ++layer) {
        const std::vector<ZPlacedObject> &objects =
            loaded.map.GetObjectLayer(layer).GetObjects();
        for (std::size_t i = 0; i < objects.size(); ++i) {
            if (objects[i].objectType !=
                static_cast<std::uint8_t>(ZPlacedObjectType::Enemy)) {
                continue;
            }

            char label[128];
            std::snprintf(label, sizeof(label), "%s enemy %u",
                          tables.GetPackName(objects[i].packHash).c_str(),
                          objects[i].localIndex);

            auto entry = std::make_unique<CEnemy::Template>();
            auto placed = std::make_unique<CEnemy>();
            if (!entry->Load(tables, objects[i].packHash, objects[i].localIndex, label)) {
                failed++;
                continue;
            }

            placed->combat.x = static_cast<float>(objects[i].x);
            placed->combat.y = static_cast<float>(objects[i].y);
            // A map is a level, so the level spawn export is the one to run.
            if (!placed->Bind(tables, *entry, true, &program)) {
                failed++;
                continue;
            }

            placed->Spawn();

            // One line each: a map places a couple of dozen at most, and which
            // template a leftover spawn names is exactly what is worth seeing.
            std::printf("[m3] %s at %d %d -- %zu configs, %u parts\n",
                        label, objects[i].x, objects[i].y,
                        placed->configs.size(),
                        placed->GetPartCount());

            loaded.enemyTemplates.push_back(std::move(entry));
            loaded.enemies.push_back(std::move(placed));
        }
    }

    if (!loaded.enemies.empty() || failed > 0) {
        std::printf("[m3] %zu placed enemies drawn, %u unloadable\n",
                    loaded.enemies.size(), failed);
    }
}

/** Move every placed enemy's animation on. */
void AdvanceEnemies(ZLoadedMap &loaded, std::int32_t deltaMs) {
    for (std::size_t i = 0; i < loaded.enemies.size(); ++i) {
        loaded.enemies[i]->Update(deltaMs);
    }
}


}
