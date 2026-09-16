/** @file ZPropWorld.h
 * @brief Map-owned prop lifetime and script messages; no graphics in CLevel.
 */
#ifndef GUN_BROS_RE_ZPROPWORLD_H
#define GUN_BROS_RE_ZPROPWORLD_H
#include "gun_bros_re/gameplay/ZCombatTypes.h"
class ZPropWorld {
public:
    virtual ~ZPropWorld() = default;
    virtual void Reset() = 0;
    virtual void StartLayer(int layer) {}
    virtual bool Spawn(int layer, int objectId) { return false; }
    virtual void Update(int deltaMs) = 0;
    virtual void SendMessage(int objectId, int message) = 0;
    virtual bool GetObjectPosition(int objectId, float &x, float &y) const { return false; }
    virtual bool IsActivePortal(int objectId) const { return false; }
    virtual unsigned ResolveIndicatorTarget(int objectId) const { return 0; }
    virtual bool GetIndicatorTarget(unsigned key, float &x, float &y) const { return false; }
    virtual ZCombatTrace Trace(const ZCombatHit &hit, float x, float y, float dx, float dy,
        float radius, const std::vector<ZCombatId> &skip) { return {}; }
    virtual ZHitResult ApplyHit(ZCombatId target, const ZCombatHit &hit) { return ZHitResult::Ignored; }
    virtual void Splash(const ZCombatHit &hit, float radius) {}
};
#endif
