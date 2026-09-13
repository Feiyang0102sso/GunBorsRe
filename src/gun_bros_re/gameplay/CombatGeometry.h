/** @file CombatGeometry.h
 * @brief Shared continuous collision tests for actors and scripted map props.
 */
#ifndef GUN_BROS_RE_COMBATGEOMETRY_H
#define GUN_BROS_RE_COMBATGEOMETRY_H
#include "gun_bros_re/gameplay/CCollisionData.h"
#include <algorithm>
#include <cmath>
namespace CombatGeometry {
/** Original Collision::CircleCircle :65444, verified against ARM 0x37710.
 * Keep its discriminant limits and travel test; these are not standard CCD.
 */
inline bool CircleCircle(const CollisionPoint &previous, const CollisionPoint &current, float radius,
    const CollisionPoint &otherPrevious, const CollisionPoint &otherCurrent, float otherRadius,
    float &fraction) {
    const float moveX = current.x - previous.x, moveY = current.y - previous.y;
    const float relativeX = otherCurrent.x - otherPrevious.x - moveX;
    const float relativeY = otherCurrent.y - otherPrevious.y - moveY;
    const float offsetX = otherPrevious.x - previous.x, offsetY = otherPrevious.y - previous.y;
    const float distanceSquared = offsetX * offsetX + offsetY * offsetY;
    const float combinedRadius = radius + otherRadius;
    const float radiusSquared = combinedRadius * combinedRadius;
    if (distanceSquared <= radiusSquared) { fraction = 0; return true; }
    const float a = relativeX * relativeX + relativeY * relativeY;
    const float b = 2 * (relativeX * offsetX + relativeY * offsetY);
    const float discriminant = b * b + 4 * a * (radiusSquared - distanceSquared);
    if (!(discriminant >= 0.000001f && discriminant <= 1.0f)) { return false; }
    fraction = (-b - std::sqrt(discriminant)) / (2 * a);
    return fraction > 0 && fraction * fraction <= moveX * moveX + moveY * moveY;
}

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
