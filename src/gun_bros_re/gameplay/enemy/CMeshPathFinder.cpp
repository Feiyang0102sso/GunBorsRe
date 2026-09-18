/** @file CMeshPathFinder.cpp
 * Original: src/gunbros/meshPathFinder.cpp CalculateDestination :168399;
 * ARM 0xe7754-0xe7978 verifies the signed epsilon checks and one-unit offset.
 */
#include "gun_bros_re/gameplay/enemy/CMeshPathFinder.h"
#include "gun_bros_re/gameplay/map/CLayerPathMesh.h"
#include <algorithm>
#include <cmath>

void CMeshPathFinder::Update(const CLayerPathMesh &path, const std::vector<float> &distances,
    float x, float y, float targetX, float targetY) {
    const int current = path.FindNode(x, y);
    const int target = path.FindNode(targetX, targetY);
    if (current == target || current < 0) {
        m_x = targetX;
        m_y = targetY;
        return;
    }
    // CanMoveDirect (ARM 0xe71a0-0xe71d4) tests two zero edge endpoints,
    // without calling GetSharedSide. That degenerate segment never intersects.
    // Preserve this verified iOS behaviour instead of inventing a visibility test.
    const int next = path.GetClosestConnection(distances, current);
    if (next < 0) {
        m_x = path.GetNodes()[current].x;
        m_y = path.GetNodes()[current].y;
        return;
    }
    ZCollisionPoint a, b;
    if (!path.GetSharedSide(current, next, a, b)) { return; }
    // Collision::ClosestPoint :65802 projects onto the authored shared edge.
    const float edgeX = b.x - a.x;
    const float edgeY = b.y - a.y;
    const float edgeSquared = edgeX * edgeX + edgeY * edgeY;
    float fraction = 0;
    if (edgeSquared > 0) {
        fraction = std::clamp(((x - a.x) * edgeX + (y - a.y) * edgeY) / edgeSquared, 0.0f, 1.0f);
    }
    m_x = a.x + edgeX * fraction;
    m_y = a.y + edgeY * fraction;
    float dx = m_x - x;
    float dy = m_y - y;
    // These comparisons are signed, not absolute-value comparisons in the binary.
    if (dx <= 0.000001f || dy <= 0.000001f) {
        dx = path.GetNodes()[next].x - m_x;
        dy = path.GetNodes()[next].y - m_y;
    }
    const float length = std::hypot(dx, dy);
    if (length > 0) {
        m_x += dx / length;
        m_y += dy / length;
    }
}
