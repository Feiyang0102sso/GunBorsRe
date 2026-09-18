/** Original: src/gunbros/linkPathFinder.cpp Init :168567, Update :168635.
 * Windows graphics/resource storage is adapted; original data comes from BIG.
 */
#pragma once
/** @file CLinkPathFinder.h
 * @brief Follow authored links, with the original patrol/once modes.
 * Reference: CLinkPathFinder::Init/SetMode/Update, iOS :168567-168752.
 */
#include "gun_bros_re/gameplay/ILayerPath.h"

class CLinkPathFinder {
public:
    void Init(const ILayerPath *path, float x, float y);
    void SetMode(bool once, float x, float y);
    void Update(float x, float y);
    const ILayerPath::Node *GetDestination() const;
    bool IsDone() const { return m_once && m_done; }
private:
    const ILayerPath *m_path = nullptr;
    int m_previous = -1;
    int m_current = -1;
    bool m_once = false;
    bool m_done = false;
    std::vector<bool> m_visited;
};
