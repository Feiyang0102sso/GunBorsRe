/**
 * @file CCollisionData.cpp
 * @brief Map collision parsing and circle movement resolution.
 */

#include "gun_bros_re/gameplay/CCollisionData.h"

void CCollisionData::SetGroupEnabled(int group, bool enabled) {
    for (CollisionEdge &edge : m_edges) {
        if (edge.group == group) { edge.enabled = enabled; }
    }
}

#include <cmath>
#include <cstdio>
#include <limits>

namespace {

constexpr int kMovementSubsteps = 5;
constexpr int kMaximumCorrectionsPerSubstep = 3;
constexpr float kMinimumLengthSquared = 0.000001f;

float Clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

}  // namespace

bool CCollisionData::Load(CArrayInputStream &stream) {
    Clear();

    const std::uint16_t vertexCount = stream.ReadUInt16();
    m_vertices.reserve(vertexCount);
    for (std::uint16_t i = 0; i < vertexCount; ++i) {
        const float x = static_cast<float>(stream.ReadInt32());
        const float y = static_cast<float>(stream.ReadInt32());
        m_vertices.push_back(CollisionPoint(x, y));
    }

    const std::uint16_t edgeCount = stream.ReadUInt16();
    m_edges.reserve(edgeCount);
    for (std::uint16_t i = 0; i < edgeCount; ++i) {
        CollisionEdge edge;
        edge.group = stream.ReadUInt8();
        edge.firstVertex = stream.ReadUInt16();
        edge.secondVertex = stream.ReadUInt16();
        edge.enabled = true;
        m_edges.push_back(edge);
    }

    if (stream.Overran()) {
        std::printf("[collision] layer truncated\n");
        m_vertices.clear();
        m_edges.clear();
        return false;
    }

    for (std::size_t i = 0; i < m_edges.size(); ++i) {
        const CollisionEdge &edge = m_edges[i];
        if (edge.firstVertex >= m_vertices.size() ||
            edge.secondVertex >= m_vertices.size()) {
            std::printf("[collision] edge %zu names vertex %u -> %u; only %zu exist\n",
                        i, edge.firstVertex, edge.secondVertex, m_vertices.size());
            m_vertices.clear();
            m_edges.clear();
            return false;
        }
    }

    return true;
}

void CCollisionData::Clear() {
    m_vertices.clear();
    m_edges.clear();
}

bool CCollisionData::AppendTranslated(const CCollisionData &source,
                                      float offsetX, float offsetY) {
    const std::size_t firstVertex = m_vertices.size();
    const std::size_t combinedVertexCount = firstVertex + source.m_vertices.size();
    if (combinedVertexCount >
        static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1) {
        std::printf("[collision] assembled scene has too many vertices\n");
        return false;
    }

    m_vertices.reserve(combinedVertexCount);
    for (std::size_t i = 0; i < source.m_vertices.size(); ++i) {
        const CollisionPoint &vertex = source.m_vertices[i];
        m_vertices.push_back(
            CollisionPoint(vertex.x + offsetX, vertex.y + offsetY));
    }

    m_edges.reserve(m_edges.size() + source.m_edges.size());
    for (std::size_t i = 0; i < source.m_edges.size(); ++i) {
        const CollisionEdge &sourceEdge = source.m_edges[i];
        CollisionEdge edge = sourceEdge;
        edge.firstVertex = static_cast<std::uint16_t>(
            firstVertex + sourceEdge.firstVertex);
        edge.secondVertex = static_cast<std::uint16_t>(
            firstVertex + sourceEdge.secondVertex);
        m_edges.push_back(edge);
    }

    return true;
}

CollisionPoint CCollisionData::ResolveCircleMovement(
    const CollisionPoint &start, const CollisionPoint &movement,
    float radius) const {
    CollisionPoint position = start;
    if (radius <= 0.0f || m_edges.empty()) {
        position.x += movement.x;
        position.y += movement.y;
        return position;
    }

    const float stepX = movement.x / static_cast<float>(kMovementSubsteps);
    const float stepY = movement.y / static_cast<float>(kMovementSubsteps);

    for (int step = 0; step < kMovementSubsteps; ++step) {
        const CollisionPoint previous = position;
        position.x += stepX;
        position.y += stepY;

        for (int correction = 0;
             correction < kMaximumCorrectionsPerSubstep; ++correction) {
            if (!ResolveNearestPenetration(previous, position, radius)) {
                break;
            }
        }
    }

    return position;
}

bool CCollisionData::ResolveNearestPenetration(
    const CollisionPoint &previous, CollisionPoint &position,
    float radius) const {
    float nearestDistanceSquared = std::numeric_limits<float>::max();
    float nearestX = 0.0f;
    float nearestY = 0.0f;
    const CollisionEdge *nearestEdge = nullptr;

    for (std::size_t i = 0; i < m_edges.size(); ++i) {
        const CollisionEdge &edge = m_edges[i];
        if (!edge.enabled) {
            continue;
        }

        const CollisionPoint &first = m_vertices[edge.firstVertex];
        const CollisionPoint &second = m_vertices[edge.secondVertex];
        const float edgeX = second.x - first.x;
        const float edgeY = second.y - first.y;
        const float edgeLengthSquared = edgeX * edgeX + edgeY * edgeY;
        if (edgeLengthSquared <= kMinimumLengthSquared) {
            continue;
        }

        const float fromFirstX = position.x - first.x;
        const float fromFirstY = position.y - first.y;
        float along = (fromFirstX * edgeX + fromFirstY * edgeY) /
                      edgeLengthSquared;
        along = Clamp01(along);

        const float pointX = first.x + edgeX * along;
        const float pointY = first.y + edgeY * along;
        const float separationX = position.x - pointX;
        const float separationY = position.y - pointY;
        const float distanceSquared = separationX * separationX +
                                      separationY * separationY;
        if (distanceSquared >= radius * radius ||
            distanceSquared >= nearestDistanceSquared) {
            continue;
        }

        nearestDistanceSquared = distanceSquared;
        nearestX = pointX;
        nearestY = pointY;
        nearestEdge = &edge;
    }

    if (nearestEdge == nullptr) {
        return false;
    }

    float normalX = position.x - nearestX;
    float normalY = position.y - nearestY;
    float distance = std::sqrt(nearestDistanceSquared);

    if (distance <= kMinimumLengthSquared) {
        const CollisionPoint &first = m_vertices[nearestEdge->firstVertex];
        const CollisionPoint &second = m_vertices[nearestEdge->secondVertex];
        normalX = -(second.y - first.y);
        normalY = second.x - first.x;
        distance = std::sqrt(normalX * normalX + normalY * normalY);
        if (distance <= kMinimumLengthSquared) {
            return false;
        }

        // Keep the circle on the side it occupied before this substep. If it
        // started exactly on the line, oppose the attempted movement.
        const float previousSide = (previous.x - first.x) * normalX +
                                   (previous.y - first.y) * normalY;
        const float movementSide = (position.x - previous.x) * normalX +
                                   (position.y - previous.y) * normalY;
        if (previousSide < 0.0f ||
            (previousSide == 0.0f && movementSide > 0.0f)) {
            normalX = -normalX;
            normalY = -normalY;
        }
    }

    normalX /= distance;
    normalY /= distance;
    const float penetration = radius - std::sqrt(nearestDistanceSquared);
    position.x += normalX * penetration;
    position.y += normalY * penetration;
    return true;
}
