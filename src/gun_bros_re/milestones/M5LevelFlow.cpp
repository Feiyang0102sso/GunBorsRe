/** @file M5LevelFlow.cpp
 * @brief Simulate spawn/death callbacks; this verifies flow, not gameplay AI.
 */
#include "milestones/M5LevelFlow.h"
#include "gun_bros/CLevel.h"
#include "gun_bros/CMap.h"
#include "gun_bros/CEnemy.h"
#include "runtime/PackTables.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cmath>

namespace {
unsigned CheckCameraScale() {
    CCamera camera;
    unsigned failures = 0;
    camera.SnapScale(0.8f);
    camera.SetScale(0.4f);
    if (std::fabs(camera.GetScale() - 0.8f) > 0.00001f) { ++failures; }
    camera.Update(250);
    if (std::fabs(camera.GetScale() - 0.74142136f) > 0.00001f) { ++failures; }
    camera.Update(250);
    if (std::fabs(camera.GetScale() - 0.6f) > 0.00001f) { ++failures; }
    camera.SetScale(1);
    if (std::fabs(camera.GetScale() - 0.6f) > 0.00001f) { ++failures; }
    camera.Update(500);
    if (std::fabs(camera.GetScale() - 0.8f) > 0.00001f) { ++failures; }
    camera.Update(5000);
    if (camera.GetScale() != 1) { ++failures; }
    CMap map;
    CLevel level;
    CLevel::Template data;
    level.Bind(data, map);
    const std::int16_t argument = 128;
    level.FunctionResolver(14, &argument, 1);
    map.GetCamera().Update(1000);
    if (map.GetCamera().GetScale() != 0.5f) { ++failures; }
    level.Bind(data, map);
    if (map.GetCamera().GetScale() != 0.8f) { ++failures; }
    std::printf("[camera-check] interpolation/retarget/snap/native/restart failures=%u\n", failures);
    return failures;
}

/** A script test world: validate enemy resources, then finish them after 1s. */
class LevelFlowWorld : public IEnemySpawnWorld {
public:
    LevelFlowWorld(CLevel &level, PackTables &tables, CMap &map) : m_level(level), m_tables(tables), m_map(map) {}
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
        if (!m_tables.ReadSectionResource(enemy.packHash, GameSection::Enemy, enemy.localIndex, payload)) {
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
        if (!m_tables.ReadSectionResource(sound.packHash, GameSection::SoundEffect, sound.localIndex, payload)) {
            ++failures;
            return;
        }
        CArrayInputStream stream(payload);
        CGameAssetRef wav;
        wav.Init(stream);
        if (stream.Overran() || wav.assetId < 0 ||
            !m_tables.ReadSectionResource(wav.packHash, GameSection::Wav, wav.assetId, payload)) { ++failures; }
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
    PackTables &m_tables;
    CMap &m_map;
    std::vector<Entry> m_enemies;
};
}

int RunLevelFlowCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) {
        return 1;
    }
    PackTables tables(toc);
    std::filesystem::create_directories("out");
    std::ofstream report("out/level-flow-check.csv");
    if (!report) {
        return 1;
    }
    report << "pack,level,states,waves_per_revolution,wave_limit,final_state,wave,spawned,unsupported_level,unsupported_spawner,failures\n";
    unsigned failures = CheckCameraScale();
    unsigned levels = 0;
    for (std::uint32_t packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC *pack = toc.GetPack(packIndex);
        const std::uint32_t count = tables.GetObjectPack(packIndex).GetObjectCount(GameSection::Level);
        for (std::uint32_t index = 0; index < count; ++index) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack->GetPackHash(), GameSection::Level, index, payload)) {
                return 1;
            }
            CArrayInputStream stream(payload);
            CLevel::Template data;
            if (!data.Init(stream) || stream.Available() != 0) {
                return 1;
            }
            if (!data.script.IsPresent()) {
                continue;
            }
            if (!tables.ReadSectionResource(data.mapRef.packHash, GameSection::TileLayer, data.mapRef.localIndex, payload)) {
                return 1;
            }
            CArrayInputStream mapStream(payload);
            CMap map;
            if (!map.Init(mapStream)) {
                return 1;
            }
            CLevel level;
            LevelFlowWorld world(level, tables, map);
            if (data.script.IsPresent()) {
                std::ofstream bytecode("out/level-flow-" + pack->GetShortName() + "-" + std::to_string(index) + ".txt");
                bytecode << "exports ";
                for (unsigned function : data.script.GetExportFunctions()) { bytecode << function << ' '; }
                bytecode << '\n';
                for (unsigned resourceIndex = 0; resourceIndex < data.script.GetResources().size(); ++resourceIndex) {
                    const auto &resource = data.script.GetResources()[resourceIndex];
                    bytecode << "resource " << resourceIndex << " type " << unsigned(resource.sectionOrType)
                        << " pack " << resource.packHash << " item " << resource.resourceId << '\n';
                }
                for (std::size_t functionIndex = 0; functionIndex < data.script.GetFunctions().size(); ++functionIndex) {
                    bytecode << "function " << functionIndex << " ";
                    const CScriptCode &code = data.script.GetFunctions()[functionIndex];
                    if (code.Begin() != nullptr) {
                        for (unsigned byte = 0; byte <= code.GetByteLength(); ++byte) {
                            char hex[4];
                            std::snprintf(hex, sizeof(hex), "%02X ", code.Begin()[byte]);
                            bytecode << hex;
                        }
                    }
                    bytecode << '\n';
                }
                for (std::size_t stateIndex = 0; stateIndex < data.script.GetStates().size(); ++stateIndex) {
                    const CScriptState &state = data.script.GetStates()[stateIndex];
                    for (const ScriptStateExport &handler : state.GetExports()) {
                        bytecode << "state " << stateIndex << " export " << unsigned(handler.id) << " ";
                        const CScriptCode &handlerCode = handler.code;
                        for (unsigned byte = 0; byte <= handlerCode.GetByteLength(); ++byte) {
                            char hex[4];
                            std::snprintf(hex, sizeof(hex), "%02X ", handlerCode.Begin()[byte]);
                            bytecode << hex;
                        }
                        bytecode << '\n';
                    }
                    bytecode << "state " << stateIndex << " parent " << unsigned(state.GetParent()) << " enter ";
                    const CScriptCode &code = state.GetEnterCode();
                    if (code.Begin() != nullptr) {
                        for (unsigned byte = 0; byte <= code.GetByteLength(); ++byte) {
                            char hex[4];
                            std::snprintf(hex, sizeof(hex), "%02X ", code.Begin()[byte]);
                            bytecode << hex;
                        }
                    }
                    bytecode << '\n';
                }
            }
            std::printf("[level-flow] begin %s level %u\n", pack->GetShortName().c_str(), index);
            level.Bind(data, map, &world);
            // CGame::OnPlay starts the HUD intro; its completion emits event 2.
            level.HandleEvent(2);
            for (int elapsed = 0; elapsed < 300000; elapsed += 16) {
                const int previousWave = level.GetWave();
                map.GetCamera().Update(16);
                level.Update(16);
                world.Update(16);
                if (level.GetWave() != previousWave) {
                    level.HandleEvent(2);
                }
            }
            unsigned entryFailures = world.failures;
            // Large retail wave scripts must actually schedule and advance.
            if (data.script.GetStates().size() > 100 && (world.spawned == 0 || level.GetWave() < 2)) {
                ++entryFailures;
            }
            report << pack->GetShortName() << ',' << index << ',' << data.script.GetStates().size() << ','
                << data.wavesPerRevolution << ',' << data.waveLimit << ',' << level.GetStateId() << ',' << level.GetWave() << ','
                << world.spawned << ',' << level.GetUnimplementedCallCount() << ','
                << level.GetSpawner().GetUnsupportedCount() << ',' << entryFailures << '\n';
            std::printf("[level-flow] %s level %u state=%d wave=%d spawned=%u sounds=%u failures=%u\n",
                pack->GetShortName().c_str(), index, level.GetStateId(), level.GetWave(), world.spawned, world.sounds, entryFailures);
            failures += entryFailures;
            ++levels;
        }
    }
    std::printf("[level-flow] levels=%u failures=%u\n", levels, failures);
    if (failures != 0 || !report) {
        return 1;
    }
    return 0;
}
