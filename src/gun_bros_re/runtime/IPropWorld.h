/** @file IPropWorld.h
 * @brief Map-owned prop lifetime and script messages; no graphics in CLevel.
 */
#ifndef GUN_BROS_RE_IPROPWORLD_H
#define GUN_BROS_RE_IPROPWORLD_H
#include "gun_bros/CombatTypes.h"
class IPropWorld {
public:
    virtual ~IPropWorld() = default;
    virtual void Reset() = 0;
    virtual void Update(int deltaMs) = 0;
    virtual void SendMessage(int objectId, int message) = 0;
    virtual CombatTrace Trace(const CombatHit &hit, float x, float y, float dx, float dy,
        float radius, const std::vector<CombatId> &skip) { return {}; }
    virtual HitResult ApplyHit(CombatId target, const CombatHit &hit) { return HitResult::Ignored; }
    virtual void Splash(const CombatHit &hit, float radius) {}
};
#endif
