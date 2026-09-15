/** Real BIG actors must separate while pursuing the same distant target. */
#include "gameplay/SurvivalStudy.h"
#include "gun_bros_re/gameplay/CombatScene.h"
#include "gun_bros_re/gameplay/CFlock.h"
#include <cmath>
#include <cstdio>
#include <limits>

namespace {
class StraightLink : public ILayerPath {
public:
    StraightLink() {
        m_nodes.resize(2);
        m_nodes[0].x = 600; m_nodes[0].y = 440;
        m_nodes[1].x = 700; m_nodes[1].y = 440;
        m_nodes[0].neighbours = {1};
        m_nodes[1].neighbours = {0};
    }
};
}

int CheckFlockMovement(CombatScene &scene) {
    scene.Reset();
    scene.playerX = 1000;
    scene.playerY = 450;
    CombatEnemy *first = scene.Spawn(0, 600, 440);
    CombatEnemy *second = scene.Spawn(0, 600, 460);
    if (first == nullptr || second == nullptr) { return 1; }
    // Finish spawn animation before isolating the movement native from AI.
    for (int step = 0; step < 80; ++step) { scene.Update(16, 0, 0, false); }
    first->model.enemy.combat.x = 600;
    first->model.enemy.combat.y = 440;
    second->model.enemy.combat.x = 600;
    second->model.enemy.combat.y = 460;
    for (int step = 0; step < 60; ++step) {
        for (CombatEnemy *actor : {first, second}) {
            auto &state = actor->model.enemy.combat;
            state.behaviour = 0;
            state.arrivalDistance = 0;
            state.targetType = 0;
            state.targetRange = 100000;
            state.variables[0] = 60;
            state.variables[12] = 0;
        }
        scene.Update(16, 0, 0, false);
    }
    const auto &a = first->model.enemy.combat;
    const auto &b = second->model.enemy.combat;
    const float gap = std::hypot(a.x - b.x, a.y - b.y);
    const bool passed = gap > 20 && a.x > 600 && b.x > 600;
    std::printf("[flock-check] initial-gap=20 final-gap=%.3f forward=%.3f/%.3f passed=%d\n",
        gap, a.x - 600, b.x - 600, passed);
    unsigned failures = 0;
    if (!passed) { ++failures; }
    // A frozen actor keeps its position even with neighbours exerting force.
    auto &enemy = first->model.enemy;
    const float frozenX = enemy.combat.x;
    const float frozenY = enemy.combat.y;
    enemy.stun.SetStunned(750, 30, 4);
    scene.Update(16, 0, 0, false);
    if (enemy.combat.x != frozenX || enemy.combat.y != frozenY) { ++failures; }
    enemy.stun.ClearStunned();
    // Link movement must still reach the exact node used by the original
    // CLinkPathFinder::Update :168635, and finish once-mode without a loop.
    StraightLink link;
    enemy.combat.x = 600;
    enemy.combat.y = 440;
    enemy.combat.flockX = 0;
    enemy.combat.flockY = 0;
    enemy.SetPath(&link);
    const std::int16_t follow[] = {2};
    const std::int16_t once[] = {1};
    enemy.FunctionResolver(0, follow, 1);
    enemy.FunctionResolver(24, once, 1);
    for (int step = 0; step < 160 && !enemy.combat.arrived; ++step) {
        enemy.combat.variables[0] = 60;
        enemy.Update(16);
    }
    if (!enemy.combat.arrived || enemy.combat.x != 700 || enemy.combat.y != 440) { ++failures; }
    // Verify the original cutoff, coincidence and removal from the next list.
    EnemyCombat left;
    EnemyCombat right;
    std::vector<EnemyCombat *> neighbours = {&left, &right};
    right.x = 100;
    CFlock::RefreshFlock(neighbours);
    if (left.flockX != -10 || right.flockX != 10) { ++failures; }
    right.x = 101;
    CFlock::RefreshFlock(neighbours);
    if (left.flockX != 0 || right.flockX != 0) { ++failures; }
    right.x = 0;
    CFlock::RefreshFlock(neighbours);
    if (left.flockX != 0 || right.flockX != 0) { ++failures; }
    neighbours.pop_back();
    left.flockX = 100;
    CFlock::RefreshFlock(neighbours);
    if (left.flockX != 0) { ++failures; }
    std::printf("[flock-check] stun/link-arrival/cutoff/coincidence/membership failures=%u\n", failures);
    if (failures != 0) { return 1; }
    return 0;
}
