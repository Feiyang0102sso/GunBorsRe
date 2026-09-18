/** @file CMeshPathFinder.h
 * Original: src/gunbros/meshPathFinder.cpp CalculateDestination :168399,
 * Update :168519. The host passes the actor/target positions explicitly.
 */
#pragma once
#include <vector>
class CLayerPathMesh;

class CMeshPathFinder {
public:
    void Update(const CLayerPathMesh &path, const std::vector<float> &distances,
        float x, float y, float targetX, float targetY);
    void GetDestination(float &x, float &y) const { x = m_x; y = m_y; }
private:
    float m_x = 0;
    float m_y = 0;
};
