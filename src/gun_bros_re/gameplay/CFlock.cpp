#include "gun_bros_re/gameplay/CFlock.h"
#include "gun_bros_re/gameplay/ZEnemyCombat.h"

void CFlock::RefreshFlock(const std::vector<ZEnemyCombat *> &enemies) {
    // CFlock::RefreshFlock, iOS :170259. Algorithm constants, not BIG data.
    constexpr float kNeighbourDistanceSquared = 10000.0f;
    constexpr float kSeparationWeight = 1000.0f;
    for (ZEnemyCombat *enemy : enemies) {
        enemy->flockX = 0;
        enemy->flockY = 0;
        for (const ZEnemyCombat *other : enemies) {
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
