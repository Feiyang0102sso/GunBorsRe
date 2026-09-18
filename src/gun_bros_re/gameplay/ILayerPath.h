/** @file ILayerPath.h
 * @brief Common path-layer query surface; coordinates are world pixels.
 * The two wire formats belong to CLayerPathLink and CLayerPathMesh.
 */
#ifndef GUN_BROS_RE_ILAYERPATH_H
#define GUN_BROS_RE_ILAYERPATH_H
#include "engine/resources/CArrayInputStream.h"
#include "gun_bros_re/gameplay/CLayerCamera.h"
#include <vector>
#include <map>
#include <utility>

class ZRandom;

#include "gun_bros_re/gameplay/enemy/COffscreenSpawnLocationFilter.h"

class ILayerPath {
public:
    virtual ~ILayerPath() = default;
    /** Native path layers own their distinct spawn search algorithms. */
    virtual int GetSpawnLocation(float sourceX, float sourceY, const COffscreenSpawnLocationFilter &filter, ZRandom &random) const { return -1; }
    struct Node {
        float x = 0;
        float y = 0;
        float radius = 0;
        bool locked = false;
        std::vector<unsigned> neighbours;
    };
    void SetLayerIndex(unsigned index) { m_layerIndex = index; }
    unsigned GetLayerIndex() const { return m_layerIndex; }
    const std::vector<Node> &GetNodes() const { return m_nodes; }
    std::uint64_t GetRevision() const { return m_revision; }
    void SetNodeLocked(int index, bool locked);
    virtual void PropogateNodeLock(int boundary, int origin, bool locked) {}
    virtual void UnlockNodesBetween(int first, int origin, int last) {}
    void SetAllNodesLocked(bool locked) {
        for (unsigned index = 0; index < m_nodes.size(); ++index) { SetNodeLocked(index, locked); }
    }
    /** Return nearest unlocked node, or -1 for an empty graph. */
    int FindNearest(float x, float y) const;
    /** Meshes select the containing polygon; links use their nearest node. */
    virtual int FindNode(float x, float y) const { return FindNearest(x, y); }
    virtual void GetConnectionPoint(int from, int to, float &x, float &y) const {
        x = m_nodes[to].x;
        y = m_nodes[to].y;
    }
    /** Dijkstra over original links; next node from start towards destination. */
    int FindNext(int start, int destination) const;
protected:
    /** Runtime query results depend only on topology and node locks. */
    void InvalidateRoutes() { m_nextRoutes.clear(); ++m_revision; }
    unsigned m_layerIndex = 0;
    std::vector<Node> m_nodes;
private:
    std::uint64_t m_revision = 0;
    int FindNextUncached(int start, int destination) const;
    // Host query cache: retain the existing Dijkstra tie order exactly.
    // iOS CFlock::RefreshDistanceMaps :170441 and CalculateDistanceMap :167955
    // also reuse navigation work; this is not a copy of their distance-map layout.
    mutable std::map<std::pair<int, int>, int> m_nextRoutes;
};
#endif
