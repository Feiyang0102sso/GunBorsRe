/** @file CombatTypes.h
 * @brief World-space combat messages shared by actors and projectiles.
 */
#ifndef GUN_BROS_RE_COMBATTYPES_H
#define GUN_BROS_RE_COMBATTYPES_H

#include "gun_bros_re/data/CGameAssetRef.h"
#include <cstdint>
#include <vector>
#include <string>

struct WeaponCombatProgress {
    GameObjectRef resource;
    unsigned experience = 0;
    unsigned maximum = 0;
};

struct EnemyCasualty {
    GameObjectRef resource;
    unsigned count = 0;
    std::string name;
};

// IDs survive vector growth and never point at actors that have been removed.
using CombatId = std::uint64_t;
constexpr CombatId kNoCombatId = 0;
constexpr CombatId kPlayerCombatId = 1;
constexpr CombatId kBrotherCombatId = UINT64_MAX;

enum class HitResult { Pending, Ignored, Hit, Killed };

/** The actor owns health across equipment changes. Legacy harnesses start invincible. */
struct PlayerVitals {
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

struct CombatHit {
    CombatId projectile = 0;
    CombatId owner = 0;
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

struct CombatTrace {
    CombatId target = 0;
    float fraction = 1;
    int part = -1;
    int edge = -1;
    float normalX = 0;
    float normalY = 0;
};

/** Gameplay owns target filtering and damage; projectile presentation asks it. */
class CLevel;
class IProjectileWorld {
public:
    virtual ~IProjectileWorld() = default;
    virtual CLevel *GetScriptLevel() { return nullptr; }
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
