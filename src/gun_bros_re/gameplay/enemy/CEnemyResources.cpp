/** Original: src/gunbros/enemy.cpp Template::Load :68895, Bind :73381; moveSetMesh.cpp Load :123213.
 * Windows graphics/resource storage is adapted; original data comes from BIG.
 */
/**
 * @file CEnemyResources.cpp
 * @brief One enemy's models, assembled by its script and ready to draw.
 * Historical ownership note from the removed model wrapper:
 * Harness scaffolding shared by the two places an enemy appears: the M3.8
 * viewer, which shows one on a turntable, and the M3 map viewer, which stands
 * them on the terrain their spawn points name. Both need the same four steps
 * -- read the template, load every mesh config, let the script build the part
 * table, draw the parts against a base matrix -- and only the base matrix
 * differs, so only the base matrix is left to the caller.
 * The scale a model is drawn at is NOT part of the base matrix by accident:
 * see EnemyModelWorldScale, which is the one number this file exists to get
 * right.
 * That calculation now lives in CEnemy::GetWorldScale; no wrapper remains.
 */

#include "gun_bros_re/gameplay/enemy/CEnemy.h"

#include "engine/resources/CArrayInputStream.h"
#include "engine/graphics/ZPNG.h"
#include "gun_bros_re/data/CGameObjectPack.h"

#include <cstdio>

static bool LoadEnemyConfigs(ZPackTables &tables, const CEnemy::Template &entry,
    bool createBuffers, const ZShaderProgram *program,
    std::vector<std::shared_ptr<CEnemy::ModelConfig>> &configs, CEnemy::ResourceCache *cache) {
    configs.clear();
    if (createBuffers && program == nullptr) { return false; }
    // Cache only uploaded resources; a CPU-only preview must not poison it.
    if (!createBuffers) { cache = nullptr; }
    const std::uint64_t key = (static_cast<std::uint64_t>(entry.packHash) << 32) | entry.ordinal;
    bool cached = false;
    if (cache != nullptr) {
        const auto found = cache->entries.find(key);
        if (found != cache->entries.end()) { configs = found->second; ++cache->hits; cached = true; }
        else { ++cache->misses; }
    }

    for (std::size_t i = 0; !cached && i < entry.moveSet.GetMeshConfigs().size(); ++i) {
        const ZMeshConfig &config = entry.moveSet.GetMeshConfigs()[i];
        auto loaded = std::make_shared<CEnemy::ModelConfig>();

        std::vector<std::uint8_t> meshPayload;
        if (!tables.ReadSectionResource(entry.moveSet.GetPackHash(), ZGameSection::Mesh,
                                        config.meshOrdinal, meshPayload)) {
            std::printf("[enemy] %s: mesh %u unreadable\n", entry.owner.c_str(),
                        config.meshOrdinal);
            return false;
        }

        CArrayInputStream meshStream(meshPayload);
        // CEnemy::Template::Load :68895 -> CMoveSetMesh::LoadMesh :123178
        // retains only move-used frames. The first retained pose supplies the
        // bounds used by both DrawUI and gameplay size normalization.
        if (!loaded->mesh.Init(meshStream, &entry.moveSet)) {
            return false;
        }

        if (createBuffers) {
            std::vector<std::uint8_t> imagePayload;
            ZPNGImage decoded;
            if (!tables.ReadSectionResource(entry.moveSet.GetPackHash(), ZGameSection::Png,
                                            config.imageOrdinal, imagePayload) ||
                !PNGDecode(imagePayload, decoded) ||
                !loaded->texture.Create(decoded, GL_REPEAT)) {
                std::printf("[enemy] %s: atlas %u unreadable\n",
                            entry.owner.c_str(), config.imageOrdinal);
                return false;
            }

            if (!loaded->buffer.Create(*program) ||
                !loaded->buffer.SetMesh(loaded->mesh)) {
                std::printf("[enemy] %s: config %zu has no GL buffer\n",
                            entry.owner.c_str(), i);
                return false;
            }
        }

        loaded->valid = true;
        configs.push_back(std::move(loaded));
    }

    if (configs.empty()) {
        return false;
    }
    if (cache != nullptr && !cached) { cache->entries[key] = configs; }

    return true;
}

bool CEnemy::Preload(ZPackTables &tables, const CEnemy::Template &entry,
    const ZShaderProgram &program, CEnemy::ResourceCache &cache) {
    std::vector<std::shared_ptr<CEnemy::ModelConfig>> configs;
    if (!LoadEnemyConfigs(tables, entry, true, &program, configs, &cache)) { return false; }
    for (const auto &config : configs) { if (!config->valid) { return false; } }
    return true;
}

bool CEnemy::Bind(ZPackTables &tables, const Template &entry,
    bool createBuffers, const ZShaderProgram *program, ResourceCache *cache) {
    data = &entry;
    if (!LoadEnemyConfigs(tables, entry, createBuffers, program, configs, cache)) { return false; }

    m_configMeshes.assign(configs.size(), nullptr);
    for (std::size_t i = 0; i < configs.size(); ++i) {
        if (configs[i]->valid) {
            m_configMeshes[i] = &configs[i]->mesh;
        }
    }

    Bind(entry.script, entry.moveSet, m_configMeshes);
    ConfigureTemplate(static_cast<float>(entry.radius116), entry.flag117 != 0,
        entry.objectRef104, entry.collision);
    // Historical fallback notes retained below; these behaviours were not
    // present in SpawnForUI :72856 and must not execute in the runtime.
    // A template whose export 3 does nothing leaves part 0 without a move.
    // Enter the first state that gives it something to play -- a state is what
    // the game would put it in, so this is closer than picking a move out of
    // the list would be.
    // Still nothing: no state animates part 0, so show its first move rather
    // than an empty buffer.
    return true;
}
