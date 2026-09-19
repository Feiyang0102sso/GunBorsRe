/** @file Collision.h
 * @brief Shared continuous collision tests for actors and scripted map props.
 * @brief World-space combat messages shared by actors and projectiles.
 */
#ifndef GUN_BROS_RE_COLLISION_H
#define GUN_BROS_RE_COLLISION_H
#include "gun_bros_re/gameplay/collision/CCollisionData.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include <algorithm>
#include <cmath>
namespace Collision {
// Value messages and stable IDs are desktop adaptations; the recovered geometry
// functions below retain their original algorithms. This is not an ARM layout.
// IDs survive vector growth and never point at actors that have been removed.
using ObjectId = std::uint64_t;
constexpr Collision::ObjectId NoObject = 0;
constexpr Collision::ObjectId Player = 1;
constexpr Collision::ObjectId Brother = UINT64_MAX;

enum class HitResult { Pending, Ignored, Hit, Killed };

struct Hit {
    Collision::ObjectId projectile = 0;
    Collision::ObjectId owner = 0;
    GameObjectRef weapon;
    unsigned weaponSlot = 0;
    unsigned weaponMasteryLimit = 0;
    GameObjectRef bullet;
    bool critical = false;
    int ownerType = 0;
    float damage = 0;
    float x = 0;
    float y = 0;
    float direction = 0;
    // CBullet template +120/+124, consumed only by CBrother::HandleCollision.
    float knockbackSpeed = 0;
    unsigned knockbackDurationMs = 0;
    std::uint32_t flags = 0;
    int part = -1;
    int edge = -1;
    bool splash = false;
    // CProp native 7 passes an actor source, not a bullet carrying its owner.
    bool propExplosion = false;
    bool percentDamage = false;
    bool applyArmorAttack = true; // Scripted fixed-damage air strikes bypass gun bonuses.
    int spawnObjectId = -1;
    bool forceSpawn = false;
};

struct Trace {
    Collision::ObjectId target = 0;
    float fraction = 1;
    int part = -1;
    int edge = -1;
    float normalX = 0;
    float normalY = 0;
};


/** Original Collision::CircleCircle :65444, verified against ARM 0x37710.
 * Keep its discriminant limits and travel test; these are not standard CCD.
 */
inline bool CircleCircle(const ZCollisionPoint &previous, const ZCollisionPoint &current, float radius,
    const ZCollisionPoint &otherPrevious, const ZCollisionPoint &otherCurrent, float otherRadius,
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
    const ZCollisionPoint &a, const ZCollisionPoint &b, float radius) {
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

/** Nearest intersection prevents fast projectiles from tunnelling through walls. */
inline float SegmentFraction(float x, float y, float dx, float dy, const CCollisionData *scene,
    float *normalX = nullptr, float *normalY = nullptr, bool *hit = nullptr) {
    float fraction = 1.0f;
    if (scene == nullptr) { return fraction; }
    for (const ZCollisionEdge &edge : scene->GetEdges()) {
        if (!edge.enabled) { continue; }
        const ZCollisionPoint &a = scene->GetVertices()[edge.firstVertex];
        const ZCollisionPoint &b = scene->GetVertices()[edge.secondVertex];
        const float ex = b.x - a.x, ey = b.y - a.y;
        const float cross = dx * ey - dy * ex;
        if (std::abs(cross) < 0.00001f) { continue; }
        const float t = ((a.x - x) * ey - (a.y - y) * ex) / cross;
        const float u = ((a.x - x) * dy - (a.y - y) * dx) / cross;
        if (t >= 0.0f && t <= fraction && u >= 0.0f && u <= 1.0f) {
            if (hit != nullptr) { *hit = true; }
            fraction = t;
            const float length = std::hypot(ex, ey);
            if (normalX != nullptr && normalY != nullptr && length > 0) {
                *normalX = -ey / length;
                *normalY = ex / length;
            }
        }
    }
    return fraction;
}

}
#endif
