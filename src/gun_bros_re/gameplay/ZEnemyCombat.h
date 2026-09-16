/** @file ZEnemyCombat.h
 * @brief Runtime state next to the original enemy's script and part table.
 */
#ifndef GUN_BROS_RE_ZENEMYCOMBAT_H
#define GUN_BROS_RE_ZENEMYCOMBAT_H

#include "gun_bros_re/gameplay/ZCombatTypes.h"
#include "gun_bros_re/gameplay/CCollisionData.h"
#include <array>
#include <vector>

/** Script-native side effects are consumed by the scene after actor update. */
struct ZEnemyAction {
    enum class Kind {
        Bullet, Effect, LinkedEffect, StopEffect, Sound, LoopSound, StopSound,
        Splash, Broadcast, SpawnEnemy, RemoveBullet, CollisionResolved,
        Shake, Reward, Stun, LevelEvent, SpawnPickup, TurretActive, Teleported
    };
    Kind kind = Kind::Effect;
    GameObjectRef resource;
    int part = 0;
    int node = -1;
    int slot = 0;
    float x = 0;
    float y = 0;
    float direction = 0;
    float speed = 0;
    float damage = 0;
    float radius = 0;
    float force = 0;
    int durationMs = 0;
    ZCombatId projectile = 0;
    ZCombatId owner = 0;
    ZHitResult result = ZHitResult::Pending;
};

struct ZEnemyCombat {
    GameObjectRef templateRef;
    // IDs are the original class 7 variable IDs (:68982), not field offsets.
    // 0 move speed; 1 facing mode; 2 hit part; 3 damage /256;
    // 4 splash-hit flag; 5 hit world angle; 6 bullet speed; 7 hit edge group;
    // 8 facing; 9 relative hit angle; 10 turn speed; 11 aim node;
    // 12 contact force; 13 contact force time; 14 active part;
    // 15 health bar visibility; 16 collision allegiance; 17 contact damage;
    // 18 path index; 19 critical hit; 20 wave index.
    std::array<std::int16_t, 21> variables{};
    bool enabled = false;
    ZCombatId id = 0;
    // Local equivalent of the projectile creator's participant ownership.
    ZCombatId summoner = 0;
    float x = 0;
    float y = 0;
    float previousX = 0;
    float previousY = 0;
    float facing = 0;
    float health = 0;
    float maxHealth = 0;
    float lastDamage = 0;
    float totalDamage = 0;
    float hitFlash = 0;
    int healthBarFlashMs = 0; // CEnemy::Damage mem+1240, independent of mesh tint.
    bool dead = false;
    bool removed = false;
    int portalObjectId = -1;
    bool portalActive = false;
    bool targetable = true;
    bool turret = false;
    int targetType = 0;
    ZCombatId targetId = 0;
    float targetX = 0;
    float targetY = 0;
    bool hasNavigationTarget = false;
    float navigationX = 0;
    float navigationY = 0;
    // CFlock snapshot for this logical tick; independent of script state.
    float flockX = 0;
    float flockY = 0;
    bool targetAlive = false;
    float targetRange = 100000;
    int behaviour = 7;
    float arrivalDistance = 0;
    bool arrived = false;
    float destinationX = 0;
    float destinationY = 0;
    int movementTimer = -1;
    float rotationStart = 0;
    float rotationEnd = 0;
    float rotationTime = 0;
    float rotationDuration = 0;
    bool rotationSmooth = false;
    int functionTimer = 0;
    int timerFunction = 0;
    int eventTimer = 0;
    int autoFireInterval = 0;
    int autoFireTimer = 0;
    int autoFireResource = -1;
    float triggerDistance = 0;
    bool triggerInside = false;
    float scaleFactor = 1;
    int deathCount = 0;
    int hitCount = 0;
    unsigned deferredMechanisms = 0; // 1 boss presentation, 2 level context, 4 rewards.
    std::uint32_t randomState = 1;
    GameObjectRef bullet;
    CCollisionData collision;
    ZCombatHit pendingHit;
    bool collisionPending = false;
    ZHitResult collisionResult = ZHitResult::Pending;
    std::vector<ZEnemyAction> actions;
};

#endif
