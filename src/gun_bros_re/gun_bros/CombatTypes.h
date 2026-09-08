/** @file CombatTypes.h
 * @brief World-space combat messages shared by actors and projectiles.
 */
#ifndef GUN_BROS_RE_COMBATTYPES_H
#define GUN_BROS_RE_COMBATTYPES_H

#include "gun_bros/CGameAssetRef.h"
#include <cstdint>
#include <vector>

// IDs survive vector growth and never point at actors that have been removed.
using CombatId = std::uint64_t;
constexpr CombatId kNoCombatId = 0;
constexpr CombatId kPlayerCombatId = 1;
constexpr CombatId kBrotherCombatId = UINT64_MAX;

enum class HitResult { Pending, Ignored, Hit, Killed };

/** The actor owns health across equipment changes. Arena starts invincible. */
struct PlayerVitals {
    float maximum = 0;
    float health = 0;
    float lastDamage = 0;
    float incomingDamage = 0;
    float flash = 0;
    int stunMs = 0;
    bool invincible = true;
    bool dead = false;
    unsigned hits = 0;
    unsigned deaths = 0;

    void Reset() {
        health = maximum;
        lastDamage = 0;
        incomingDamage = 0;
        flash = 0;
        stunMs = 0;
        dead = false;
        hits = 0;
        deaths = 0;
    }
};

struct CombatHit {
    CombatId projectile = 0;
    CombatId owner = 0;
    int ownerType = 0;
    float damage = 0;
    float x = 0;
    float y = 0;
    float direction = 0;
    std::uint32_t flags = 0;
    int part = -1;
    int edge = -1;
    bool splash = false;
    bool percentDamage = false;
    bool applyArmorAttack = true; // Scripted fixed-damage air strikes bypass gun bonuses.
    int spawnObjectId = -1;
    bool forceSpawn = false;
};

struct CombatTrace {
    CombatId target = 0;
    float fraction = 1;
    int part = -1;
    int edge = -1;
    float normalX = 0;
    float normalY = 0;
};

/** Gameplay owns target filtering and damage; projectile presentation asks it. */
class IProjectileWorld {
public:
    virtual ~IProjectileWorld() = default;
    virtual CombatTrace Trace(const CombatHit &hit, float x, float y,
        float dx, float dy, float radius, const std::vector<CombatId> &skipTargets) = 0;
    virtual HitResult ApplyHit(CombatId target, const CombatHit &hit) = 0;
    virtual float GetDamageMultiplier(CombatId owner, float fallback = 1) const { return fallback; }
    virtual float GetProjectilePowerupMultiplier(CombatId owner) const { return 1; }
    virtual float GetEnemyTimeScale() const { return 1; }
    virtual void Splash(const CombatHit &hit, float radius, float coneDegrees,
        float force, int forceMs) = 0;
    virtual void SpawnFromProjectile(const GameObjectRef &resource,
        const CombatHit &hit) = 0;
    virtual bool FindTarget(const CombatHit &hit, float radius, float &x, float &y) = 0;
    virtual bool Anchor(CombatId actor, int part, int node,
        float &x, float &y, float &z, float &direction) = 0;
};

#endif
