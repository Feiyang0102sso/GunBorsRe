/** @file WeaponEffects.h
 * @brief Original weapon cues rendered in a shared two-dimensional scene.
 */
#ifndef GUN_BROS_RE_WEAPONEFFECTS_H
#define GUN_BROS_RE_WEAPONEFFECTS_H

#include "runtime/PlayerModel.h"
#include "gun_bros/CCollisionData.h"

/** Original map selectors plus prop bullet shapes, assembled by the scene. */
struct WeaponCollision {
    CCollisionData walls;
    CCollisionData terrain;
};

enum class WeaponDrawPass { All, BehindPlayer, InFrontOfPlayer };

/** Game-layer adapter for CBullet and particle effects; not an original class.
 * Engine backends own only generic mesh/quad drawing and WAV playback.
 */
class WeaponEffects {
public:
    WeaponEffects(CResTOCManager &toc, PackTables &tables, const CShaderProgram &program);
    ~WeaponEffects();
    /** Consume gun cues, then advance both new and existing projectiles. */
    void Update(PlayerModel &player, const float *modelToScene, float facingDegrees,
                int deltaMs, const WeaponCollision *collision = nullptr);
    /** Optional world-to-screen projection for the rotating character preview. */
    void Draw(const float *sceneMvp, const float *previewProjection = nullptr, float meshCameraScale = 1.0f,
              WeaponDrawPass pass = WeaponDrawPass::All);
    void Clear();
    void SetCombatWorld(IProjectileWorld *world);
    /** Enemy/manual projectile speed is in world units per second. */
    CombatId SpawnProjectile(const GameObjectRef &resource, float x, float y, float z,
        float direction, float speed, CombatId owner, int ownerType, int part = 0, int node = 0);
    void ResolveHit(CombatId projectile, HitResult result);
    void Emit(const GunCue &cue, float x, float y, float z, float direction,
        CombatId actor = 0, int slot = 0, int part = 0, int node = 0);
    bool RemoveOldestProjectile(CombatId owner);
    void RetireOwner(CombatId owner);
    void PlayMoveSound(const GameObjectRef &sound);
    void SetPaused(bool paused);
    std::size_t GetBulletCount() const;
    std::size_t GetParticleCount() const;
    std::size_t GetTrailCount() const;
    std::size_t GetShotCount() const;
    std::size_t GetSoundCueCount() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
#endif
