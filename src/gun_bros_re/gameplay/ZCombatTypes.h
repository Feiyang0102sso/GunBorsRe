/** @file ZCombatTypes.h
 * @brief World-space combat messages shared by actors and projectiles.
 */
#ifndef GUN_BROS_RE_ZCOMBATTYPES_H
#define GUN_BROS_RE_ZCOMBATTYPES_H

#include "gun_bros_re/data/CGameAssetRef.h"
#include <cstdint>
#include <vector>
#include <string>

struct ZWeaponCombatProgress {
    GameObjectRef resource;
    unsigned experience = 0;
    unsigned maximum = 0;
};

struct ZEnemyCasualty {
    GameObjectRef resource;
    unsigned count = 0;
    std::string name;
};

// IDs survive vector growth and never point at actors that have been removed.
using ZCombatId = std::uint64_t;
constexpr ZCombatId kNoCombatId = 0;
constexpr ZCombatId kPlayerCombatId = 1;
constexpr ZCombatId kBrotherCombatId = UINT64_MAX;

enum class ZHitResult { Pending, Ignored, Hit, Killed };

/** The actor owns health across equipment changes. Legacy harnesses start invincible. */
struct ZPlayerVitals {
    float maximum = 0;
    float health = 0;
    float lastDamage = 0;
    float incomingDamage = 0;
    float flash = 0;
    int stunMs = 0;
    bool invincible = true;
    // Viewer-only unlimited health keeps the original nonfatal damage event.
    bool unlimitedHealth = false;
    bool dead = false;
    // The original Flow native 1 reports death only after its mesh sequence.
    bool deathAnimationComplete = false;
    bool inputHidden = false;
    unsigned hits = 0;
    unsigned deaths = 0;

    void Reset() {
        health = maximum;
        lastDamage = 0;
        incomingDamage = 0;
        flash = 0;
        stunMs = 0;
        dead = false;
        deathAnimationComplete = false;
        inputHidden = false;
        hits = 0;
        deaths = 0;
    }
};

struct ZCombatHit {
    ZCombatId projectile = 0;
    ZCombatId owner = 0;
    GameObjectRef weapon;
    unsigned weaponSlot = 0;
    unsigned weaponMasteryLimit = 0;
    GameObjectRef bullet;
    bool critical = false;
    int ownerType = 0;
    float damage = 0;
    float x = 0;
    float y = 0;
    float direction = 0;
    std::uint32_t flags = 0;
    int part = -1;
    int edge = -1;
    bool splash = false;
    // CProp native 7 passes an actor source, not a bullet carrying its owner.
    bool propExplosion = false;
    bool percentDamage = false;
    bool applyArmorAttack = true; // Scripted fixed-damage air strikes bypass gun bonuses.
    int spawnObjectId = -1;
    bool forceSpawn = false;
};

struct ZCombatTrace {
    ZCombatId target = 0;
    float fraction = 1;
    int part = -1;
    int edge = -1;
    float normalX = 0;
    float normalY = 0;
};

/** Gameplay owns target filtering and damage; projectile presentation asks it. */
class CLevel;
class ZProjectileWorld {
public:
    virtual ~ZProjectileWorld() = default;
    virtual CLevel *GetScriptLevel() { return nullptr; }
    virtual ZCombatTrace Trace(const ZCombatHit &hit, float x, float y,
        float dx, float dy, float radius, const std::vector<ZCombatId> &skipTargets) = 0;
    virtual ZHitResult ApplyHit(ZCombatId target, const ZCombatHit &hit) = 0;
    virtual float GetDamageMultiplier(ZCombatId owner, float fallback = 1) const { return fallback; }
    virtual float GetProjectilePowerupMultiplier(ZCombatId owner) const { return 1; }
    virtual float GetEnemyTimeScale() const { return 1; }
    virtual void Splash(const ZCombatHit &hit, float radius, float coneDegrees,
        float force, int forceMs) = 0;
    virtual void SpawnFromProjectile(const GameObjectRef &resource,
        const ZCombatHit &hit) = 0;
    virtual bool FindTarget(const ZCombatHit &hit, float radius, float &x, float &y) = 0;
    /** Ordinary brother particles remain anchored while death Flow finishes. */
    virtual bool ParticleAnchor(ZCombatId actor, float &x, float &y, float &z, float &angle) {
        return Anchor(actor, -1, -1, x, y, z, angle);
    }
    /** Linked enemy particles use its current part and actor facing. */
    virtual bool LinkedParticleAnchor(ZCombatId actor, int node, float &x, float &y, float &z, float &angle) {
        float direction = 0;
        if (!Anchor(actor, 0, node, x, y, z, direction)) { return false; }
        angle = direction + 90;
        return true;
    }
    virtual bool Anchor(ZCombatId actor, int part, int node,
        float &x, float &y, float &z, float &direction) = 0;
};

#endif
