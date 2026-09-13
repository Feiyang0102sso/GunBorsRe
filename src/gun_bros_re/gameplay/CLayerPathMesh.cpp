/** @file CLayerPathMesh.cpp
 * @brief Wire format and centre calculation from iOS :167381-167560.
 */
#include "gun_bros_re/gameplay/CLayerPathMesh.h"

void CLayerPathMesh::PropogateNodeLock(int boundary, int origin, bool locked) {
    if (origin < 0 || origin >= static_cast<int>(m_nodes.size())) { return; }
    // Original :167663 writes both endpoints before expanding from origin.
    // Retain its spelling; nodes already in the requested state stop the flood.
    SetNodeLocked(boundary, locked);
    SetNodeLocked(origin, locked);
    std::vector<int> pending{origin};
    while (!pending.empty()) {
        const int current = pending.back();
        pending.pop_back();
        for (unsigned neighbour : m_nodes[current].neighbours) {
            if (m_nodes[neighbour].locked == locked) { continue; }
            SetNodeLocked(static_cast<int>(neighbour), locked);
            pending.push_back(static_cast<int>(neighbour));
        }
    }
}

void CLayerPathMesh::UnlockNodesBetween(int first, int origin, int last) {
    // Original :167619 seeds three open nodes, then propagates from the middle.
    SetNodeLocked(last, false);
    PropogateNodeLock(first, origin, false);
}

void CLayerPathMesh::GetConnectionPoint(int from, int to, float &x, float &y) const {
    // CLayerPathMesh::GetSharedSide :167837 uses common vertex identities.
    unsigned shared[2] = {};
    unsigned count = 0;
    for (unsigned first : m_quads[from].vertices) {
        for (unsigned second : m_quads[to].vertices) {
            if (first == second && (count == 0 || shared[0] != first)) {
                shared[count++] = first;
                break;
            }
        }
        if (count == 2) { break; }
    }
    x = m_nodes[to].x;
    y = m_nodes[to].y;
    if (count == 2) {
        const float middleX = (m_vertices[shared[0]].x + m_vertices[shared[1]].x) * 0.5f;
        const float middleY = (m_vertices[shared[0]].y + m_vertices[shared[1]].y) * 0.5f;
        // Aim just inside the next cell so its containment test changes sides.
        x = middleX * 0.9f + x * 0.1f;
        y = middleY * 0.9f + y * 0.1f;
    }
}

int CLayerPathMesh::FindNode(float x, float y) const {
    for (unsigned index = 0; index < m_quads.size(); ++index) {
        if (m_nodes[index].locked) { continue; }
        bool positive = false, negative = false;
        const Quad &quad = m_quads[index];
        for (unsigned side = 0; side < 4; ++side) {
            const CollisionPoint &a = m_vertices[quad.vertices[side]];
            const CollisionPoint &b = m_vertices[quad.vertices[(side + 1) % 4]];
            const float cross = (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
            if (cross > 0.01f) { positive = true; }
            if (cross < -0.01f) { negative = true; }
        }
        if (!positive || !negative) { return static_cast<int>(index); }
    }
    return FindNearest(x, y);
}

bool CLayerPathMesh::Init(CArrayInputStream &stream) {
    InvalidateRoutes();
    const unsigned vertexCount = stream.ReadUInt16();
    const unsigned nodeCount = stream.ReadUInt16();
    const unsigned neighbourRefCount = stream.ReadUInt16();
    m_vertices.clear();
    m_quads.clear();
    m_nodes.clear();
    m_nodes.resize(nodeCount);
    for (unsigned index = 0; index < vertexCount; ++index) {
        CollisionPoint point;
        point.x = static_cast<float>(stream.ReadInt16());
        point.y = static_cast<float>(stream.ReadInt16());
        m_vertices.push_back(point);
    }
    unsigned refs = 0;
    for (Node &node : m_nodes) {
        Quad quad;
        quad.flags = stream.ReadUInt8();
        for (std::uint16_t &vertex : quad.vertices) {
            vertex = stream.ReadUInt16();
            if (vertex >= vertexCount) { return false; }
            node.x += m_vertices[vertex].x * 0.25f;
            node.y += m_vertices[vertex].y * 0.25f;
        }
        const unsigned count = stream.ReadUInt8();
        for (unsigned index = 0; index < count; ++index) {
            const unsigned neighbour = stream.ReadUInt16();
            if (neighbour >= nodeCount) { return false; }
            node.neighbours.push_back(neighbour);
            ++refs;
        }
        m_quads.push_back(quad);
    }
    return !stream.Overran() && refs == neighbourRefCount;
}
