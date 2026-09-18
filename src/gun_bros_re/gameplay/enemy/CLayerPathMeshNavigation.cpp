/** @file CLayerPathMeshNavigation.cpp
 * Original: src/gunbros/layerPathMesh.cpp GetSharedSide :167837,
 * CalculateDistanceMapAtCell :167932, GetClosestConnection :168066.
 * Data: maps/map.bt PathMeshLayer; costs derive from original cell centres.
 */
#include "gun_bros_re/gameplay/CLayerPathMesh.h"
#include <cmath>
#include <functional>
#include <limits>
#include <queue>

bool CLayerPathMesh::GetSharedSide(int from, int to, ZCollisionPoint &a, ZCollisionPoint &b) const {
    if (from < 0 || to < 0 || from >= static_cast<int>(m_quads.size()) || to >= static_cast<int>(m_quads.size())) {
        return false;
    }
    unsigned count = 0;
    for (unsigned first : m_quads[from].vertices) {
        for (unsigned second : m_quads[to].vertices) {
            if (first != second) { continue; }
            if (count == 0) { a = m_vertices[first]; }
            else { b = m_vertices[first]; return true; }
            ++count;
            break;
        }
    }
    return false;
}

void CLayerPathMesh::CalculateDistanceMapAtCell(int destination, std::vector<float> &distances) const {
    distances.assign(m_nodes.size(), std::numeric_limits<float>::max());
    if (destination < 0 || destination >= static_cast<int>(m_nodes.size())) { return; }
    // Equivalent relaxation to WalkCell :167884, using a heap instead of native
    // recursion. The neighbour selector below retains authored tie order.
    using Pending = std::pair<float, unsigned>;
    std::priority_queue<Pending, std::vector<Pending>, std::greater<Pending>> pending;
    distances[destination] = 0;
    pending.emplace(0.0f, static_cast<unsigned>(destination));
    while (!pending.empty()) {
        const auto [distance, current] = pending.top();
        pending.pop();
        if (distance != distances[current]) { continue; }
        const Node &node = m_nodes[current];
        for (unsigned neighbour : node.neighbours) {
            const Node &next = m_nodes[neighbour];
            if (next.locked) { continue; }
            const float candidate = distance + std::hypot(next.x - node.x, next.y - node.y);
            if (candidate >= distances[neighbour]) { continue; }
            distances[neighbour] = candidate;
            pending.emplace(candidate, neighbour);
        }
    }
}

int CLayerPathMesh::GetClosestConnection(const std::vector<float> &distances, int from) const {
    if (from < 0 || from >= static_cast<int>(m_nodes.size()) || distances.size() != m_nodes.size()) { return -1; }
    int closest = -1;
    float best = std::numeric_limits<float>::max();
    for (unsigned neighbour : m_nodes[from].neighbours) {
        if (distances[neighbour] < best) {
            best = distances[neighbour];
            closest = static_cast<int>(neighbour);
        }
    }
    return closest;
}

float CLayerPathMesh::CastRay(float x, float y, float directionX, float directionY) const {
    int cell = FindNode(x, y);
    float distance = 0;
    // LineSegmentRayIntersection :65734 forms a second point, then subtracts
    // the origin. Preserve that float rounding at large map coordinates.
    const float rayX = (x + directionX) - x;
    const float rayY = (y + directionY) - y;
    while (cell >= 0) {
        const auto &quad = m_quads[cell];
        int connected = -1;
        for (unsigned side = 0; side < 4; ++side) {
            const unsigned first = quad.vertices[side];
            const unsigned second = quad.vertices[(side + 1) % 4];
            const auto &a = m_vertices[first];
            const auto &b = m_vertices[second];
            const float edgeX = b.x - a.x;
            const float edgeY = b.y - a.y;
            const float determinant = rayX * edgeY - rayY * edgeX;
            if (determinant == 0) { continue; }
            const float fromX = a.x - x;
            const float fromY = a.y - y;
            const float alongRay = (fromX * edgeY - fromY * edgeX) / determinant;
            const float alongEdge = (fromX * rayY - fromY * rayX) / determinant;
            if (alongRay <= distance || alongEdge < 0 || alongEdge > 1) { continue; }
            distance = alongRay;
            connected = -1;
            // GetConnectedCell :167987 searches authored neighbours in order,
            // matching either edge orientation. It does not filter their locks.
            for (unsigned neighbour : m_nodes[cell].neighbours) {
                const auto &other = m_quads[neighbour];
                for (unsigned otherSide = 0; otherSide < 4; ++otherSide) {
                    const unsigned otherFirst = other.vertices[otherSide];
                    const unsigned otherSecond = other.vertices[(otherSide + 1) % 4];
                    if ((first == otherFirst && second == otherSecond) ||
                        (first == otherSecond && second == otherFirst)) {
                        connected = static_cast<int>(neighbour);
                        break;
                    }
                }
                if (connected >= 0) { break; }
            }
        }
        cell = connected;
    }
    return distance;
}
