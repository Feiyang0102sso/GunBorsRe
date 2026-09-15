/** @file ILayerPath.cpp
 * @brief Graph queries shared by the original link and mesh layer types.
 */
#include "gun_bros_re/gameplay/ILayerPath.h"
#include <cmath>
#include <limits>
#if GB_ENABLE_TESTS
#include "gun_bros_re/debug/PerformanceProbe.h"
#endif

void ILayerPath::SetNodeLocked(int index, bool locked) {
    if (index >= 0 && index < static_cast<int>(m_nodes.size()) && m_nodes[index].locked != locked) {
        m_nodes[index].locked = locked;
        InvalidateRoutes();
    }
}

int ILayerPath::FindNearest(float x, float y) const {
    int result = -1;
    float best = std::numeric_limits<float>::max();
    for (unsigned index = 0; index < m_nodes.size(); ++index) {
        const Node &node = m_nodes[index];
        const float distance = std::hypot(node.x - x, node.y - y);
        if (!node.locked && distance < best) {
            result = static_cast<int>(index);
            best = distance;
        }
    }
    return result;
}

int ILayerPath::FindNext(int start, int destination) const {
#if GB_ENABLE_TESTS
    PerformanceProbe::Scope timing(PerformanceProbe::counters.pathSearchMs);
    if (PerformanceProbe::enabled) {
        ++PerformanceProbe::counters.pathSearches;
        PerformanceProbe::counters.pathNodes = static_cast<unsigned>(m_nodes.size());
    }
    // Development A/B replay uses the unchanged search as its reference.
    if (PerformanceProbe::uncachedPaths) { return FindNextUncached(start, destination); }
#endif
    const int count = static_cast<int>(m_nodes.size());
    if (start < 0 || start >= count || destination < 0 || destination >= count) { return -1; }
    if (start == destination) { return destination; }
    const auto key = std::make_pair(start, destination);
    const auto found = m_nextRoutes.find(key);
    if (found != m_nextRoutes.end()) {
#if GB_ENABLE_TESTS
        if (PerformanceProbe::enabled) { ++PerformanceProbe::counters.pathCacheHits; }
#endif
        return found->second;
    }
    const int result = FindNextUncached(start, destination);
    m_nextRoutes.emplace(key, result);
    return result;
}

int ILayerPath::FindNextUncached(int start, int destination) const {
    const int count = static_cast<int>(m_nodes.size());
    if (start < 0 || start >= count || destination < 0 || destination >= count) { return -1; }
    if (start == destination) { return destination; }
    std::vector<float> distances(count, std::numeric_limits<float>::max());
    std::vector<int> previous(count, -1);
    std::vector<bool> visited(count, false);
    distances[start] = 0;
    for (int step = 0; step < count; ++step) {
        int current = -1;
        float best = std::numeric_limits<float>::max();
        for (int index = 0; index < count; ++index) {
            if (!visited[index] && distances[index] < best) {
                current = index;
                best = distances[index];
            }
        }
        if (current < 0) { return -1; }
        if (current == destination) { break; }
        visited[current] = true;
        const Node &node = m_nodes[current];
        for (unsigned neighbour : node.neighbours) {
            const Node &next = m_nodes[neighbour];
            if (next.locked || visited[neighbour]) { continue; }
            const float distance = best + std::hypot(next.x - node.x, next.y - node.y);
            if (distance < distances[neighbour]) {
                distances[neighbour] = distance;
                previous[neighbour] = current;
            }
        }
    }
    int result = destination;
    while (previous[result] != start) {
        result = previous[result];
        if (result < 0) { return -1; }
    }
    return result;
}
