/** @file ZWeaponEffects.h
 * @brief Original weapon cues rendered in a shared two-dimensional scene.
 */
#ifndef GUN_BROS_RE_ZWEAPONEFFECTS_H
#define GUN_BROS_RE_ZWEAPONEFFECTS_H

#include "gun_bros_re/gameplay/brother/ZPlayerModel.h"
#include "gun_bros_re/gameplay/CCollisionData.h"

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
    // Exact current collision parameters; beams use a ray, not their sprite radius.
    float collisionRadius = 0;
    bool collisionEnabled = false;
};

class CParticlePool;

/** Game-layer adapter for CBullet and particle effects; not an original class.
 * Engine backends own only generic mesh/quad drawing and WAV playback.
 */
class ZWeaponEffects {
public:
    ZWeaponEffects(CResTOCManager &toc, ZPackTables &tables, const ZShaderProgram &program,
        std::shared_ptr<CParticlePool> particlePool = nullptr);
    ~ZWeaponEffects();
    /** Windows audio adaptation: coalesce identical one-shots within one tick. */
    void BeginAudioFrame();
    /** Consume gun cues, then advance both new and existing projectiles. */
    void Update(ZPlayerModel &player, const float *modelToScene, float facingDegrees,
                int deltaMs, const ZWeaponCollision *collision = nullptr);
    /** Consume another brother's cues without advancing all projectiles twice. */
    void EmitBrother(ZPlayerModel &player, const float *modelToScene, float facingDegrees,
        ZCombatId owner, const ZWeaponCollision *collision = nullptr);
    /** Optional world-to-screen projection for the rotating character preview. */
    void Draw(const float *sceneMvp, const float *previewProjection = nullptr, float meshCameraScale = 1.0f,
              ZWeaponDrawPass pass = ZWeaponDrawPass::All);
    void Clear();
    void SetCombatWorld(ZProjectileWorld *world);
    /**
     * The camera rectangle CBullet::CanBeCulled :60583 tests against.
     *
     * Without one no projectile is culled, which is what the standalone
     * research scenes and the arena had before.
     */
    void SetViewBounds(float centerX, float centerY, float width, float height);
    /** Enemy/manual projectile speed is in world units per second. */
    ZCombatId SpawnProjectile(const GameObjectRef &resource, float x, float y, float z,
        float direction, float speed, ZCombatId owner, int ownerType, int part = 0, int node = 0);
    void ResolveHit(ZCombatId projectile, ZHitResult result);
    void Emit(const ZGunCue &cue, float x, float y, float z, float direction,
        ZCombatId actor = 0, int slot = 0, int part = 0, int node = 0);
    bool RemoveOldestProjectile(ZCombatId owner);
    void RetireOwner(ZCombatId owner);
    void PlayMoveSound(const GameObjectRef &sound);
    /** CPickup owns an emitter handle; stopping it preserves living particles. */
    // The historical StopEffect name now means immediate Stop. Use StopSpawning to drain.
    std::uint64_t StartPersistentEffect(const GameObjectRef &resource, float x, float y, bool loop = false,
        std::shared_ptr<CParticlePool> particlePool = nullptr);
    void StopEffect(std::uint64_t handle);
    /** CPickup::OnRemove :99723 and CTransferEffect::Update :174361 drain. */
    void StopSpawning(std::uint64_t handle);
    /** Standalone research scenes have no brother/projectile update. */
    void AdvanceAmbientEffects(int deltaMs);
    /** Finite actor bursts include emitted particles after their emitter ends. */
    bool HasActorBurst(ZCombatId actor) const;
    void SetPaused(bool paused);
    std::size_t GetBulletCount() const;
    std::size_t GetParticleCount() const;
    std::size_t GetEffectCount() const;
    std::size_t GetTrailCount() const;
    std::size_t GetRibbonCount() const;
    std::size_t GetDrawnBeamQuadCount() const;
    std::size_t GetDrawnLightningQuadCount() const;
    std::size_t GetShotCount() const;
    std::vector<ZWeaponProjectileState> GetProjectileStates() const;
    std::size_t GetSoundCueCount() const;
    /** Voices sounding right now. One WAV never occupies more than one. */
    unsigned GetVoiceCount() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
#endif
