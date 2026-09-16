/** Windows Bot routing against the current BIG map/prop collision geometry.
 * Authored paths select tactical destinations; A* resolves passages around
 * active cover, which the original static navigation mesh does not include.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/ZCombatWorld.h"
#include "gun_bros_re/gameplay/ZCombatGeometry.h"
#include <queue>
#include <limits>
#include <cmath>
#include "gun_bros_re/debug/PerformanceProbe.h"

bool ZCombatWorld::FindMatchRoute(float x, float y, float goalX, float goalY, std::vector<ZCollisionPoint> &route) const {
    PerformanceProbe::Scope timing(PerformanceProbe::counters.pathSearchMs);
    route.clear();
    if (CanBrotherWalk(x, y, goalX, goalY)) { route.emplace_back(goalX, goalY); return true; }
    const float step = std::max(24.0f, m_playerRadius * 2);
    const int columns = static_cast<int>((m_right - m_left) / step) + 1;
    const int rows = static_cast<int>((m_bottom - m_top) / step) + 1;
    if (columns < 1 || rows < 1) { return false; }
    const auto point = [&](int index) { return ZCollisionPoint(m_left + index % columns * step, m_top + index / columns * step); };
    const auto attach = [&](float px, float py) {
        const int column = static_cast<int>(std::lround((px - m_left) / step));
        const int row = static_cast<int>(std::lround((py - m_top) / step));
        int chosen = -1;
        float nearest = std::numeric_limits<float>::max();
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                const int cx = column + dx, cy = row + dy;
                if (cx < 0 || cy < 0 || cx >= columns || cy >= rows) { continue; }
                const int index = cy * columns + cx;
                const auto node = point(index);
                const float distance = std::hypot(node.x - px, node.y - py);
                if (distance < nearest && CanBrotherWalk(px, py, node.x, node.y)) { chosen = index; nearest = distance; }
            }
        }
        return chosen;
    };
    const int first = attach(x, y), last = attach(goalX, goalY);
    if (first < 0 || last < 0) { return false; }
    // Index the current collision snapshot by grid cell. Scanning every map
    // edge for every A* neighbour caused 500 ms stalls on the larger arenas.
    // Rebuilding this small index per route also respects destroyed cover.
    std::vector<std::vector<unsigned>> nearbyEdges(columns * rows);
    if (m_collision != nullptr) {
        const auto &vertices = m_collision->GetVertices();
        const auto &edges = m_collision->GetEdges();
        const float margin = step + m_playerRadius;
        for (unsigned index = 0; index < edges.size(); ++index) {
            const auto &edge = edges[index];
            if (!edge.enabled) { continue; }
            const auto &a = vertices[edge.firstVertex], &b = vertices[edge.secondVertex];
            const int left = std::clamp(static_cast<int>(std::floor((std::min(a.x, b.x) - margin - m_left) / step)), 0, columns - 1);
            const int right = std::clamp(static_cast<int>(std::ceil((std::max(a.x, b.x) + margin - m_left) / step)), 0, columns - 1);
            const int top = std::clamp(static_cast<int>(std::floor((std::min(a.y, b.y) - margin - m_top) / step)), 0, rows - 1);
            const int bottom = std::clamp(static_cast<int>(std::ceil((std::max(a.y, b.y) + margin - m_top) / step)), 0, rows - 1);
            for (int row = top; row <= bottom; ++row) {
                for (int column = left; column <= right; ++column) { nearbyEdges[row * columns + column].push_back(index); }
            }
        }
    }
    const auto clearEdge = [&](int current, const ZCollisionPoint &origin, const ZCollisionPoint &target) {
        for (unsigned index : nearbyEdges[current]) {
            const auto &edge = m_collision->GetEdges()[index];
            const auto &vertices = m_collision->GetVertices();
            if (CombatGeometry::EdgeFraction(origin.x, origin.y, target.x - origin.x, target.y - origin.y,
                vertices[edge.firstVertex], vertices[edge.secondVertex], m_playerRadius) < 1) { return false; }
        }
        return true;
    };
    struct OpenNode {
        float score; int index;
        bool operator<(const OpenNode &other) const { return score > other.score; }
    };
    std::priority_queue<OpenNode> open;
    std::vector<float> costs(columns * rows, std::numeric_limits<float>::max());
    std::vector<int> previous(columns * rows, -1);
    std::vector<bool> closed(columns * rows, false);
    costs[first] = 0; open.push({0, first});
    while (!open.empty()) {
        const int current = open.top().index;
        open.pop();
        if (closed[current]) { continue; }
        if (current == last) { break; }
        closed[current] = true;
        const auto origin = point(current);
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) { continue; }
                const int column = current % columns + dx, row = current / columns + dy;
                if (column < 0 || row < 0 || column >= columns || row >= rows) { continue; }
                const int next = row * columns + column;
                if (closed[next]) { continue; }
                const auto target = point(next);
                const float cost = costs[current] + std::hypot(target.x - origin.x, target.y - origin.y);
                // Grid edges require clearance, not the costly wall-sliding
                // recovery used when attaching an actor already touching cover.
                if (cost >= costs[next] || !clearEdge(current, origin, target)) { continue; }
                costs[next] = cost; previous[next] = current;
                open.push({cost + std::hypot(target.x - goalX, target.y - goalY), next});
            }
        }
    }
    if (first != last && previous[last] < 0) { return false; }
    route.emplace_back(goalX, goalY);
    for (int index = last; index != first; index = previous[index]) { route.push_back(point(index)); }
    route.push_back(point(first));
    std::reverse(route.begin(), route.end());
    return true;
}
