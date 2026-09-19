/** Original: src/gunbros/flock.cpp RefreshFlock :170259.
 * Windows graphics/resource storage is adapted; original data comes from BIG.
 */
/** Original CFlock neighbour separation; route caching remains in ILayerPath. */
// Updated: CFlock also owns the original per-target distance maps. ILayerPath's
// older point-to-point route cache remains for non-enemy callers.
#pragma once
#include <vector>
#include "gun_bros_re/gameplay/enemy/CEnemy.h"
class CLayerPathMesh;

class CFlock {
public:
    // Refresh before any enemy moves. The host stores the cached vector on
    // each actor instead of looking up the original pointer/vector table.
    static void RefreshFlock(const std::vector<CEnemy::CombatState *> &enemies);
    // Original RefreshDistanceMaps :170441 shares routes for each target.
    void RefreshDistanceMaps(const CLayerPathMesh &path,
        const std::vector<CEnemy::CombatState *> &enemies);
    const std::vector<float> &GetDistanceMap(Collision::ObjectId target) const;
    void Clear() { m_distanceMaps.clear(); m_path = nullptr; }
private:
    struct DistanceMap {
        int cell = -1;
        std::uint64_t revision = 0;
        std::vector<float> distances;
    };
    const CLayerPathMesh *m_path = nullptr;
    std::map<Collision::ObjectId, DistanceMap> m_distanceMaps;
};
