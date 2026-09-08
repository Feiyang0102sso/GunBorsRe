/** @file CLayerPathLink.cpp
 * @brief Read the original graph; use a small deterministic shortest-path search.
 */
#include "gun_bros/CLayerPathLink.h"
#include <cmath>
#include <limits>

bool CLayerPathLink::Init(CArrayInputStream &stream) {
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

