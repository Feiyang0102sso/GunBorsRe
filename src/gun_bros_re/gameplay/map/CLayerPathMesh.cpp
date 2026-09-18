/**
 * CLayerPathMesh: the quad navigation mesh.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:167381
 *
 *   uint16 vertexCount, uint16 nodeCount, uint16 neighbourRefCount
 *   vertices: { int16 x, int16 y }
 *   nodes:    uint8 flags, int16 vertexIndices[4],
 *             uint8 neighbourCount, uint16 neighbours[neighbourCount]
 *
 * The third header count is the sum of every node's neighbour count, so it is
 * redundant -- the nodes are walked rather than trusted.
 */
/** @file CLayerPathMesh.cpp
 * @brief Wire format and centre calculation from iOS :167381-167560.
 */
#include "engine/core/ZRandom.h"
#include "gun_bros_re/gameplay/map/CLayerPathMesh.h"
#include <limits>

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
    // GetCellForLocation :167744 uses ray crossings, including concave quads.
    // Outside the mesh it returns the nearest unlocked centre by squared distance.
    int nearest = -1;
    float best = std::numeric_limits<float>::max();
    for (unsigned index = 0; index < m_quads.size(); ++index) {
        if (m_nodes[index].locked) { continue; }
        bool inside = false;
        const Quad &quad = m_quads[index];
        unsigned previous = 3;
        for (unsigned current = 0; current < 4; ++current) {
            const auto &a = m_vertices[quad.vertices[current]];
            const auto &b = m_vertices[quad.vertices[previous]];
            if ((a.y <= y && y < b.y) || (b.y <= y && y < a.y)) {
                const float crossing = a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y);
                if (x < crossing) { inside = !inside; }
            }
            previous = current;
        }
        if (inside) { return static_cast<int>(index); }
        const float dx = m_nodes[index].x - x;
        const float dy = m_nodes[index].y - y;
        const float squared = dx * dx + dy * dy;
        if (squared < best) {
            nearest = static_cast<int>(index);
            best = squared;
        }
    }
    return nearest;
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
        ZCollisionPoint point;
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

int CLayerPathMesh::GetSpawnLocation(float, float, const COffscreenSpawnLocationFilter &filter, ZRandom &random) const {
    const auto &nodes = m_nodes;
    if (nodes.empty()) { return -1; }
    // CLayerPathMesh::GetSpawnLocation :168115 starts at a random polygon
    // and scans cyclically for the first unlocked, offscreen centre.
    // Link layers below instead choose among the five nearest nodes.
    const unsigned start = static_cast<unsigned>(random.Integer(0, static_cast<std::int16_t>(nodes.size())));
    for (unsigned offset = 0; offset < nodes.size(); ++offset) {
        const std::size_t index = (start + offset) % nodes.size();
        const auto &node = nodes[index];
        if (node.locked) { continue; }
        if (!filter.AcceptSpawnLocation(node.x, node.y)) { continue; }
        return static_cast<int>(index);
    }
    return -1;
}
