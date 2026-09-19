/** Original: src/gunbros/levelObjectPool.cpp constructor :145287, GetEnemy :145509, Release :145426.
 * Windows graphics/resource storage is adapted; original data comes from BIG.
 */
/** @file CLevelObjectPool.cpp
 * @brief Original level-object allocation boundary used by CLevel.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/enemy/CLevelObjectPool.h"

#include "gun_bros_re/debug/PerformanceProbe.h"
#include "gun_bros_re/gameplay/level/CLevel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kPoolArenaWidth = 1200;
constexpr float kPoolArenaHeight = 900;
constexpr float kSpawnDistance = 280;
constexpr float kRadians = 3.14159265f / 180;
constexpr std::size_t kEnemyPoolCapacity = 100;
constexpr std::size_t kPickupPoolCapacity = 20; // GetPickup :145637 compares the free index to 19.
}

CLevelObjectPool::CLevelObjectPool(ZPackTables &tables, const ZShaderProgram &program,
    const std::vector<CEnemy::Template> &catalog)
    : m_tables(&tables), m_program(&program), m_catalog(&catalog) {}

void CLevelObjectPool::BindRuntime(ZPackTables &tables, const ZShaderProgram &program,
    const std::vector<CEnemy::Template> &catalog) {
    m_tables = &tables;
    m_program = &program;
    m_catalog = &catalog;
}

void CLevelObjectPool::Clear() {
    m_enemies.clear();
    ClearPickups();
    m_pendingEnemies.clear();
    m_summoners.clear();
    m_spawnCount = 0;
    m_invalidSpawnCount = 0;
}

bool CLevelObjectPool::PreloadEnemies(const RequirementList &requirements, const CScript &levelScript) {
    if (m_tables == nullptr || m_program == nullptr || m_catalog == nullptr) { return false; }
    // CMap::GetRequirements :92483 and RequirementList::Add :191754/191802.
    // Only immutable meshes/textures enter this cache; no Flow export runs.
    std::vector<ZScriptResourceRef> pending = levelScript.GetResources();
    for (const auto &entry : requirements.objects) {
        ZScriptResourceRef ref;
        ref.packHash = entry.object.packHash;
        ref.sectionOrType = entry.objectType;
        ref.resourceId = entry.object.localIndex;
        pending.push_back(ref);
    }
    unsigned loaded = 0;
    for (std::size_t index = 0; index < pending.size(); ++index) {
        const ZScriptResourceRef ref = pending[index];
        if (ref.sectionOrType != static_cast<unsigned>(ZGameSection::Enemy) - 1 || ref.resourceId == 255) {
            continue;
        }
        const std::uint64_t key = (static_cast<std::uint64_t>(ref.packHash) << 32) | ref.resourceId;
        if (m_enemyModelCache.entries.count(key) != 0) { continue; }
        const CEnemy::Template *data = nullptr;
        for (const auto &entry : *m_catalog) {
            if (entry.packHash == ref.packHash && entry.ordinal == ref.resourceId) {
                data = &entry;
                break;
            }
        }
        if (data == nullptr || !CEnemy::Preload(*m_tables, *data, *m_program, m_enemyModelCache)) {
            std::printf("[preload] enemy=%08x:%u failed\n", ref.packHash, ref.resourceId);
            return false;
        }
        ++loaded;
        const auto &dependencies = data->script.GetResources();
        pending.insert(pending.end(), dependencies.begin(), dependencies.end());
    }
    m_enemyModelCache.hits = 0;
    m_enemyModelCache.misses = 0;
    std::printf("[preload] enemy-templates=%u map-references=%zu\n", loaded, requirements.objects.size());
    return true;
}

CEnemy *CLevelObjectPool::SpawnEnemy(std::size_t entry, float x, float y, bool forcePool) {
    PerformanceProbe::Scope timing(PerformanceProbe::counters.spawnMs);
    if (PerformanceProbe::enabled) { ++PerformanceProbe::counters.spawns; }
    if (m_tables == nullptr || m_program == nullptr || m_catalog == nullptr) { return nullptr; }
    if (entry >= m_catalog->size()) { return nullptr; }
    if (m_enemies.size() >= kEnemyPoolCapacity) { return nullptr; }
    // The original active counter falls only in Release, so corpses continue
    // occupying a slot until their object is actually retired.
    if (!forcePool && m_level != nullptr && m_enemies.size() >= m_level->GetEnemyLimit()) {
        return nullptr;
    }

    std::unique_ptr<CEnemy> actor(new CEnemy());
    actor->data = &(*m_catalog)[entry];
    CLevel *scriptLevel = nullptr;
    if (m_level != nullptr) { scriptLevel = m_level->GetScriptLevel(); }
    actor->SetLevelContext(scriptLevel);
    CEnemy::CombatState &state = actor->combat;
    state.templateRef.packHash = actor->data->packHash;
    state.templateRef.localIndex = static_cast<std::uint8_t>(actor->data->ordinal);
    state.enabled = actor->data->script.IsPresent();
    state.id = m_nextId++;
    state.randomState = static_cast<std::uint32_t>(state.id * 7919);
    actor->SetRandomSeed(state.randomState);
    state.x = std::clamp(x, 70.0f, kPoolArenaWidth - 70);
    state.y = std::clamp(y, 150.0f, kPoolArenaHeight - 70);
    if (m_usesMapCoordinates) {
        state.x = x;
        state.y = y;
    }
    state.previousX = state.x;
    state.previousY = state.y;
    if (!actor->Bind(*m_tables, *actor->data, true, m_program, &m_enemyModelCache)) {
        ++m_invalidSpawnCount;
        return nullptr;
    }
    actor->Spawn();
    CEnemy *result = actor.get();
    m_enemies.push_back(std::move(actor));
    ++m_spawnCount;
    return result;
}

CEnemy *CLevelObjectPool::GetNearbyEnemy(std::size_t entry, float centerX, float centerY) {
    float radius = 40;
    if (m_catalog != nullptr && entry < m_catalog->size()) {
        radius = std::max(radius, static_cast<float>((*m_catalog)[entry].radius116));
    }
    for (int attempt = 0; attempt < 120; ++attempt) {
        const float angle = (m_spawnCount * 137.5f + attempt * 137.5f) * kRadians;
        const float distance = kSpawnDistance + (attempt % 5) * 55;
        const float x = centerX + std::sin(angle) * distance;
        const float y = centerY - std::cos(angle) * distance;
        if (x < radius + 20 || x > kPoolArenaWidth - radius - 20 ||
            y < 145 + radius || y > kPoolArenaHeight - radius - 20) {
            continue;
        }
        bool free = true;
        for (const auto &actor : m_enemies) {
            const CEnemy::CombatState &state = actor->combat;
            const float otherRadius = std::max(35.0f, actor->GetPart(0).radius);
            if (!state.removed && std::hypot(x - state.x, y - state.y) < radius + otherRadius + 15) {
                free = false;
                break;
            }
        }
        if (free) { return SpawnEnemy(entry, x, y); }
    }
    std::printf("[arena] no free spawn position\n");
    return nullptr;
}

CEnemy *CLevelObjectPool::FindEnemy(Collision::ObjectId id) {
    for (auto &actor : m_enemies) {
        if (actor->combat.id == id) { return actor.get(); }
    }
    return nullptr;
}

std::size_t CLevelObjectPool::GetAliveEnemyCount() const {
    std::size_t count = 0;
    for (const auto &actor : m_enemies) {
        const CEnemy::CombatState &state = actor->combat;
        if (!state.dead && !state.removed) { ++count; }
    }
    return count;
}

void CLevelObjectPool::QueueEnemy(const GameObjectRef &resource, float x, float y,
    int objectId, bool forcePool, Collision::ObjectId summoner) {
    if (m_catalog == nullptr) { ++m_invalidSpawnCount; return; }
    for (std::size_t index = 0; index < m_catalog->size(); ++index) {
        if ((*m_catalog)[index].packHash == resource.packHash &&
            (*m_catalog)[index].ordinal == resource.localIndex) {
            m_pendingEnemies.push_back({index, x, y, objectId, forcePool, summoner});
            return;
        }
    }
    ++m_invalidSpawnCount;
    std::printf("[combat] missing spawn resource %08x:%u\n", resource.packHash, resource.localIndex);
}

std::vector<CEnemy *> CLevelObjectPool::FinishEnemySpawns() {
    std::vector<PendingEnemy> pending;
    pending.swap(m_pendingEnemies);
    std::vector<CEnemy *> spawned;
    spawned.reserve(pending.size());
    for (const PendingEnemy &entry : pending) {
        CEnemy *actor = SpawnEnemy(entry.entry, entry.x, entry.y, entry.forcePool);
        if (actor == nullptr) { continue; }
        actor->objectId = entry.objectId;
        actor->combat.summoner = entry.summoner;
        if (entry.summoner != 0) {
            m_summoners[actor->combat.id] = entry.summoner;
        }
        spawned.push_back(actor);
    }
    return spawned;
}

Collision::ObjectId CLevelObjectPool::GetSummoner(Collision::ObjectId owner) const {
    const auto found = m_summoners.find(owner);
    if (found == m_summoners.end()) { return 0; }
    return found->second;
}

void CLevelObjectPool::ReleaseEnemy(std::size_t index) {
    if (index >= m_enemies.size()) { return; }
    const Collision::ObjectId id = m_enemies[index]->combat.id;
    m_summoners.erase(id);
    m_enemies.erase(m_enemies.begin() + index);
}

CPickup *CLevelObjectPool::GetPickup() {
    if (m_pickups.size() >= kPickupPoolCapacity) { return nullptr; }
    auto pickup = std::make_unique<CPickup>();
    auto *result = pickup.get();
    m_pickups.push_back(std::move(pickup));
    return result;
}

void CLevelObjectPool::ReleasePickup(std::size_t index) {
    m_pickups.erase(m_pickups.begin() + index);
}

void CLevelObjectPool::ClearPickups() { m_pickups.clear(); }
