/** @file CEnemySpawner.cpp
 * @brief CEnemySpawner rules from iOS :146174-146945 and spawner.link.
 */
#include "gun_bros/CEnemySpawner.h"
#include "gun_bros/CLevel.h"
#include <cstdio>

void CEnemySpawner::Bind(CLevel &level, IEnemySpawnWorld *world) {
    m_level = &level;
    m_world = world;
    m_spawnCount = 0;
    m_unsupported = 0;
    Reset();
}

void CEnemySpawner::Reset() {
    for (Rule &rule : m_rules) {
        rule = Rule();
    }
    m_paused = false;
    m_maximum = 0;
    m_layer = -1;
    m_allNodes = true;
    m_enabledNodes.clear();
}

bool CEnemySpawner::Spawn(int resource, int layer, int node, int objectId) {
    GameObjectRef enemy;
    if (m_level == nullptr || m_world == nullptr || !m_level->GetResource(resource, enemy)) {
        return false;
    }
    if (m_world->CountEnemies() >= static_cast<int>(m_level->GetEnemyLimit())) { return false; }
    if (layer < 0) {
        layer = m_layer;
    }
    if (!m_world->SpawnEnemy(enemy, layer, node, objectId)) {
        return false;
    }
    // Script-tracked spawns are the ones a level later addresses by object id
    // -- the pack12 babe among them. Worth seeing which template and node the
    // script rolled.
    if (objectId >= 0) {
        std::printf("[spawner] tracked spawn resource=%d object=%08x:%u layer=%d node=%d id=%d\n",
            resource, enemy.packHash, enemy.localIndex, layer, node, objectId);
    }
    ++m_spawnCount;
    return true;
}

void CEnemySpawner::Update(int deltaMs) {
    if (m_paused || m_world == nullptr || deltaMs <= 0) {
        return;
    }
    if (m_maximum > 0 && m_world->CountEnemies() >= m_maximum) {
        return;
    }
    for (Rule &rule : m_rules) {
        if (!rule.enabled) {
            continue;
        }
        rule.elapsedMs += deltaMs;
        // Original UpdateRule allows at most two spawns to catch up per tick.
        for (int attempt = 0; attempt < 2 && rule.elapsedMs >= rule.intervalMs; ++attempt) {
            GameObjectRef enemy;
            if (!m_level->GetResource(rule.resource, enemy)) {
                break;
            }
            if (rule.maximum >= 0 && m_world->CountEnemies(&enemy) >= rule.maximum) {
                break;
            }
            if (!Spawn(rule.resource, rule.layer, -1, -1)) {
                break;
            }
            if (rule.remaining > 0) {
                --rule.remaining;
                if (rule.remaining == 0) {
                    rule.enabled = false;
                    break;
                }
            }
            if (rule.intervalMs <= 0) {
                break;
            }
            rule.elapsedMs -= rule.intervalMs;
        }
    }
}

std::int16_t CEnemySpawner::FunctionResolver(std::uint8_t function,
    const std::int16_t *arguments, std::uint8_t argumentCount) {
    int first = 0;
    int second = 0;
    if (argumentCount > 0) {
        first = arguments[0];
    }
    if (argumentCount > 1) {
        second = arguments[1];
    }
    Rule *rule = nullptr;
    if (first >= 0 && first < static_cast<int>(m_rules.size())) {
        rule = &m_rules[first];
    }
    switch (function) {
    case 0: m_paused = first != 0; break;
    case 1:
        if (rule != nullptr) { rule->enabled = true; rule->elapsedMs = 0; }
        break;
    case 2:
        if (rule != nullptr) { rule->enabled = false; }
        break;
    case 3:
        if (rule != nullptr) { *rule = Rule(); }
        break;
    case 4:
        if (rule != nullptr) { rule->resource = second; }
        break;
    case 5:
        if (rule != nullptr) { rule->maximum = second; }
        break;
    case 6:
        if (rule != nullptr) { rule->intervalMs = second * 1000 / 256; }
        break;
    case 7:
        if (rule != nullptr) { rule->remaining = second; }
        break;
    case 8: m_maximum = first; break;
    case 9: Reset(); break;
    case 10: m_allNodes = true; m_enabledNodes.clear(); break;
    case 11: m_allNodes = false; m_enabledNodes.clear(); break;
    case 12: m_enabledNodes.push_back(first); break;
    case 13: m_layer = first; break;
    case 14: m_layer = -1; break;
    case 15: return Spawn(first, -1, -1, -1);
    case 16: {
        int node = -1;
        int objectId = -1;
        if (argumentCount >= 3) { node = arguments[2]; }
        if (argumentCount >= 4) { objectId = arguments[3]; }
        return Spawn(first, second, node, objectId);
    }
    case 17:
        if (rule != nullptr) { rule->layer = second; }
        break;
    case 18:
    case 19:
    case 21: {
        std::printf("[spawner] pickup native=%u args=", function);
        for (unsigned index = 0; index < argumentCount; ++index) { std::printf("%d,", arguments[index]); }
        std::printf("\n");
        GameObjectRef pickup;
        if (m_world == nullptr || !m_level->GetResource(first, pickup)) { return false; }
        if (function == 18 && argumentCount >= 4) {
            return m_world->SpawnPickup(pickup, second, arguments[2], arguments[3], false);
        }
        if (function == 19 && argumentCount >= 3) {
            return m_world->SpawnPickup(pickup, second, -1, arguments[2], true);
        }
        if (function == 21 && argumentCount >= 4) {
            return m_world->SpawnPickupAt(pickup, arguments[2], arguments[3], second);
        }
        return false;
    }
    default:
        ++m_unsupported;
        std::printf("[spawner] unsupported native %u\n", function);
        break;
    }
    return 0;
}
