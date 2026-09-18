/** @file IEnemySpawnerScriptInterface.cpp
 * Original: src/gunbros/enemySpawner.cpp IEnemySpawnerScriptInterface::FunctionResolver :146664.
 * Offline host: resolver methods share the concrete CEnemySpawner state.
 * No network implementation or duplicate script state is introduced.
 */
#include "gun_bros_re/gameplay/enemy/CEnemySpawner.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include <cstdio>

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
    case 20: {
        // IEnemySpawnerScriptInterface::SpawnMPMatchPickup :147070.
        GameObjectRef pickup;
        if (argumentCount < 2 || m_world == nullptr || !m_level->GetResource(first, pickup)) { return false; }
        return m_world->SpawnMPMatchPickup(pickup, second);
    }
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
