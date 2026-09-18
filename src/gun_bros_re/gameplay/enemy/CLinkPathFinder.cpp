/** Original: src/gunbros/linkPathFinder.cpp Init :168567, Update :168635.
 * Windows graphics/resource storage is adapted; original data comes from BIG.
 */
#include "gun_bros_re/gameplay/enemy/CLinkPathFinder.h"

void CLinkPathFinder::Init(const ILayerPath *path, float x, float y) {
    m_path = path;
    m_current = -1;
    if (path != nullptr) { m_current = path->FindNearest(x, y); }
    m_previous = m_current;
    m_once = false;
    m_done = false;
    m_visited.clear();
    if (path != nullptr) { m_visited.resize(path->GetNodes().size(), false); }
}

const ILayerPath::Node *CLinkPathFinder::GetDestination() const {
    if (m_path == nullptr || m_current < 0) { return nullptr; }
    return &m_path->GetNodes()[m_current];
}

void CLinkPathFinder::SetMode(bool once, float x, float y) {
    m_once = once;
    const auto *node = GetDestination();
    if (once && node != nullptr && node->x == x && node->y == y) { m_visited[m_current] = true; }
}

void CLinkPathFinder::Update(float x, float y) {
    const auto *node = GetDestination();
    if (m_done || node == nullptr || node->x != x || node->y != y) { return; }
    const int previous = m_previous;
    const int current = m_current;
    m_previous = current;
    if (node->neighbours.size() == 1) {
        const unsigned next = node->neighbours.front();
        if (m_once && m_visited[next]) { m_done = true; return; }
        if (m_once) { m_visited[next] = true; }
        m_current = static_cast<int>(next);
        return;
    }
    // Original link order determines branch choice; do not invent a route.
    for (unsigned next : node->neighbours) {
        if (static_cast<int>(next) == current || static_cast<int>(next) == previous) { continue; }
        if (m_once && m_visited[next]) { continue; }
        m_visited[next] = true;
        m_current = static_cast<int>(next);
        break;
    }
    if (m_once && m_current == current) { m_done = true; }
}
