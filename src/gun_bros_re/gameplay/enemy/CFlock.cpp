/** Original: src/gunbros/flock.cpp RefreshFlock :170259.
 * Windows graphics/resource storage is adapted; original data comes from BIG.
 */
#include "gun_bros_re/gameplay/enemy/CFlock.h"
#include "gun_bros_re/gameplay/enemy/CEnemy.h"
#include "gun_bros_re/gameplay/map/CLayerPathMesh.h"
#include "gun_bros_re/debug/PerformanceProbe.h"
#include <set>

void CFlock::RefreshDistanceMaps(const CLayerPathMesh &path,
    const std::vector<CEnemy::CombatState *> &enemies) {
    if (m_path != &path) { Clear(); m_path = &path; }
    std::set<Collision::ObjectId> used;
    for (const auto *enemy : enemies) {
        if (enemy->dead || !enemy->targetAlive || enemy->behaviour != 0) { continue; }
        if (!used.insert(enemy->targetId).second) { continue; }
        const int cell = path.FindNode(enemy->targetX, enemy->targetY);
        auto &map = m_distanceMaps[enemy->targetId];
        PerformanceProbe::Scope timing(PerformanceProbe::counters.pathSearchMs);
        if (PerformanceProbe::enabled) {
            ++PerformanceProbe::counters.pathSearches;
            PerformanceProbe::counters.pathNodes = static_cast<unsigned>(path.GetNodes().size());
        }
        if (!PerformanceProbe::uncachedPaths && map.cell == cell && map.revision == path.GetRevision() &&
            map.distances.size() == path.GetNodes().size()) {
            if (PerformanceProbe::enabled) { ++PerformanceProbe::counters.pathCacheHits; }
            continue;
        }
        path.CalculateDistanceMapAtCell(cell, map.distances);
        map.cell = cell;
        map.revision = path.GetRevision();
    }
    // Local PvP summons can target enemies; retire those maps with their targets.
    for (auto current = m_distanceMaps.begin(); current != m_distanceMaps.end();) {
        if (used.count(current->first) == 0) { current = m_distanceMaps.erase(current); }
        else { ++current; }
    }
}

const std::vector<float> &CFlock::GetDistanceMap(Collision::ObjectId target) const {
    static const std::vector<float> empty;
    const auto found = m_distanceMaps.find(target);
    if (found == m_distanceMaps.end()) { return empty; }
    return found->second.distances;
}

void CFlock::RefreshFlock(const std::vector<CEnemy::CombatState *> &enemies) {
    // CFlock::RefreshFlock, iOS :170259. Algorithm constants, not BIG data.
    constexpr float kNeighbourDistanceSquared = 10000.0f;
    constexpr float kSeparationWeight = 1000.0f;
    for (CEnemy::CombatState *enemy : enemies) {
        enemy->flockX = 0;
        enemy->flockY = 0;
        for (const CEnemy::CombatState *other : enemies) {
            if (enemy == other) { continue; }
            const float dx = enemy->x - other->x;
            const float dy = enemy->y - other->y;
            const float distanceSquared = dx * dx + dy * dy;
            // Original coincident centres contribute zero, without jitter.
            if (distanceSquared > 0 && distanceSquared <= kNeighbourDistanceSquared) {
                const float weight = kSeparationWeight / distanceSquared;
                enemy->flockX += dx * weight;
                enemy->flockY += dy * weight;
            }
        }
    }
}
