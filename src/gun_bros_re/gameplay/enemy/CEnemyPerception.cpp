/** @file CEnemyPerception.cpp
 * Original: src/gunbros/enemy.cpp native 23 :72152;
 * layerCollision.cpp TestCollisionSegment :125270, collision.cpp :65660.
 * The host's terrain set contains the selected map layer plus active prop bullet
 * edges, rebuilt from BIG collision_data.bt shapes whenever props change.
 */
#include "gun_bros_re/gameplay/level/CLevel.h"

bool CLevel::TestEnemyLineOfSight(float x, float y, float targetX, float targetY) const {
    if (m_weaponCollision == nullptr) { return true; }
    const auto &collision = m_weaponCollision->terrain;
    const auto &vertices = collision.GetVertices();
    const float dx = targetX - x;
    const float dy = targetY - y;
    for (const auto &edge : collision.GetEdges()) {
        if (!edge.enabled) { continue; }
        const auto &a = vertices[edge.firstVertex];
        const auto &b = vertices[edge.secondVertex];
        const float edgeX = b.x - a.x;
        const float edgeY = b.y - a.y;
        const float determinant = dx * edgeY - dy * edgeX;
        // Original LineIntersection rejects exactly parallel/collinear lines.
        if (determinant == 0) { continue; }
        const float fromX = a.x - x;
        const float fromY = a.y - y;
        const float alongSight = (fromX * edgeY - fromY * edgeX) / determinant;
        const float alongEdge = (fromX * dy - fromY * dx) / determinant;
        if (alongSight >= 0 && alongSight <= 1 && alongEdge >= 0 && alongEdge <= 1) { return false; }
    }
    return true;
}
