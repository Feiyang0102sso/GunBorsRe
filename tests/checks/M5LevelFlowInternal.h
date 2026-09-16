#pragma once
/** @file M5LevelFlow.cpp
 * @brief Simulate spawn/death callbacks; this verifies flow, not gameplay AI.
 */
#include "tests/checks/M5LevelFlow.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "gun_bros_re/gameplay/CMap.h"
#include "gun_bros_re/gameplay/CEnemy.h"
#include "gun_bros_re/data/ZPackTables.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cmath>

namespace M5LevelFlowDetail {

class IndicatorWorld : public ZLevelWorld {
public:
    bool SpawnEnemy(const GameObjectRef &, int, int, int) override { return false; }
    int CountEnemies(const GameObjectRef *, int) const override { return 0; }
    bool GetObjectPosition(int objectId, float &x, float &y) const override {
        if (objectId != 7) { return false; }
        x = 2000; y = 500; return true;
    }
};

unsigned CheckCameraScale();

/** A script test world: validate enemy resources, then finish them after 1s. */
class LevelFlowWorld : public ZLevelWorld {
public:
    LevelFlowWorld(CLevel &level, ZPackTables &tables, CMap &map) : m_level(level), m_tables(tables), m_map(map) {}
    bool SpawnEnemy(const GameObjectRef &enemy, int layer, int node, int objectId) override {
        if (layer >= 0) {
            CLayerPathLink *path = m_map.GetPathLinkLayer(layer);
            if (path == nullptr || path->GetNodes().empty() || node >= static_cast<int>(path->GetNodes().size())) {
                std::printf("[level-flow] invalid spawn layer=%d node=%d\n", layer, node);
                ++failures;
                return false;
            }
        }
        std::vector<std::uint8_t> payload;
        if (!m_tables.ReadSectionResource(enemy.packHash, ZGameSection::Enemy, enemy.localIndex, payload)) {
            ++failures;
            return false;
        }
        Entry entry;
        entry.enemy = enemy;
        entry.objectId = objectId;
        m_enemies.push_back(entry);
        ++spawned;
        return true;
    }
    int CountEnemies(const GameObjectRef *enemy, int objectId = -1) const override {
        int count = 0;
        for (const Entry &entry : m_enemies) {
            if (objectId >= 0 && entry.objectId != objectId) { continue; }
            if (enemy == nullptr || (entry.enemy.packHash == enemy->packHash && entry.enemy.localIndex == enemy->localIndex)) {
                ++count;
            }
        }
        return count;
    }
    void PlayLevelSound(const GameObjectRef &sound) override {
        std::vector<std::uint8_t> payload;
        if (!m_tables.ReadSectionResource(sound.packHash, ZGameSection::SoundEffect, sound.localIndex, payload)) {
            ++failures;
            return;
        }
        CArrayInputStream stream(payload);
        CGameAssetRef wav;
        wav.Init(stream);
        if (stream.Overran() || wav.assetId < 0 ||
            !m_tables.ReadSectionResource(wav.packHash, ZGameSection::Wav, wav.assetId, payload)) { ++failures; }
        ++sounds;
    }
    void Update(int deltaMs) {
        std::vector<Entry> killed;
        for (std::size_t index = 0; index < m_enemies.size();) {
            m_enemies[index].ageMs += deltaMs;
            if (m_enemies[index].ageMs >= 1000) {
                killed.push_back(m_enemies[index]);
                m_enemies.erase(m_enemies.begin() + index);
            } else {
                ++index;
            }
        }
        for (const Entry &entry : killed) {
            m_level.OnEnemyKilled(entry.objectId, entry.enemy);
        }
    }
    unsigned failures = 0;
    unsigned spawned = 0;
    unsigned sounds = 0;
private:
    struct Entry {
        GameObjectRef enemy;
        int objectId = -1;
        int ageMs = 0;
    };
    CLevel &m_level;
    ZPackTables &m_tables;
    CMap &m_map;
    std::vector<Entry> m_enemies;
};
}
