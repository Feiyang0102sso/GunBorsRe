#include "gun_bros_re/gameplay/CFlock.h"
#include "gun_bros_re/gameplay/EnemyCombat.h"

void CFlock::RefreshFlock(const std::vector<EnemyCombat *> &enemies) {
    // CFlock::RefreshFlock, iOS :170259. Algorithm constants, not BIG data.
    constexpr float kNeighbourDistanceSquared = 10000.0f;
    constexpr float kSeparationWeight = 1000.0f;
    for (EnemyCombat *enemy : enemies) {
        enemy->flockX = 0;
        enemy->flockY = 0;
        for (const EnemyCombat *other : enemies) {
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
