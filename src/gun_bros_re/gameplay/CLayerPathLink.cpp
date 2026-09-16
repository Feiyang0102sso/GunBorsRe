/** @file CLayerPathLink.cpp
 * @brief Read the original graph; use a small deterministic shortest-path search.
 */
#include "engine/core/ZRandom.h"
#include "gun_bros_re/gameplay/CLayerPathLink.h"
#include <cmath>
#include <limits>

bool CLayerPathLink::Init(CArrayInputStream &stream) {
    InvalidateRoutes();
    const unsigned nodeCount = stream.ReadUInt8();
    const unsigned linkCount = stream.ReadUInt8();
    const unsigned regionCount = stream.ReadUInt8();
    m_nodes.clear();
    m_nodes.resize(nodeCount);
    m_regions.clear();
    for (Node &node : m_nodes) {
        node.x = static_cast<float>(stream.ReadInt16());
        node.y = static_cast<float>(stream.ReadInt16());
        node.radius = static_cast<float>(stream.ReadInt16());
    }
    for (unsigned index = 0; index < linkCount; ++index) {
        const unsigned first = stream.ReadUInt8();
        const unsigned second = stream.ReadUInt8();
        if (first >= nodeCount || second >= nodeCount) { return false; }
        m_nodes[first].neighbours.push_back(second);
        m_nodes[second].neighbours.push_back(first);
    }
    for (unsigned index = 0; index < regionCount; ++index) {
        Region region;
        const unsigned count = stream.ReadUInt8();
        for (unsigned item = 0; item < count; ++item) {
            const std::uint8_t node = stream.ReadUInt8();
            if (node >= nodeCount) { return false; }
            region.nodes.push_back(node);
        }
        region.bounds.x = stream.ReadInt16();
        region.bounds.y = stream.ReadInt16();
        region.bounds.width = stream.ReadInt16();
        region.bounds.height = stream.ReadInt16();
        m_regions.push_back(region);
    }
    return !stream.Overran();
}

// CLayerPathLink::GetSpawnLocation :166819; DistanceList :167261.
int CLayerPathLink::GetSpawnLocation(float sourceX, float sourceY, const ZSpawnFilter &filter, ZRandom &random) const {
    const auto &nodes = m_nodes;
    // {node index, squared distance to the player}, nearest first.
    std::vector<std::pair<int, float>> nearest;
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const ILayerPath::Node &node = nodes[index];
        if (node.locked) { continue; }
        const bool onScreen = !filter.Accepts(node.x, node.y);
        if (onScreen) { continue; }
        const float dx = sourceX - node.x;
        const float dy = sourceY - node.y;
        const float distance = dx * dx + dy * dy;
        std::size_t position = 0;
        while (position < nearest.size() && nearest[position].second <= distance) { ++position; }
        if (position >= 5) { continue; }
        nearest.insert(nearest.begin() + position, {static_cast<int>(index), distance});
        if (nearest.size() > 5) { nearest.pop_back(); }
    }
    if (nearest.empty()) { return -1; }
    const int pick = random.Integer(0, static_cast<std::int16_t>(nearest.size() - 1));
    return nearest[pick].first;
}
