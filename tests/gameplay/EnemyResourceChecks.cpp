/** Enemy resources may be shared; script/animation state must stay per actor. */
#include "gun_bros_re/gameplay/enemy/CEnemy.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include <cmath>
#include <cstdio>

namespace {
unsigned CheckEnemyNodeAndSpawn(CResTOCManager &toc, CGunBros &tables,
    CLevel &scene, const CEnemy::Template &entry) {
    CLevel::Template levelTemplate;
    GameObjectRef expectedResource;
    int resourceIndex = -1;
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount() && resourceIndex < 0; ++packIndex) {
        const auto *pack = toc.GetPack(packIndex);
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Level);
        for (unsigned index = 0; index < count && resourceIndex < 0; ++index) {
            std::vector<std::uint8_t> payload;
            if (!tables.ReadSectionResource(pack->GetPackHash(), ZGameSection::Level, index, payload)) { return 1; }
            CArrayInputStream input(payload);
            if (!levelTemplate.Init(input)) { return 1; }
            const auto &resources = levelTemplate.script.GetResources();
            for (unsigned resource = 0; resource < resources.size(); ++resource) {
                if (resources[resource].sectionOrType != static_cast<unsigned>(ZGameSection::Enemy) - 1) { continue; }
                expectedResource.packHash = resources[resource].packHash;
                expectedResource.localIndex = static_cast<std::uint8_t>(resources[resource].resourceId);
                resourceIndex = static_cast<int>(resource);
                break;
            }
        }
    }
    if (resourceIndex < 0) { return 1; }
    CMap map;
    CLevel level;
    level.Bind(levelTemplate, map);
    scene.Reset();
    CEnemy *enemy = scene.Spawn(0, 600.75f, 450.75f);
    if (enemy == nullptr) { return 1; }
    enemy->SetLevelContext(&level);
    enemy->combat.facing = 0;
    enemy->combat.scaleFactor = 3; // Hurtbox scaling must not change original node coordinates.
    enemy->TakeActions();
    const auto &bounds = enemy->GetPart(0).controller.GetAnimation().GetMesh()->GetBounds();
    float x = 0, y = 0, z = 0;
    unsigned failures = 0;
    const float expectedX = enemy->combat.x - bounds.centerX * entry.gameScale * bounds.inverseExtent;
    const float expectedY = enemy->combat.y - bounds.centerY * entry.gameScale * bounds.inverseExtent;
    if (!enemy->GetNodeLocationChunk(0, -1, x, y, z) ||
        std::abs(x - expectedX) > 0.0001f || std::abs(y - expectedY) > 0.0001f) { ++failures; }
    const std::int16_t stored[] = {128, -256, 512, 7};
    enemy->FunctionResolver(59, stored, 4);
    if (enemy->combat.native59Parameters != std::array<float, 4>{0.5f, -1, 2, 7}) { ++failures; }
    const std::int16_t spawn[] = {static_cast<std::int16_t>(resourceIndex), -1, 123};
    enemy->FunctionResolver(54, spawn, 2);
    auto actions = enemy->TakeActions();
    if (actions.size() != 1 || actions[0].slot != -1 ||
        actions[0].resource.packHash != expectedResource.packHash ||
        actions[0].resource.localIndex != expectedResource.localIndex ||
        actions[0].x != std::trunc(x) || actions[0].y != std::trunc(y)) { ++failures; }
    enemy->FunctionResolver(54, spawn, 3);
    const unsigned before = scene.GetSpawnCount();
    scene.Update(16, 0, 0, false);
    const CEnemy *child = nullptr;
    for (const auto &actor : scene.GetEnemies()) {
        if (actor->objectId == 123) { child = actor.get(); break; }
    }
    if (scene.GetSpawnCount() != before + 1 || child == nullptr ||
        child->combat.summoner != 0 || child->combat.x != std::trunc(x) || child->combat.y != std::trunc(y)) { ++failures; }
    scene.Reset();
    std::printf("[enemy-native-check] level-resource=%08x:%u node-origin=part spawn-id=123 native59=stored failures=%u\n",
        expectedResource.packHash, expectedResource.localIndex, failures);
    return failures;
}
}

unsigned CheckEnemyResources(CResTOCManager &toc, CGunBros &tables, CLevel &scene,
    const CEnemy::Template &entry, const ZShaderProgram &program) {
    unsigned failures = 0;
    CEnemy::ResourceCache cache;
    CEnemy survey;
    if (!survey.Bind(tables, entry, false, nullptr, &cache)) { return 1; }
    if (!cache.entries.empty() || survey.GetPartConfig(0) != -1) { ++failures; }
    CEnemy missingProgram;
    if (missingProgram.Bind(tables, entry, true, nullptr, &cache) || !cache.entries.empty()) { ++failures; }
    if (!CEnemy::Preload(tables, entry, program, cache)) { return 1; }
    CEnemy first, second;
    if (!first.Bind(tables, entry, true, &program, &cache) ||
        !second.Bind(tables, entry, true, &program, &cache)) { return 1; }
    if (cache.hits != 2 || cache.misses != 1 || first.configs.empty() ||
        first.configs.front() != second.configs.front()) { ++failures; }
    first.Spawn();
    if (first.GetPartConfig(0) < 0 || second.GetPartConfig(0) != -1) { ++failures; }
    second.Spawn();
    const float secondHealth = second.combat.health;
    const auto secondState = second.GetStateId();
    first.Damage(first.combat.health);
    if (!first.combat.dead || second.combat.dead || second.combat.health != secondHealth ||
        second.GetStateId() != secondState) { ++failures; }
    std::vector<std::uint8_t> payload;
    if (!tables.ReadSectionResource(entry.packHash, ZGameSection::Enemy, entry.ordinal, payload)) { return 1; }
    CEnemy::Template invalid;
    payload.push_back(0);
    CArrayInputStream extra(payload);
    if (invalid.Init(extra)) { ++failures; }
    payload.resize(payload.size() - 2);
    CArrayInputStream truncated(payload);
    if (invalid.Init(truncated)) { ++failures; }
    std::printf("[enemy-resource-check] cpu-cache=isolated gpu-cache=shared actors=independent template-boundary=strict failures=%u\n", failures);
    return failures + CheckEnemyNodeAndSpawn(toc, tables, scene, entry);
}
