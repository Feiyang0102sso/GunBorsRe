#include "gun_bros_re/gameplay/GameScriptObject.h"
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
#include "gun_bros_re/data/CGameAssetRef.h"  // CGameSpriteGluRef
#include "gun_bros_re/gameplay/CGun.h"
#include "gun_bros_re/gameplay/CombatTypes.h"
#include "gun_bros_re/gameplay/CLightningArc.h"

#include <cstdint>
#include <array>

/** CBullet native12/13: authored point count, width, sample time and RGBA. */
struct BulletRibbonSettings {
    unsigned capacity = 0;
    float width = 0;
    unsigned intervalMs = 0;
    std::array<std::uint16_t, 4> color{};
};

// What a CGameAssetRef holds when it points at nothing.
constexpr std::int32_t kNoAssetId = -1;

class CBullet : public GameScriptObject {
public:
    CBullet() = default;
    ~CBullet();
    CBullet(const CBullet &) = delete;
    CBullet &operator=(const CBullet &) = delete;
    class Template {
    public:
        Template();

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
        float GetAcceleration() const { return m_scalar116 * 100.0f; }
        float GetBaseDamage() const { return static_cast<float>(m_flag32); }
        float GetRadius() const { return m_value20 * m_scalar24; }
        float GetTrajectoryHeight() const { return m_scalar260; }
        unsigned GetTrajectoryDurationMs() const { return m_value264; }
        unsigned GetTrajectoryType() const { return m_flag266; }

    private:
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
    void Update(int deltaMs, int animationDurationMs);
    void Hit();
    /** ForceRemoval dispatches event 2 and releases the gun count immediately. */
    void ForceRemoval();
    void OnRemove();
    void OnWallCollision();
    void OnCollision(HitResult result);
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
    std::vector<GunCue> TakeCues();

    int ageMs = 0;
    int animation = 0;
    int animationAgeMs = 0;
    bool animationFinished = false;
    bool removed = false;
    bool visible = true;
    std::uint32_t flags = 0;
    float velocityScale = 1.0f;
    float acceleration = 0.0f;
    bool collisionEnabled = true;
    float seekRadius = 0;
    int maximumBeamLength = 3000; // CBullet::Bind :63673; native18 overrides range.
    BulletRibbonSettings ribbon;
    BulletLightningSettings lightning;
    int zOrderGroup = 3;

private:
    friend class CGun;
    CGun *m_sourceGun = nullptr;
    CScriptInterpreter m_interpreter;
    std::vector<GunCue> m_cues;
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

#endif  // GUN_BROS_RE_GUN_BROS_CBULLET_H
