/** @file PropCatalog.cpp
 * @brief Parse full original prop records and exercise reachable host callbacks.
 */
#include "runtime/PropCatalog.h"
#include "runtime/StoreCatalog.h"
#include "gun_bros/CProp.h"
#include "gun_bros/CMap.h"
#include <cstdio>
#include <fstream>
#include <filesystem>

int RunPropCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    std::filesystem::create_directories("out");
    std::ofstream report("out/prop-check.txt");
    unsigned failures = 0, templates = 0, scripts = 0, moves = 0, actions = 0;
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC &pack = *toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(GameSection::Prop);
        for (unsigned index = 0; index < count; ++index) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack.GetPackHash(), GameSection::Prop, index, payload)) { ++failures; continue; }
            CArrayInputStream stream(payload);
            CProp::Template data;
            if (!data.Init(stream) || stream.Available() != 0) {
                ++failures;
                std::printf("[prop-check] invalid %s:%u remaining=%zu\n", pack.GetShortName().c_str(), index, stream.Available());
                if (index < 2 || index == 43) {
                    std::printf("[prop-check] tail=");
                    std::size_t begin = 0;
                    if (payload.size() > 36) { begin = payload.size() - 36; }
                    for (std::size_t byte = begin; byte < payload.size(); ++byte) { std::printf("%02x ", payload[byte]); }
                    std::printf("\n");
                }
                continue;
            }
            ++templates;
            if (data.GetScript().IsPresent()) { ++scripts; }
            moves += static_cast<unsigned>(data.GetMoveSet().moves.size());
            report << pack.GetShortName() << ':' << index << " script=" << data.GetScript().IsPresent()
                << " moves=" << data.GetMoveSet().moves.size() << " states=" << data.GetScript().GetStates().size()
                << " removeWhenDead=" << data.RemoveWhenDead();
            CProp prop;
            prop.Bind(data);
            report << " start=" << prop.GetStateId() << " hp=" << prop.GetHealth();
            for (int message = 0; message < 3; ++message) { prop.HandleMessage(message); prop.Update(100, true); }
            prop.Damage(10000, 0xffffffffu);
            for (int elapsed = 0; elapsed < 5000; elapsed += 100) { prop.Update(100, false); }
            report << " end=" << prop.GetStateId() << " hp=" << prop.GetHealth();
            const auto cues = prop.TakeActions();
            actions += static_cast<unsigned>(cues.size());
            for (const PropAction &cue : cues) {
                report << " action=" << static_cast<int>(cue.kind);
                if (cue.resource.IsNull()) { continue; }
                GameSection section = GameSection::ParticleEffect;
                if (cue.kind == PropAction::Kind::Sound) { section = GameSection::SoundEffect; }
                if (!tables.ReadSectionResource(cue.resource.packHash, section, cue.resource.localIndex, payload)) { ++failures; }
            }
            failures += prop.GetUnsupportedCount();
            report << " unsupported=" << prop.GetUnsupportedCount() << '\n';
        }
    }
    std::printf("[prop-check] templates=%u scripts=%u moves=%u actions=%u failures=%u\n",
        templates, scripts, moves, actions, failures);
    // Inspect every object layer, including inactive campaign/deathmatch data.
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC &pack = *toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(GameSection::TileLayer);
        for (unsigned index = 0; index < count; ++index) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack.GetPackHash(), GameSection::TileLayer, index, payload)) { continue; }
            CArrayInputStream stream(payload);
            CMap map;
            if (!map.Init(stream)) { continue; }
            for (unsigned layerIndex = 0; layerIndex < map.GetObjectLayerCount(); ++layerIndex) {
                const auto &layer = map.GetObjectLayer(layerIndex);
                for (const auto &object : layer.GetObjects()) {
                    if (object.objectType == 19 && object.packHash == 0x267589 && object.localIndex == 43) {
                        std::printf("[prop-check] malformed-reference %s MAP %u layer=%u tag=%u\n",
                            pack.GetShortName().c_str(), index, layer.GetLayerIndex(), object.spawnTag);
                    }
                }
            }
        }
    }
    return failures != 0;
}
