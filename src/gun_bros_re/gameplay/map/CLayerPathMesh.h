/** @file CLayerPathMesh.h
 * @brief Original quad mesh; graph nodes retain authored centres and adjacency.
 */
#ifndef GUN_BROS_RE_CLAYERPATHMESH_H
#define GUN_BROS_RE_CLAYERPATHMESH_H
#include "gun_bros_re/gameplay/map/ILayerPath.h"
#include "gun_bros_re/gameplay/collision/CCollisionData.h"
#include <array>

class CLayerPathMesh : public ILayerPath {
public:
    struct Quad {
        std::uint8_t flags = 0;
        std::array<std::uint16_t, 4> vertices{};
    };
    bool Init(CArrayInputStream &stream);
    int GetSpawnLocation(float sourceX, float sourceY, const COffscreenSpawnLocationFilter &filter, CRandGen &random) const override;
    int FindNode(float x, float y) const override;
    // Original layerPathMesh.cpp :167837, :167955, :168066.
    bool GetSharedSide(int from, int to, ZCollisionPoint &a, ZCollisionPoint &b) const;
    void CalculateDistanceMapAtCell(int destination, std::vector<float> &distances) const;
    int GetClosestConnection(const std::vector<float> &distances, int from) const;
    /** Original CastRay :168257: follow connected cell edges to the mesh boundary. */
    float CastRay(float x, float y, float directionX, float directionY) const;
    void PropogateNodeLock(int boundary, int origin, bool locked) override;
    void UnlockNodesBetween(int first, int origin, int last) override;
    void GetConnectionPoint(int from, int to, float &x, float &y) const override;
    const std::vector<ZCollisionPoint> &GetVertices() const { return m_vertices; }
    const std::vector<Quad> &GetQuads() const { return m_quads; }
private:
    std::vector<ZCollisionPoint> m_vertices;
    std::vector<Quad> m_quads;
};
#endif
