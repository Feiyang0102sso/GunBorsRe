/** CBullet travel and collision, ported from :62375-63590. */
#define NOMINMAX
#include "gun_bros_re/gameplay/weapon/CBullet.h"
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "gun_bros_re/gameplay/collision/Collision.h"
#include "engine/glu/sprite/ZSpriteRenderer.h"
#include <algorithm>
#include <cmath>
using Collision::SegmentFraction;
namespace { constexpr float kRadians = 3.14159265f / 180.0f; }

void CBullet::CheckSpawnCollision(float ownerX, float ownerY, const CCollisionData::Scene *collision) {
    spawnCollision = false;
    spawnNormalX = 0;
    spawnNormalY = 0;
    if (collision == nullptr) { return; }
    // Original CBullet::Fire :62212-62243. Testing only the subsequent travel
    // misses an obstacle already crossed by the owner-to-muzzle offset.
    const CCollisionData *shape = &collision->walls;
    if ((flags & 0x20) != 0) { shape = &collision->terrain; }
    SegmentFraction(ownerX, ownerY, x - ownerX, y - ownerY,
        shape, &spawnNormalX, &spawnNormalY, &spawnCollision);
}

/**
 * CBullet::CanBeCulled :60583, as the original writes it.
 *
 * The camera rectangle is compared against the projectile's own bounds one
 * axis at a time, and only on the axis it is travelling along: a bullet is
 * culled once it has left the view moving away from it. A bullet still
 * approaching the view, or one crossing it sideways, is kept.
 */
bool CBullet::View::PastBounds(float x, float y, float radius, float velocityX, float velocityY) const {
    if (!enabled) { return false; }
    if (velocityX < 0 && x + radius < left) { return true; }
    if (velocityX > 0 && x - radius > left + width) { return true; }
    if (velocityY < 0 && y + radius < top) { return true; }
    if (velocityY > 0 && y - radius > top + height) { return true; }
    return false;
}

void CBullet::UpdateProjectile(CBullet::World *world, ZSpriteRenderer &sprites,
    const CBullet::View &view, CBrother &player, const float *modelToScene,
    float facingDegrees, int deltaMs, const CCollisionData::Scene *collision, std::uint32_t &randomState) {
    int shotDeltaMs = deltaMs;
    if (ownerType == 1 && world != nullptr) {
        shotDeltaMs = std::max(1, static_cast<int>(std::lround(deltaMs * world->GetEnemyTimeScale())));
    }
    const CCollisionData *shotCollision = nullptr;
    if (collision != nullptr) {
        shotCollision = &collision->walls;
        if ((flags & 0x20) != 0) { shotCollision = &collision->terrain; }
    }
    const CGameSpriteGluRef &sprite = data->GetSpriteRef();
    PrepareSpriteAnimation(sprites);
    const int duration = sprites.Animation(sprite.packHash, sprite.archetype, animation).durationMs;
    Update(shotDeltaMs, duration);
    if (beam && !removed) {
        lightningArc.Update(lightning, shotDeltaMs, randomState);
    }
    if (removed) { return; }
    Collision::Hit hit;
    hit.projectile = id;
    hit.owner = owner;
    hit.weapon = weapon;
    hit.weaponSlot = weaponSlot;
    hit.bullet = source.resource;
    hit.critical = critical;
    hit.weaponMasteryLimit = weaponMasteryLimit;
    hit.ownerType = ownerType;
    hit.flags = flags;
    hit.x = x;
    hit.y = y;
    hit.damage = GetDamage() * powerupMultiplier * masteryDamageMultiplier;
    if (world != nullptr) {
        hit.damage *= world->GetDamageMultiplier(owner, damageMultiplier);
    }
    UpdateSeeking(world, shotDeltaMs);
    const float startX = x, startY = y;
    bool hitWall = spawnCollision;
    float wallNormalX = spawnNormalX, wallNormalY = spawnNormalY;
    spawnCollision = false;
    const float radians = direction * kRadians;
    if (!hitWall && beam) {
        const float beamLength = static_cast<float>(maximumBeamLength);
        if (followsMuzzle && owner == Collision::Player) {
            if (!player.ActiveWeapon().IsShooting()) { removed = true; }
            player.ProjectMuzzle(modelToScene, source.hand, source.node, x, y, z);
            direction = facingDegrees - 90;
        } else if (followsMuzzle && world != nullptr && !world->Anchor(owner, part,
            source.node, x, y, z, direction)) {
            removed = true;
        }
        const float dx = std::cos(direction * kRadians) * beamLength;
        const float dy = std::sin(direction * kRadians) * beamLength;
        length = beamLength * SegmentFraction(x, y, dx, dy, shotCollision);
    } else if (!hitWall) {
        speed = std::max(0.0f, speed + acceleration * shotDeltaMs * 0.001f);
        const float distance = speed * velocityScale * shotDeltaMs * 0.001f;
        const float dx = std::cos(radians) * distance, dy = std::sin(radians) * distance;
        const float fraction = SegmentFraction(x, y, dx, dy, shotCollision, &wallNormalX, &wallNormalY);
        x += dx * fraction; y += dy * fraction;
        hitWall = fraction < 1.0f;
        // CBullet::Update :63537 culls a moved projectile as soon as
        // CanBeCulled agrees, and CBullet::Remove(this, 1) :60862 retires
        // it with no hit event and no wall event. Template flag 0x10 keeps
        // a projectile alive off screen; beams are never culled.
        if ((flags & 0x10) == 0 &&
            view.PastBounds(x, y, data->GetRadius(), dx, dy)) {
            removed = true;
            hitWall = false;
        }
    }
    // CEnemy::HandleCollision :71435 leaves an unhandled event pending in
    // the enemy, but single-player bullets keep moving/testing collisions.
    // Only the multiplayer ApplyCollision branch :71676 pauses a bullet.
    if (world != nullptr && HasActiveCollision() && !removed) {
        float traceX = startX, traceY = startY;
        float dx = x - startX, dy = y - startY;
        if (beam) {
            traceX = x;
            traceY = y;
            dx = std::cos(direction * kRadians) * length;
            dy = std::sin(direction * kRadians) * length;
        }
        std::vector<Collision::ObjectId> skip;
        for (auto it = hitUntil.begin(); it != hitUntil.end();) {
            if (it->second <= ageMs) { it = hitUntil.erase(it); }
            else { skip.push_back(it->first); ++it; }
        }
        // Sweep the full segment, so a fast projectile cannot jump over a
        // target. Penetrating projectiles continue through remaining actors.
        for (int contact = 0; contact < 64; ++contact) {
            // CBullet::UpdateBeam -> RayCastNearest uses a ray, not the
            // bullet's large authored sprite/collision radius (up to 345).
            float radius = data->GetRadius();
            if (beam) { radius = 0; }
            const Collision::Trace trace = world->Trace(hit, traceX, traceY, dx, dy, radius, skip);
            if (trace.target == 0) { break; }
            hit.x = traceX + dx * trace.fraction;
            hit.y = traceY + dy * trace.fraction;
            hit.direction = direction;
            // Original HandleCollision :137831 normalizes the moving bullet's
            // velocity. A stationary projectile has no force direction.
            if (speed * velocityScale > 0) {
                hit.knockbackSpeed = data->GetKnockbackSpeed();
                hit.knockbackDurationMs = data->GetKnockbackDurationMs();
            }
            hit.part = trace.part;
            hit.edge = trace.edge;
            const Collision::HitResult result = world->ApplyHit(trace.target, hit);
            pendingHit = result == Collision::HitResult::Pending;
            OnCollision(result);
            skip.push_back(trace.target);
            if (!beam) { hitUntil[trace.target] = ageMs + 100; }
            if (beam) {
                length *= trace.fraction;
                break;
            }
            if (removed) {
                x = hit.x;
                y = hit.y;
                break;
            }
            if ((flags & 0x1000) != 0) {
                const float normalLength = std::hypot(trace.normalX, trace.normalY);
                if (normalLength > 0) {
                    const float nx = trace.normalX / normalLength, ny = trace.normalY / normalLength;
                    const float vx = std::cos(direction * kRadians), vy = std::sin(direction * kRadians);
                    const float dot = vx * nx + vy * ny;
                    direction = std::atan2(vy - 2 * dot * ny, vx - 2 * dot * nx) / kRadians;
                }
                x = hit.x;
                y = hit.y;
                break;
            }
        }
    }
    // Resolve actor/prop contacts along the clipped segment before the
    // wall event retires the shot. Otherwise destructible props are walls
    // that can never receive their original on-hit script callback.
    if (hitWall && !removed) {
        if ((flags & 0x800) != 0) {
            const float vx = std::cos(direction * kRadians);
            const float vy = std::sin(direction * kRadians);
            const float dot = vx * wallNormalX + vy * wallNormalY;
            const float reflectedX = vx - 2 * dot * wallNormalX;
            const float reflectedY = vy - 2 * dot * wallNormalY;
            direction = std::atan2(reflectedY, reflectedX) / kRadians;
            // Separate the next sweep from this exact edge contact.
            x += reflectedX * 0.01f;
            y += reflectedY * 0.01f;
        }
        OnWallCollision();
    }
}
