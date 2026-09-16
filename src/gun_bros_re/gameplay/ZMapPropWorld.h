#pragma once
/** Connects placed CProp instances to level events and combat queries. */
#include "gun_bros_re/gameplay/ZMapResources.h"
#include "gun_bros_re/gameplay/CLevel.h"

namespace MapDetail {
void BuildCollisionScene(ZLoadedMap &loaded);
/** Original script state lives beside the map instance, never in shared quads. */
class ZMapPropWorld : public ZPropWorld {
public:
    ZMapPropWorld(ZLoadedMap &map, CLevel &scene, CLevel &level, ZWeaponEffects &effects)
        : m_map(map), m_scene(scene), m_level(level), m_effects(effects) {
        for (ZPlacedProp &prop : m_map.props) {
            if (prop.sprite->data.GetScript().IsPresent()) { prop.runtime = std::make_shared<CProp>(); }
        }
    }

    void Reset() override {
        m_activeProps.clear();
        for (ZPlacedProp &prop : m_map.props) { prop.active = false; }
        BuildCollisionScene(m_map);
    }

    void StartLayer(int layer) override ;

    bool Spawn(int layer, int objectId) override ;

    void SendMessage(int objectId, int message) override ;

    unsigned ResolveIndicatorTarget(int objectId) const override {
        for (unsigned index = 0; index < m_activeProps.size(); ++index) {
            const auto &prop = *m_activeProps[index];
            if (prop.objectId == objectId && prop.active && (!prop.runtime || !prop.runtime->IsRemoved())) { return index + 1; }
        }
        return 0;
    }

    bool GetObjectPosition(int objectId, float &x, float &y) const override {
        const unsigned key = ResolveIndicatorTarget(objectId);
        if (key == 0) { return false; }
        x = m_activeProps[key - 1]->x;
        y = m_activeProps[key - 1]->y;
        return true;
    }

    bool GetIndicatorTarget(unsigned key, float &x, float &y) const override ;
    bool IsActivePortal(int objectId) const override {
        const unsigned key = ResolveIndicatorTarget(objectId);
        if (key == 0) { return false; }
        const auto &prop = *m_activeProps[key - 1];
        return prop.runtime != nullptr && prop.runtime->IsActivePortal();
    }

    void Update(int deltaMs) override ;

    ZCombatTrace Trace(const ZCombatHit &hit, float x, float y, float dx, float dy,
        float radius, const std::vector<ZCombatId> &skip) override ;

    ZHitResult ApplyHit(ZCombatId target, const ZCombatHit &hit) override ;

    void Splash(const ZCombatHit &hit, float radius) override ;

    unsigned GetFailures() const {
        unsigned failures = 0;
        for (const ZPlacedProp &prop : m_map.props) {
            if (prop.runtime != nullptr) { failures += prop.runtime->GetUnsupportedCount(); }
        }
        return failures;
    }

    unsigned GetHitCount() const { return m_hitCount; }

private:
    static constexpr ZCombatId kPropIdBase = 1ull << 62;
    void SyncPlayers(ZPlacedProp &prop) {
        prop.foreground = prop.runtime->GetPlayer(2);
        prop.main = prop.runtime->GetPlayer(1);
        prop.background = prop.runtime->GetPlayer(0);
    }

    bool PlayerInside(const ZPlacedProp &prop) const ;

    void ApplyAction(ZPlacedProp &prop, const ZPropAction &action);

    ZLoadedMap &m_map;
    std::vector<ZPlacedProp *> m_activeProps; // Stable until the next level Reset.
    const CLayerCollision *m_bodyLayer = nullptr;
    const CLayerCollision *m_bulletLayer = nullptr;
    CLevel &m_scene;
    CLevel &m_level;
    ZWeaponEffects &m_effects;
    unsigned m_hitCount = 0;
};

}
