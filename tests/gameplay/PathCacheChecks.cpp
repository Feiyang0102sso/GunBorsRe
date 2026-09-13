/** Regression for repeated route requests and live graph changes. */
#include "gameplay/SurvivalStudy.h"
#include "gameplay/PerformanceProbe.h"
#include "gun_bros_re/gameplay/CLayerPathLink.h"
#include "gun_bros_re/gameplay/CLayerPathMesh.h"
#include <cstdio>

namespace {
class BranchingPath : public CLayerPathMesh {
public:
    BranchingPath() {
        // Two equal routes, one isolated node; exercise deterministic tie order.
        m_nodes.resize(5);
        m_nodes[0].x = 0; m_nodes[0].y = 0;
        m_nodes[1].x = 1; m_nodes[1].y = 1;
        m_nodes[2].x = 1; m_nodes[2].y = -1;
        m_nodes[3].x = 2; m_nodes[3].y = 0;
        m_nodes[0].neighbours = {1, 2};
        m_nodes[1].neighbours = {0, 3};
        m_nodes[2].neighbours = {0, 3};
        m_nodes[3].neighbours = {1, 2};
    }
};
}

int RunPathCacheCheck() {
    unsigned failures = 0;
    BranchingPath path;
    PerformanceProbe::enabled = true;
    PerformanceProbe::counters = {};
    for (unsigned query = 0; query < 100; ++query) {
        if (path.FindNext(0, 3) != 1 || path.FindNext(0, 4) != -1) { ++failures; }
    }
    const unsigned hits = PerformanceProbe::counters.pathCacheHits;
    if (hits < 198) { ++failures; }
    path.SetNodeLocked(1, true);
    if (path.FindNext(0, 3) != 2) { ++failures; }
    path.SetNodeLocked(2, true);
    if (path.FindNext(0, 3) != -1) { ++failures; }
    path.SetAllNodesLocked(false);
    if (path.FindNext(0, 3) != 1) { ++failures; }
    path.PropogateNodeLock(4, 1, true);
    if (path.FindNext(0, 3) != -1) { ++failures; }
    path.UnlockNodesBetween(4, 1, 3);
    if (path.FindNext(0, 3) != 1) { ++failures; }
    if (path.FindNext(-1, 3) != -1 || path.FindNext(0, 5) != -1 || path.FindNext(2, 2) != 2) { ++failures; }

    // Same-size reload must discard a previous reachable-route result.
    // map.bt PathLinkLayer: counts, i16 x/y/radius records, u8 edge pairs.
    std::vector<std::uint8_t> linked = {2, 1, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 1};
    CLayerPathLink link;
    CArrayInputStream first(linked);
    if (!link.Init(first) || link.FindNext(0, 1) != 1) { ++failures; }
    linked[1] = 0;
    linked.resize(15);
    CArrayInputStream second(linked);
    if (!link.Init(second) || link.FindNext(0, 1) != -1) { ++failures; }
    PerformanceProbe::enabled = false;
    std::printf("[path-cache-check] repeated=200 hits=%u locks=flood/reopen reload=same-size failures=%u\n", hits, failures);
    return failures != 0;
}
