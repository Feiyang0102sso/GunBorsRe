#pragma once
#include "gun_bros_re/gameplay/brother/ZPlayerModel.h"
#include "gun_bros_re/gameplay/CCollisionData.h"
#include <cmath>
namespace ProjectileGeometry {
/** Project the same animated muzzle transform used to draw the weapon. */
inline bool ProjectMuzzle(ZPlayerModel &player, const float *matrix, int hand, int node,
                   float &x, float &y, float &z) {
    ZMeshBoneTransform muzzle{};
    if (!GetPlayerMuzzle(player, hand, node, muzzle)) { return false; }
    x = matrix[0] * muzzle.posX + matrix[1] * muzzle.posY + matrix[2] * muzzle.posZ + matrix[3];
    y = matrix[4] * muzzle.posX + matrix[5] * muzzle.posY + matrix[6] * muzzle.posZ + matrix[7];
    z = matrix[8] * muzzle.posX + matrix[9] * muzzle.posY + matrix[10] * muzzle.posZ + matrix[11];
    return true;
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
