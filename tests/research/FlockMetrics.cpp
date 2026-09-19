/** @file FlockMetrics.cpp
 * @brief Nearest-neighbour spacing over the live actors. Diagnostic only.
 */
#include "research/FlockMetrics.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include <cmath>
#include <limits>

FlockMetrics MeasureFlock(const CLevel &scene) {
    FlockMetrics metrics;
    float sum = 0;
    float minimum = std::numeric_limits<float>::infinity();
    unsigned count = 0;
    for (const auto &actor : scene.GetEnemies()) {
        const auto &state = actor->combat;
        if (!state.enabled || state.dead || state.removed) { continue; }
        float nearest = std::numeric_limits<float>::infinity();
        for (const auto &other : scene.GetEnemies()) {
            const auto &neighbour = other->combat;
            if (actor == other || !neighbour.enabled || neighbour.dead || neighbour.removed) { continue; }
            const float distance = std::hypot(state.x - neighbour.x, state.y - neighbour.y);
            nearest = std::fmin(nearest, distance);
            // Diagnostic threshold only; does not define a gameplay radius.
            if (distance < 20 && state.id < neighbour.id) { ++metrics.closePairs; }
        }
        if (std::isfinite(nearest)) {
            sum += nearest;
            minimum = std::fmin(minimum, nearest);
            ++count;
        }
    }
    if (count > 0) { metrics.nearestMean = sum / count; metrics.minimum = minimum; }
    return metrics;
}
