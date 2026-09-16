/** @file SurvivalPilot.cpp
 * @brief Sample traversable corridors once, then plan short test input routes.
 */
#define NOMINMAX
#include "tests/gameplay/SurvivalPilot.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <queue>

namespace {
constexpr float kGridSpacing = 24;
constexpr float kAttackDistance = 120;
constexpr float kRadiansToDegrees = 180.0f / 3.14159265f;
}

SurvivalPilot::SurvivalPilot(CLevel &scene, const ZMapRectangle &bounds) : m_scene(scene) {
    const float margin = scene.GetPlayerRadius() + 2;
    const int columns = static_cast<int>((bounds.width - margin * 2) / kGridSpacing) + 1;
    const int rows = static_cast<int>((bounds.height - margin * 2) / kGridSpacing) + 1;
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            Node node;
            node.x = bounds.x + margin + column * kGridSpacing;
            node.y = bounds.y + margin + row * kGridSpacing;
            m_nodes.push_back(node);
        }
    }
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const int index = row * columns + column;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int x = column + dx;
                    const int y = row + dy;
                    if (x < 0 || y < 0 || x >= columns || y >= rows) { continue; }
                    const int other = y * columns + x;
                    if (other <= index) { continue; }
                    Node &first = m_nodes[index];
                    Node &second = m_nodes[other];
                    if (scene.HasClearPath(first.x, first.y, second.x, second.y, scene.GetPlayerRadius() - 0.5f)) {
                        first.neighbors.push_back(other);
                        second.neighbors.push_back(index);
                    }
                }
            }
        }
    }
}

void SurvivalPilot::Plan(const ZCombatEnemy &target) {
    m_route.clear();
    m_step = 0;
    int start = -1;
    float nearest = 150;
    std::vector<int> starts;
    const bool touching = !m_scene.HasClearPath(m_scene.GetPlayer().x, m_scene.GetPlayer().y,
        m_scene.GetPlayer().x, m_scene.GetPlayer().y, m_scene.GetPlayerRadius() - 0.5f);
    for (unsigned index = 0; index < m_nodes.size(); ++index) {
        const Node &node = m_nodes[index];
        const float distance = std::hypot(node.x - m_scene.GetPlayer().x, node.y - m_scene.GetPlayer().y);
        if (distance < 150 && !node.neighbors.empty() &&
            (m_scene.HasClearPath(m_scene.GetPlayer().x, m_scene.GetPlayer().y, node.x, node.y, m_scene.GetPlayerRadius() - 0.5f) ||
                (touching && m_scene.CanWalkTo(m_scene.GetPlayer().x, m_scene.GetPlayer().y, node.x, node.y)))) {
            starts.push_back(static_cast<int>(index));
            if (distance < nearest) { nearest = distance; start = static_cast<int>(index); }
        }
    }
    if (start < 0) { ++m_missingRoutes; return; }
    const float infinity = (std::numeric_limits<float>::max)();
    std::vector<float> distances(m_nodes.size(), infinity);
    std::vector<int> parents(m_nodes.size(), -1);
    using QueueItem = std::pair<float, int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;
    // A coarse sample can contain a tiny disconnected pocket beside an
    // obstacle. The player is not itself a grid node: seed every point it can
    // actually walk straight to, instead of committing to the closest pocket.
    for (int index : starts) {
        const Node &node = m_nodes[index];
        distances[index] = std::hypot(node.x - m_scene.GetPlayer().x, node.y - m_scene.GetPlayer().y);
        queue.push({distances[index], index});
    }
    while (!queue.empty()) {
        const QueueItem item = queue.top();
        queue.pop();
        if (item.first > distances[item.second]) { continue; }
        const Node &node = m_nodes[item.second];
        for (int next : node.neighbors) {
            const float distance = item.first + std::hypot(node.x - m_nodes[next].x, node.y - m_nodes[next].y);
            if (distance < distances[next]) {
                distances[next] = distance;
                parents[next] = item.second;
                queue.push({distance, next});
            }
        }
    }
    const ZEnemyCombat &enemy = target.model.enemy.combat;
    const float angle = m_elapsed * 0.0003f;
    const float desiredX = enemy.x + std::cos(angle) * kAttackDistance;
    const float desiredY = enemy.y + std::sin(angle) * kAttackDistance;
    int goal = start;
    float best = infinity;
    for (unsigned index = 0; index < m_nodes.size(); ++index) {
        if (distances[index] == infinity) { continue; }
        const Node &node = m_nodes[index];
        float score = std::abs(std::hypot(node.x - enemy.x, node.y - enemy.y) - kAttackDistance);
        score += std::hypot(node.x - desiredX, node.y - desiredY) * 0.4f + distances[index] * 0.02f;
        if (m_retreatMs > 0) {
            // Lure ranged enemies out of inaccessible spawn corridors instead
            // of standing inside their stopping distance behind cover.
            score = -std::min(650.0f, std::hypot(node.x - enemy.x, node.y - enemy.y)) + distances[index] * 0.15f;
        }
        if (score >= best) { continue; }
        if (m_retreatMs <= 0 && !m_scene.HasClearPath(node.x, node.y, enemy.x, enemy.y, 1)) { score += 500; }
        if (score < best) { best = score; goal = static_cast<int>(index); }
    }
    for (int current = goal; current >= 0; current = parents[current]) { m_route.push_back(current); }
    if (m_retreatMs == 8000) {
        unsigned reachable = 0;
        for (float distance : distances) { if (distance != infinity) { ++reachable; } }
        std::printf("[survival-pilot] retreat start=%d goal=%d reachable=%u player=%.1f,%.1f\n",
            start, goal, reachable, m_scene.GetPlayer().x, m_scene.GetPlayer().y);
    }
    std::reverse(m_route.begin(), m_route.end());
}

void SurvivalPilot::Update(int deltaMs, float &moveX, float &moveY) {
    m_elapsed += deltaMs;
    m_planTimer -= deltaMs;
    m_retreatMs = std::max(0, m_retreatMs - deltaMs);
    m_noDamageMs += deltaMs;
    float damage = m_scene.damageDealt;
    for (const auto &actor : m_scene.GetEnemies()) { damage += actor->model.enemy.combat.totalDamage; }
    if (damage != m_lastDamage) {
        m_noDamageMs = 0;
        m_lastDamage = damage;
    }
    if (m_noDamageMs > 12000) {
        m_retreatMs = 8000;
        m_noDamageMs = 0;
        m_planTimer = 0;
    }
    moveX = 0;
    moveY = 0;
    ZCombatEnemy *target = nullptr;
    // Keep an engagement stable. Re-selecting the nearest enemy while retreating
    // can alternate between two ranged units and trap the pilot at their midpoint.
    ZCombatEnemy *previousTarget = m_scene.Find(m_target);
    if (previousTarget != nullptr && previousTarget->model.enemy.combat.enabled &&
        previousTarget->model.enemy.combat.targetable && previousTarget->model.enemy.CanReceiveProjectile(0, kPlayerCombatId)) {
        target = previousTarget;
    }
    float nearest = 100000;
    if (target == nullptr) {
        for (const auto &actor : m_scene.GetEnemies()) {
            // Match the brother's targeting filters. Authored map actors can
            // accept collision callbacks without being combat targets.
            if (!actor->model.enemy.combat.enabled || !actor->model.enemy.combat.targetable ||
                !actor->model.enemy.CanReceiveProjectile(0, kPlayerCombatId)) { continue; }
            const ZEnemyCombat &enemy = actor->model.enemy.combat;
            const float distance = std::hypot(enemy.x - m_scene.GetPlayer().x, enemy.y - m_scene.GetPlayer().y);
            if (distance < nearest) { nearest = distance; target = actor.get(); }
        }
    }
    if (target == nullptr) { return; }
    const ZEnemyCombat &enemy = target->model.enemy.combat;
    float aimX = enemy.x;
    float aimY = enemy.y;
    const unsigned partCount = target->model.enemy.GetPartCount();
    if (partCount > 1) {
        const unsigned part = static_cast<unsigned>(m_elapsed / 1800) % partCount;
        const ZEnemyPart &piece = target->model.enemy.GetPart(part);
        if (piece.visible && piece.radius > 0) {
            float radius = 0;
            m_scene.EnemyCircle(*target, part, aimX, aimY, radius);
        }
    }
    m_scene.GetPlayer().facing = std::atan2(aimY - m_scene.GetPlayer().y, aimX - m_scene.GetPlayer().x) * kRadiansToDegrees + 90;
    if (m_planTimer <= 0 || m_target != enemy.id) {
        Plan(*target);
        m_planTimer = 1000;
        m_target = enemy.id;
    }
    while (m_step < m_route.size()) {
        const Node &node = m_nodes[m_route[m_step]];
        moveX = node.x - m_scene.GetPlayer().x;
        moveY = node.y - m_scene.GetPlayer().y;
        if (std::hypot(moveX, moveY) > 6) { return; }
        ++m_step;
    }
    moveX = 0;
    moveY = 0;
}

void SurvivalPilot::Report() const {
    std::printf("[survival-pilot] nodes=%zu route=%u/%zu missing=%u target=%llu retreat=%d\n",
        m_nodes.size(), m_step, m_route.size(), m_missingRoutes,
        static_cast<unsigned long long>(m_target), m_retreatMs);
    if (m_step < m_route.size()) {
        const Node &node = m_nodes[m_route[m_step]];
        std::printf("[survival-pilot] destination=%.1f,%.1f player=%.1f,%.1f\n", node.x, node.y, m_scene.GetPlayer().x, m_scene.GetPlayer().y);
    }
}
