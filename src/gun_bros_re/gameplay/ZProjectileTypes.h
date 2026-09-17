#pragma once
#include "gun_bros_re/gameplay/CCollisionData.h"
#include "gun_bros_re/gameplay/ZCombatTypes.h"
/** Original map selectors plus prop bullet shapes, assembled by the scene. */
struct ZWeaponCollision {
    CCollisionData walls;
    CCollisionData terrain;
};

enum class ZWeaponDrawPass { All, BehindPlayer, InFrontOfPlayer };

/** Read-only projectile evidence for the permanent weapon research checks. */
struct ZWeaponProjectileState {
    GameObjectRef resource;
    ZCombatId owner = 0;
    bool beam = false;
    float x = 0, y = 0, direction = 0, length = 0;
    int animation = 0;
    int ageMs = 0;
    int beamSourceAnimation = 0, beamEndAnimation = 0;
    // Exact current collision parameters; beams use a ray, not their sprite radius.
    float collisionRadius = 0;
    bool collisionEnabled = false;
};


/** Camera input to CBullet::CanBeCulled (:60583). */
struct ZProjectileView {
    bool enabled = false;
    float left = 0, top = 0, width = 0, height = 0;
    bool PastBounds(float x, float y, float radius, float velocityX, float velocityY) const;
};
