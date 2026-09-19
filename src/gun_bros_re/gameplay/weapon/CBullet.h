#include "gun_bros_re/host/ZGameScriptObject.h"
/**
 * @file CBullet.h
 * @brief The projectile template: a sprite, a model, and a pile of scalars.
 *
 * Port of CBullet::Template (src/gunbros/bullet.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:130584 (Init), :130637 (Load),
 *            :60448 (LoadMesh)
 *
 * Wire format (section 4, BULLET):
 *   CGameSpriteGluRef sprite
 *   CGameAssetRef     meshRef, imageRef
 *   1 skipped byte
 *   int16             value16, value18, value20
 *   uint8             flag32
 *   int32             scalar28          -- 16.16 fixed point, as are the rest
 *   CScript           script
 *   uint32            value128
 *   int32             scalar24, scalar256, scalar116, scalar120
 *   uint16            value124
 *   int32             scalar260
 *   uint16            value264
 *   uint8             flag266
 *
 * The last owner of section 31. A bullet loads its mesh with no move set at
 * all (:60448 passes null to CMesh::Init), so every frame is kept -- which
 * costs nothing, because these are all single-frame models.
 *
 * **A ref is absent when its assetId is -1, not when its hash is zero.** Load
 * checks exactly that before asking for either resource, and most bullets are
 * sprites with no model at all.
 *
 * Fields whose purpose has not been traced keep the template offset in their
 * name.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CBULLET_H
#define GUN_BROS_RE_GUN_BROS_CBULLET_H

#include "engine/resources/CArrayInputStream.h"
#include "engine/glu/script/CScript.h"
#include "engine/glu/sprite/CSpritePlayer.h"
#include "gun_bros_re/data/objects/CGameAssetRef.h"  // CGameSpriteGluRef
#include "gun_bros_re/gameplay/weapon/CGun.h"
#include "gun_bros_re/gameplay/collision/Collision.h"
#include "gun_bros_re/effects/CLightningArc.h"

#include <cstdint>
#include <array>
#include "engine/graphics/CMesh.h"
#include "engine/graphics/ZTexture.h"
#include "engine/graphics/ZMeshBuffer.h"
#include <map>
#include "gun_bros_re/effects/EffectContainer.h"
class CParticleEffect;
class CParticlePool;

class ZSpriteRenderer;
class ZEffectColors;
class ZShaderProgram;
class CBrother;
struct ZEffectProjection;


// What a CGameAssetRef holds when it points at nothing.
constexpr std::int32_t kNoAssetId = -1;

class CBullet : public ZGameScriptObject {
public:
    class World;
    /** Read-only projectile evidence for the permanent weapon research checks. */
    struct State {
        GameObjectRef resource;
        Collision::ObjectId owner = 0;
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
    struct View {
        bool enabled = false;
        float left = 0, top = 0, width = 0, height = 0;
        bool PastBounds(float x, float y, float radius, float velocityX, float velocityY) const;
    };

    CBullet() = default;
    ~CBullet();
    CBullet(const CBullet &) = delete;
    CBullet &operator=(const CBullet &) = delete;
    class Template {
    public:
        /** Original Template::LoadMesh owns the mesh shared by its live bullets.
         * GPU buffers adapt that resource lifetime to the Windows renderer.
         */
        struct Mesh {
            CMesh mesh;
            ZTexture texture;
            ZMeshBuffer buffer;
        };
        Template();
        bool Load(CGunBros &tables, const ZShaderProgram &program, const GameObjectRef &resource);
        const Mesh *GetMesh() const { return m_mesh.get(); }

        bool Init(CArrayInputStream &stream);

        /** The projectile model and its atlas. Either may be absent. */
        const CGameAssetRef &GetMeshRef() const { return m_meshRef; }
        const CGameAssetRef &GetImageRef() const { return m_imageRef; }

        bool HasMesh() const { return m_meshRef.assetId != kNoAssetId; }
        bool HasImage() const { return m_imageRef.assetId != kNoAssetId; }

        const CGameSpriteGluRef &GetSpriteRef() const { return m_sprite; }
        const CScript &GetScript() const { return m_script; }
        std::uint32_t GetFlags() const { return m_value128; }
        float GetSpriteScale() const { return m_scalar24; }
        float GetMeshScale() const { return m_scalar256; }
        float GetSeekingTurnRate() const { return m_scalar28; }
        float GetAcceleration() const { return m_scalar116 * 100.0f; }
        float GetBaseDamage() const { return static_cast<float>(m_flag32); }
        float GetKnockbackSpeed() const { return m_scalar120; }
        unsigned GetKnockbackDurationMs() const { return m_value124; }
        float GetRadius() const { return m_value20 * m_scalar24; }
        float GetTrajectoryHeight() const { return m_scalar260; }
        unsigned GetTrajectoryDurationMs() const { return m_value264; }
        unsigned GetTrajectoryType() const { return m_flag266; }

    private:
        std::shared_ptr<Mesh> m_mesh;
        CGameSpriteGluRef m_sprite;
        CGameAssetRef m_meshRef;
        CGameAssetRef m_imageRef;

        std::int16_t m_value16;
        std::int16_t m_value18;
        std::int16_t m_value20;
        std::uint8_t m_flag32;
        float m_scalar28;

        CScript m_script;

        std::uint32_t m_value128;
        float m_scalar24;
        float m_scalar256;
        float m_scalar116;
        float m_scalar120;
        std::uint16_t m_value124;
        float m_scalar260;
        std::uint16_t m_value264;
        std::uint8_t m_flag266;
    };

    /** Visual script host; damage and enemy spawning belong to combat logic. */
    void Bind(const Template &data, bool alternate);
    /** CBullet::Fire tests the owner's position to the muzzle for every owner type. */
    void CheckSpawnCollision(float ownerX, float ownerY, const CCollisionData::Scene *collision);
    void Update(int deltaMs, int animationDurationMs);
    void Hit();
    /** ForceRemoval dispatches event 2 and releases the gun count immediately. */
    void ForceRemoval();
    void OnRemove();
    void OnWallCollision();
    void OnCollision(Collision::HitResult result);
    float GetDamage() const;
    /** Original pseudo-height: no collision-coordinate displacement. */
    float GetTrajectoryFraction() const;
    float GetTrajectoryPhaseScale() const;
    float GetTrajectoryHeight() const { const float phase = GetTrajectoryPhaseScale(); return m_trajectoryHeight * phase * phase * GetTrajectoryFraction(); }
    bool HasActiveCollision() const { return collisionEnabled && GetTrajectoryHeight() < m_collisionHeightThreshold; }
    unsigned GetTrajectoryEvents() const { return m_trajectoryEvents; }
    void SetScriptSequenceFrame(std::uint8_t frame) override;
    bool IsScriptSequenceFrameFinished() override { return animationFinished; }
    std::int16_t FunctionResolver(std::uint8_t function,
        const std::int16_t *arguments, std::uint8_t argumentCount);
    std::int16_t *VariableResolver(std::uint8_t variable);
    std::vector<ZGunCue> TakeCues();

    /** CBullet::Update owns travel, ray/actor collision and culling. */
    void UpdateProjectile(CBullet::World *world, ZSpriteRenderer &sprites,
        const CBullet::View &view, CBrother &player, const float *modelToScene,
        float facingDegrees, int deltaMs, const CCollisionData::Scene *collision, std::uint32_t &randomState);
    /** Original bullet sprite/mesh/arc draw; callers provide the Windows renderer. */
    void DrawProjectile(ZSpriteRenderer &sprites, ZEffectColors &colors, const ZShaderProgram &program,
        const float *sceneMvp, const ZEffectProjection &projection, float meshCameraScale,
        std::size_t &beamQuads, std::size_t &lightningQuads);
    void DrawLightning(ZSpriteRenderer &sprites, ZEffectColors &colors,
        const ZEffectProjection &projection, std::size_t &lightningQuads) const;
    void AttachParticleEffect(const CParticleEffect &data, std::shared_ptr<CParticlePool> pool, bool trail, bool align);
    void StopTrail();
    void ApplyRibbonCue(const ZGunCue &cue);
    void StopAttachedEffects();
    void UpdateAttachedEffects(int deltaMs, std::uint32_t &randomState);
    EffectContainer effects;
    Collision::ObjectId id = 0;
    Collision::ObjectId owner = Collision::Player;
    GameObjectRef weapon;
    unsigned weaponSlot = 0;
    unsigned weaponMasteryLimit = 0;
    float masteryDamageMultiplier = 1;
    bool critical = false;
    int ownerType = 0;
    float damageMultiplier = 1;
    float powerupMultiplier = 1;
    int part = 0;
    bool pendingHit = false;
    std::map<Collision::ObjectId, int> hitUntil;
    CLightningArc lightningArc;
    const Template *data = nullptr;
    ZGunCue source;
    float x = 0, y = 0, z = 0;
    float direction = 0;
    float speed = 0;
    float length = 0;
    bool beam = false;
    // CEnemy::FireBullet passes no anchor callback; only gun beams follow a muzzle.
    bool followsMuzzle = false;
    bool spawnCollision = false;
    float spawnNormalX = 0, spawnNormalY = 0;

    int ageMs = 0;
    int animation = 0;
    // Bind fixes cap slots from the resource; Flow only changes the body player.
    int beamSourceAnimation = 0;
    int beamEndAnimation = 0;
    int animationAgeMs = 0;
    bool animationFinished = false;
    bool removed = false;
    bool visible = true;
    std::uint32_t flags = 0;
    float velocityScale = 1.0f;
    float acceleration = 0.0f;
    bool collisionEnabled = true;
    float seekingTurnRate = 0;
    Collision::ObjectId seekingTarget = Collision::NoObject;
    void InitializeSeeking(World *world);
    void UpdateSeeking(World *world, int deltaMs);
    int maximumBeamLength = 3000; // CBullet::Bind :63673; native18 overrides range.
    ZBulletRibbonSettings ribbon;
    ZBulletLightningSettings lightning;
    int zOrderGroup = 3;
    /** Original group :64005 and owner-relative Z order :60280. */
    int GetZOrderGroup() const { return zOrderGroup; }
    int GetZOrder(float ownerX, float ownerY, bool hasOwner) const;

private:
    friend class CGun;
    void PrepareSpriteAnimation(ZSpriteRenderer &sprites);
    CSpritePlayer m_spritePlayer;
    std::vector<std::uint16_t> m_spriteDurations;
    int m_boundAnimation = -1;
    std::size_t m_beamSourceFrame = 0;
    std::size_t m_beamEndFrame = 0;
    EffectContainer::Handle m_trailHandle = 0;
    EffectContainer::Handle m_ribbonHandle = 0;
    bool m_retirementStarted = false;
    EffectHolder::Anchor EffectAnchor(bool align, bool hit) const;
    CGun *m_sourceGun = nullptr;
    CScriptInterpreter m_interpreter;
    std::vector<ZGunCue> m_cues;
    int m_timer = 0;
    std::uint8_t m_timerFunction = 0;
    std::int16_t m_masteryLevel = 0;
    std::int16_t m_damagePeriodMs = 0;
    int m_damageDeltaMs = 0;
    float m_damage = 0;
    float m_trajectoryHeight = 0;
    unsigned m_trajectoryDurationMs = 0;
    unsigned m_trajectoryType = 0;
    unsigned m_trajectoryEvents = 0;
    float m_collisionHeightThreshold = 0.25f;
};

/** Gameplay owns target filtering and damage; projectile presentation asks it. */
class CLevel;
class CBullet::World {
public:
    virtual ~World() = default;
    virtual CLevel *GetScriptLevel() { return nullptr; }
    virtual Collision::Trace Trace(const Collision::Hit &hit, float x, float y,
        float dx, float dy, float radius, const std::vector<Collision::ObjectId> &skipTargets) = 0;
    virtual Collision::HitResult ApplyHit(Collision::ObjectId target, const Collision::Hit &hit) = 0;
    virtual float GetDamageMultiplier(Collision::ObjectId owner, float fallback = 1) const { return fallback; }
    virtual float GetProjectilePowerupMultiplier(Collision::ObjectId owner) const { return 1; }
    virtual float GetEnemyTimeScale() const { return 1; }
    virtual void Splash(const Collision::Hit &hit, float radius, float coneDegrees,
        float force, int forceMs) = 0;
    virtual void SpawnFromProjectile(const GameObjectRef &resource,
        const Collision::Hit &hit) = 0;
    virtual Collision::ObjectId FindSeekTarget(const Collision::Hit &hit, float angle) = 0;
    /** Ordinary brother particles remain anchored while death Flow finishes. */
    virtual bool ParticleAnchor(Collision::ObjectId actor, float &x, float &y, float &z, float &angle) {
        return Anchor(actor, -1, -1, x, y, z, angle);
    }
    /** Linked enemy particles use its current part and actor facing. */
    virtual bool LinkedParticleAnchor(Collision::ObjectId actor, int node, float &x, float &y, float &z, float &angle) {
        float direction = 0;
        if (!Anchor(actor, 0, node, x, y, z, direction)) { return false; }
        angle = direction + 90;
        return true;
    }
    virtual bool Anchor(Collision::ObjectId actor, int part, int node,
        float &x, float &y, float &z, float &direction) = 0;
};

#endif  // GUN_BROS_RE_GUN_BROS_CBULLET_H
