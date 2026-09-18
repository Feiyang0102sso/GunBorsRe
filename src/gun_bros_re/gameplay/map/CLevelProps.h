#pragma once
/** Map-owned prop lifetime and script messages; no graphics in CLevel. */
/** Connects placed CProp instances to level events and combat queries. */
#include "gun_bros_re/gameplay/map/CMapResources.h"
#include "gun_bros_re/gameplay/level/CLevel.h"

/** Original script state lives beside the map instance, never in shared quads. */
class CLevel::Props {
public:
    Props(CMap &map, CLevel &scene, CLevel &level)
        : m_map(map), m_scene(scene), m_level(level) {}

    void Reset() {
        m_activeProps.clear();
        for (CProp &prop : m_map.GetResources().props) { prop.active = false; }
        m_map.BuildCollisionScene();
    }

    void StartLayer(int layer) ;

    bool Spawn(int layer, int objectId) ;

    void SendMessage(int objectId, int message) ;

    unsigned ResolveIndicatorTarget(int objectId) const {
        for (unsigned index = 0; index < m_activeProps.size(); ++index) {
            const auto &prop = *m_activeProps[index];
            if (prop.objectId == objectId && prop.active && (!prop.HasScript() || !prop.IsRemoved())) { return index + 1; }
        }
        return 0;
    }

    bool GetObjectPosition(int objectId, float &x, float &y) const {
        const unsigned key = ResolveIndicatorTarget(objectId);
        if (key == 0) { return false; }
        x = m_activeProps[key - 1]->x;
        y = m_activeProps[key - 1]->y;
        return true;
    }

    bool GetIndicatorTarget(unsigned key, float &x, float &y) const ;
    bool IsActivePortal(int objectId) const {
        const unsigned key = ResolveIndicatorTarget(objectId);
        if (key == 0) { return false; }
        const auto &prop = *m_activeProps[key - 1];
        return prop.HasScript() && prop.IsActivePortal();
    }

    void Update(int deltaMs) ;

    ZCombatTrace Trace(const ZCombatHit &hit, float x, float y, float dx, float dy,
        float radius, const std::vector<ZCombatId> &skip) ;

    ZHitResult ApplyHit(ZCombatId target, const ZCombatHit &hit) ;

    void Splash(const ZCombatHit &hit, float radius) ;

    unsigned GetFailures() const {
        unsigned failures = 0;
        for (const CProp &prop : m_map.GetResources().props) {
            if (prop.HasScript()) { failures += prop.GetUnsupportedCount(); }
        }
        return failures;
    }

    unsigned GetHitCount() const { return m_hitCount; }

private:
    static constexpr ZCombatId kPropIdBase = 1ull << 62;

    bool PlayerInside(const CProp &prop) const ;

    void ApplyAction(CProp &prop, const CProp::Action &action);

    CMap &m_map;
    std::vector<CProp *> m_activeProps; // Stable until the next level Reset.
    const CLayerCollision *m_bodyLayer = nullptr;
    const CLayerCollision *m_bulletLayer = nullptr;
    CLevel &m_scene;
    CLevel &m_level;
    unsigned m_hitCount = 0;
};
