/** Regression for original mesh pursuit, shared maps and lock/reload invalidation. */
#include "gun_bros_re/gameplay/CLayerPathMesh.h"
#include "gun_bros_re/gameplay/enemy/CMeshPathFinder.h"
#include "gun_bros_re/gameplay/enemy/CFlock.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include <cmath>
#include <cstdio>

namespace {
void Word(std::vector<std::uint8_t> &bytes, int value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

std::vector<std::uint8_t> Corridor(int farY) {
    // map.bt: little-endian counts, i16 vertices, flags, four vertex ids, adjacency.
    std::vector<std::uint8_t> bytes;
    Word(bytes, 8); Word(bytes, 3); Word(bytes, 4);
    const int vertices[][2] = {{0, 0}, {100, 0}, {100, 100}, {0, 100},
        {200, 0}, {200, 100}, {200, farY}, {100, farY}};
    for (const auto &point : vertices) { Word(bytes, point[0]); Word(bytes, point[1]); }
    const int cells[][4] = {{0, 1, 2, 3}, {1, 4, 5, 2}, {2, 5, 6, 7}};
    for (unsigned cell = 0; cell < 3; ++cell) {
        bytes.push_back(0);
        for (int vertex : cells[cell]) { Word(bytes, vertex); }
        if (cell == 1) { bytes.push_back(2); Word(bytes, 0); Word(bytes, 2); }
        else { bytes.push_back(1); Word(bytes, 1); }
    }
    return bytes;
}

bool Near(float actual, float expected) { return std::abs(actual - expected) < 0.0001f; }

unsigned CheckEnemySight() {
    // collision_data.bt: vertices are signed 32-bit, not path mesh i16 pairs.
    std::vector<std::uint8_t> bytes;
    Word(bytes, 2);
    for (int value : {100, 0, 100, 100}) { Word(bytes, value); Word(bytes, 0); }
    Word(bytes, 1);
    bytes.push_back(7);
    Word(bytes, 0); Word(bytes, 1);
    CArrayInputStream input(bytes);
    CCollisionData collision;
    if (!collision.Load(input)) { return 1; }
    CMap map;
    ZWeaponCollision weaponCollision;
    weaponCollision.terrain = collision;
    CLevel level;
    level.SetMap(map, collision, weaponCollision, 1, 22);
    CEnemy enemy;
    enemy.SetLevelContext(&level);
    enemy.combat.x = 50;
    enemy.combat.y = 50;
    enemy.SetTarget(1, 150, 50, true);
    const int blocked = enemy.FunctionResolver(23, nullptr, 0);
    weaponCollision.terrain.SetGroupEnabled(7, false);
    const int unlocked = enemy.FunctionResolver(23, nullptr, 0);
    weaponCollision.terrain.SetGroupEnabled(7, true);
    enemy.SetTarget(1, 90, 50, true);
    const int nearby = enemy.FunctionResolver(23, nullptr, 0);
    // Collinear segments are not intersections in original Collision::LineIntersection.
    enemy.combat.x = 100;
    enemy.combat.y = -10;
    enemy.SetTarget(1, 100, 150, true);
    const int collinear = enemy.FunctionResolver(23, nullptr, 0);
    const bool passed = blocked == 0 && unlocked == 1 && nearby == 1 && collinear == 1;
    std::printf("[enemy-sight-check] blocked=%d unlocked=%d nearby=%d collinear=%d passed=%d\n",
        blocked, unlocked, nearby, collinear, passed);
    if (!passed) { return 1; }
    return 0;
}

unsigned CheckEnemyMoveAngle() {
    // Minimal map.bt fixture: null tile set, no requirements, path mesh and empty objects.
    std::vector<std::uint8_t> bytes(4, 0);
    bytes.push_back(0);
    bytes.push_back(2);
    bytes.push_back(static_cast<std::uint8_t>(ZMapLayerType::PathMesh));
    const auto corridor = Corridor(200);
    bytes.insert(bytes.end(), corridor.begin(), corridor.end());
    bytes.push_back(static_cast<std::uint8_t>(ZMapLayerType::Object));
    Word(bytes, 0);
    bytes.push_back(0);
    CArrayInputStream input(bytes);
    CMap map;
    if (!map.Init(input)) { return 1; }
    CLevel level;
    CLevel::Template levelTemplate;
    level.Bind(levelTemplate, map);
    const std::int16_t layer = 0;
    level.FunctionResolver(4, &layer, 1);
    CEnemy enemy;
    enemy.SetLevelContext(&level);
    enemy.combat.x = 50;
    enemy.combat.y = 50;
    enemy.combat.facing = 90;
    enemy.GetPart(0).radius = 10;
    const std::int16_t dash[] = {3, 500};
    enemy.FunctionResolver(0, dash, 2);
    const float forward = enemy.combat.destinationX;
    enemy.combat.facing = 270;
    enemy.FunctionResolver(0, dash, 2);
    const float backward = enemy.combat.destinationX;
    enemy.combat.facing = 90;
    const std::int16_t negative[] = {3, -5};
    enemy.FunctionResolver(0, negative, 2);
    const float minimum = enemy.combat.destinationX;
    const bool passed = Near(forward, 190) && Near(backward, 10) && Near(minimum, 51);
    std::printf("[enemy-move-angle-check] forward=%.3f backward=%.3f negative=%.3f passed=%d\n",
        forward, backward, minimum, passed);
    if (!passed) { return 1; }
    return 0;
}
}

int CheckEnemyNavigation() {
    auto bytes = Corridor(200);
    CArrayInputStream input(bytes);
    CLayerPathMesh path;
    if (!path.Init(input) || input.Available() != 0) { return 1; }
    unsigned failures = CheckEnemySight() + CheckEnemyMoveAngle();
    if (!Near(path.CastRay(50, 50, 1, 0), 150) ||
        !Near(path.CastRay(150, 50, 0, 1), 150) ||
        !Near(path.CastRay(50, 50, -1, 0), 50)) { ++failures; }
    path.SetNodeLocked(1, true);
    if (!Near(path.CastRay(50, 50, 1, 0), 150)) { ++failures; }
    path.SetNodeLocked(1, false);
    CEnemy first, second;
    for (CEnemy *enemy : {&first, &second}) {
        enemy->combat.targetId = 1;
        enemy->combat.targetAlive = true;
        enemy->combat.behaviour = 0;
        enemy->combat.targetX = 150;
        enemy->combat.targetY = 150;
    }
    std::vector<CEnemy::CombatState *> enemies{&first.combat, &second.combat};
    CFlock flock;
    flock.RefreshDistanceMaps(path, enemies);
    const auto &distances = flock.GetDistanceMap(1);
    if (distances.size() != 3 || !Near(distances[0], 200) || !Near(distances[1], 100) || distances[2] != 0) {
        ++failures;
    }
    CMeshPathFinder finder;
    float x, y;
    finder.Update(path, distances, 50, 40, 150, 150);
    finder.GetDestination(x, y);
    if (!Near(x, 100 + 50 / std::sqrt(2600.0f)) || !Near(y, 40 + 10 / std::sqrt(2600.0f))) { ++failures; }
    finder.Update(path, distances, 150, 50, 150, 150);
    finder.GetDestination(x, y);
    if (!Near(x, 150) || !Near(y, 101)) { ++failures; }
    finder.Update(path, distances, 120, 120, 150, 150);
    finder.GetDestination(x, y);
    if (x != 150 || y != 150) { ++failures; }
    if (path.FindNode(100, 40) != 1 || path.FindNode(150, 100) != 2 || path.FindNode(-20, 50) != 0) { ++failures; }

    // Locks change a distance map even when the target has not moved cells.
    path.SetNodeLocked(1, true);
    flock.RefreshDistanceMaps(path, enemies);
    finder.Update(path, flock.GetDistanceMap(1), 50, 40, 150, 150);
    finder.GetDestination(x, y);
    if (x != 50 || y != 50) { ++failures; }
    path.SetNodeLocked(1, false);
    flock.RefreshDistanceMaps(path, enemies);
    if (!Near(flock.GetDistanceMap(1)[0], 200)) { ++failures; }

    // Same-sized topology reload must not reuse the previous path's distances.
    bytes = Corridor(400);
    CArrayInputStream reloaded(bytes);
    if (!path.Init(reloaded)) { return 1; }
    flock.RefreshDistanceMaps(path, enemies);
    if (!Near(flock.GetDistanceMap(1)[0], 300)) { ++failures; }
    enemies.clear();
    flock.RefreshDistanceMaps(path, enemies);
    if (!flock.GetDistanceMap(1).empty()) { ++failures; }
    std::printf("[enemy-navigation-check] shared-edge=closest-point target-map=shared locks=live reload=same-size failures=%u\n", failures);
    return failures != 0;
}
