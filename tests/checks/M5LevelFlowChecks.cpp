/** @file M5LevelFlow.cpp
 * @brief Simulate spawn/death callbacks; this verifies flow, not gameplay AI.
 */
#include "TestOutput.h"
#include "tests/checks/M5LevelFlow.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "gun_bros_re/gameplay/CMap.h"
#include "gun_bros_re/gameplay/CEnemy.h"
#include "gun_bros_re/data/PackTables.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cmath>
#include "tests/checks/M5LevelFlowInternal.h"
using namespace M5LevelFlowDetail;
#include "Checks.h"

int RunLevelFlowCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) {
        return 1;
    }
    PackTables tables(toc);
    std::filesystem::create_directories(TestOutput::Path(""));
    std::ofstream report(TestOutput::Path("level-flow-check.csv"));
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
                std::ofstream bytecode(TestOutput::Path("level-flow-") + pack->GetShortName() + "-" + std::to_string(index) + ".txt");
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