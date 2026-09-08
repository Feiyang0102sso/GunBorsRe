/** @file CombatGeometry.h
 * @brief Shared continuous collision tests for actors and scripted map props.
 */
#ifndef GUN_BROS_RE_COMBATGEOMETRY_H
#define GUN_BROS_RE_COMBATGEOMETRY_H
#include "gun_bros/CCollisionData.h"
#include <algorithm>
#include <cmath>
namespace CombatGeometry {
// Earliest point where a moving circle overlaps a stationary one.
inline float CircleFraction(float x, float y, float dx, float dy, float cx, float cy, float radius) {
    const float ox = x - cx, oy = y - cy;
    const float c = ox * ox + oy * oy - radius * radius;
    if (c <= 0) { return 0; }
    const float a = dx * dx + dy * dy;
    if (a <= 0) { return 2; }
    const float b = ox * dx + oy * dy;
    const float discriminant = b * b - a * c;
    if (discriminant < 0) { return 2; }
    const float fraction = (-b - std::sqrt(discriminant)) / a;
    if (fraction < 0 || fraction > 1) { return 2; }
    return fraction;
}

// A swept circle against a finite edge is its strip plus both endpoint caps.
inline float EdgeFraction(float x, float y, float dx, float dy,
    const CollisionPoint &a, const CollisionPoint &b, float radius) {
    float nearest = std::min(CircleFraction(x, y, dx, dy, a.x, a.y, radius),
        CircleFraction(x, y, dx, dy, b.x, b.y, radius));
    const float ex = b.x - a.x, ey = b.y - a.y;
    const float length = std::hypot(ex, ey);
    if (length <= 0) { return nearest; }
    const float nx = -ey / length, ny = ex / length;
    const float distance = (x - a.x) * nx + (y - a.y) * ny;
    const float velocity = dx * nx + dy * ny;
    for (int side = -1; side <= 1; side += 2) {
        float fraction = 0;
        if (std::abs(distance) > radius) {
            if (std::abs(velocity) < 0.00001f) { continue; }
            fraction = (side * radius - distance) / velocity;
        }
        if (fraction < 0 || fraction > 1) { continue; }
        const float along = ((x + dx * fraction - a.x) * ex +
            (y + dy * fraction - a.y) * ey) / length;
        if (along >= 0 && along <= length) { nearest = std::min(nearest, fraction); }
    }
    return nearest;
}

}
#endif
