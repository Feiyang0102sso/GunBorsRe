/**
 * @file CCollisionData.h
 * @brief Map collision vertices, edges, and circle movement resolution.
 *
 * Port of CCollisionData (src/gunbros/collisionData.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:142444 (Load).
 */

#ifndef GUN_BROS_RE_GUN_BROS_CCOLLISIONDATA_H
#define GUN_BROS_RE_GUN_BROS_CCOLLISIONDATA_H

#include "engine/CArrayInputStream.h"

#include <cstdint>
#include <vector>

/** One point in map world coordinates. */
struct CollisionPoint {
    float x;
    float y;

    CollisionPoint() : x(0.0f), y(0.0f) {}
    CollisionPoint(float pointX, float pointY) : x(pointX), y(pointY) {}
};

/** One collision edge joining two points. */
struct CollisionEdge {
    std::uint8_t group;
    std::uint16_t firstVertex;
    std::uint16_t secondVertex;
    bool enabled;
};

/**
 * Collision geometry stored by a map layer.
 *
 * The interface deliberately exposes the source geometry for the debug draw,
 * while movement callers only need ResolveCircleMovement. All subdivision and
 * penetration correction stays inside this module.
 */
class CCollisionData {
public:
    /** Read the exact vertex-and-edge layout used by the original game. */
    bool Load(CArrayInputStream &stream);

    /** Remove all geometry before assembling a new collision scene. */
    void Clear();
    /** Original Enable/DisableCollision changes only the named edge group. */
    void SetGroupEnabled(int group, bool enabled);

    /**
     * Add another collision shape at a world-space offset.
     *
     * Map collision is already in world space and uses a zero offset. Prop
     * templates store their collision around the prop origin, so placed props
     * use their map position here. This is the same translation CProp::Bind
     * applies in the original game.
     */
    bool AppendTranslated(const CCollisionData &source, float offsetX,
                          float offsetY);

    /**
     * Move a circle through the collision edges.
     *
     * Like CPlayer::Move (:100686), the requested movement is split into five
     * short advances and each advance gets at most three penetration fixes.
     * This prevents ordinary frame movement from tunnelling through a wall and
     * naturally leaves the unblocked part of a diagonal movement as wall slide.
     */
    CollisionPoint ResolveCircleMovement(const CollisionPoint &start,
                                         const CollisionPoint &movement,
                                         float radius) const;

    const std::vector<CollisionPoint> &GetVertices() const { return m_vertices; }
    const std::vector<CollisionEdge> &GetEdges() const { return m_edges; }

private:
    bool ResolveNearestPenetration(const CollisionPoint &previous,
                                   CollisionPoint &position,
                                   float radius) const;

    std::vector<CollisionPoint> m_vertices;
    std::vector<CollisionEdge> m_edges;
};

#endif  // GUN_BROS_RE_GUN_BROS_CCOLLISIONDATA_H
