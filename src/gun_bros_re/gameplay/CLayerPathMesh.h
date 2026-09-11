/** @file CLayerPathMesh.h
 * @brief Original quad mesh; graph nodes retain authored centres and adjacency.
 */
#ifndef GUN_BROS_RE_CLAYERPATHMESH_H
#define GUN_BROS_RE_CLAYERPATHMESH_H
#include "gun_bros_re/gameplay/ILayerPath.h"
#include "gun_bros_re/gameplay/CCollisionData.h"
#include <array>

class CLayerPathMesh : public ILayerPath {
public:
    struct Quad {
        std::uint8_t flags = 0;
        std::array<std::uint16_t, 4> vertices{};
    };
    bool Init(CArrayInputStream &stream);
    int FindNode(float x, float y) const override;
    void PropogateNodeLock(int boundary, int origin, bool locked) override;
    void UnlockNodesBetween(int first, int origin, int last) override;
    void GetConnectionPoint(int from, int to, float &x, float &y) const override;
    const std::vector<CollisionPoint> &GetVertices() const { return m_vertices; }
    const std::vector<Quad> &GetQuads() const { return m_quads; }
private:
    std::vector<CollisionPoint> m_vertices;
    std::vector<Quad> m_quads;
};
#endif
