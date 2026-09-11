/** @file ILayerPath.h
 * @brief Common path-layer query surface; coordinates are world pixels.
 * The two wire formats belong to CLayerPathLink and CLayerPathMesh.
 */
#ifndef GUN_BROS_RE_ILAYERPATH_H
#define GUN_BROS_RE_ILAYERPATH_H
#include "engine/resources/CArrayInputStream.h"
#include "gun_bros_re/gameplay/CLayerCamera.h"
#include <vector>

class ILayerPath {
public:
    virtual ~ILayerPath() = default;
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
    void SetNodeLocked(int index, bool locked);
    virtual void PropogateNodeLock(int boundary, int origin, bool locked) {}
    virtual void UnlockNodesBetween(int first, int origin, int last) {}
    void SetAllNodesLocked(bool locked) {
        for (Node &node : m_nodes) { node.locked = locked; }
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
    unsigned m_layerIndex = 0;
    std::vector<Node> m_nodes;

};
#endif
