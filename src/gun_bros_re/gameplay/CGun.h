#include "gun_bros_re/gameplay/GameScriptObject.h"
/**
 * @file CGun.h
 * @brief The weapon template: stat tables, a script, and a model set.
 *
 * Port of CGun::Template (src/gunbros/gun.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:127712 (Init)
 *
 * Wire format (section 7, GUN):
 *   uint8         flag
 *   CGameAssetRef meshRef, imageRef
 *   GameObjectRef objectRef132
 *   uint16        value140
 *   int32         scalar144        -- 16.16 fixed point
 *   uint16        value148
 *   int32         scalar152        -- 16.16 fixed point
 *   uint8         flag256
 *   CScript       script
 *   uint32 table  [6]              -- uint16 count, then that many uint32
 *   CMoveSetMesh  moveSet
 *
 * A gun carries **two** models. The move set at the end is the one it animates;
 * meshRef is the weapon itself, loaded by CGun::Template::LoadMesh (:128918)
 * through GetResId(0x1E, ...), and imageRef is its atlas, handed to AddImage by
 * Load (:127965). GetResId (:78597) adds the section base to the ref's own id,
 * so an asset ref addresses section 31 exactly the way an ordinal does -- which
 * is why grepping for the ordinal route alone misses these.
 *
 * Fields whose purpose has not been traced keep the template offset in their
 * name. That is the one thing about them that is certainly true, and it makes
 * the decompile searchable from here.
 *
 * The move set is the **last** thing this reads, which is why guns cost a
 * whole template port to reach while players and enemies do not.
 *
 * Runtime clarification: this trailing move set overrides the PLAYER's upper
 * body. The weapon mesh itself has an independent animation clock.
 *
 * The six tables are per-tier stats. The original keeps only the first three
 * entries of each and floors three of the tables at 100; that is upgrade
 * logic, not parsing, so it stays out of here until M5 needs it.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CGUN_H
#define GUN_BROS_RE_GUN_BROS_CGUN_H

#include "engine/resources/CArrayInputStream.h"
#include "engine/glu/script/CScript.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include "engine/graphics/CMoveSetMesh.h"
#include "engine/glu/script/CScriptInterpreter.h"
#include "engine/graphics/CMeshAnimationController.h"

#include <cstdint>
#include <vector>

// Stat tables in a gun template, all read the same way.
constexpr std::uint32_t kGunStatTableCount = 6;
class CBullet;

/** A visual cue emitted by the original weapon script. */
struct GunCue {
    enum class Kind { Bullet, Effect, Trail, StopTrail, Sound, LoopSound, StopSound, Splash, SpawnEnemy, Grenade };
    Kind kind = Kind::Bullet;
    GameObjectRef resource;
    int hand = 0;
    int node = 0;
    float minimumAngle = 0.0f;
    float maximumAngle = 0.0f;
    float speed = 1.0f;
    bool alternate = false;
    bool alignEffect = false;
    float damage = 0;
    bool percentDamage = false;
    int spawnObjectId = -1;
    bool forceSpawn = false;
    float radius = 0;
    float cone = 360;
    float force = 0;
    int forceMs = 0;
};

class CGun : public GameScriptObject {
public:
    class Template {
    public:
        Template();

        bool Init(CArrayInputStream &stream);

        const CScript &GetScript() const { return m_script; }

        /** The weapon model, and the atlas it wears. Section 31 and 29. */
        const CGameAssetRef &GetMeshRef() const { return m_meshRef; }
        const CGameAssetRef &GetImageRef() const { return m_imageRef; }
        const CMoveSetMesh &GetMoveSet() const { return m_moveSet; }
        std::uint8_t GetCategory() const { return m_flag104; }
        std::uint8_t GetHandedness() const { return m_flag256; }
        const GameObjectRef &GetBulletRef() const { return m_objectRef132; }
        std::uint16_t GetFireIntervalMs() const { return m_value140; }
        unsigned GetMasteryLevel(unsigned experience) const;
        unsigned GetMasteryLimit() const;
        unsigned GetMasteryThreshold(unsigned index) const;
        unsigned GetMasteryModifier(unsigned table, unsigned level, unsigned base) const;
        unsigned GetCriticalDamageScale() const { if (m_value148 == 0) { return 10; } return m_value148; }

    private:
        std::uint8_t m_flag104;
        CGameAssetRef m_meshRef;
        CGameAssetRef m_imageRef;
        GameObjectRef m_objectRef132;
        std::uint16_t m_value140;
        float m_scalar144;
        std::uint16_t m_value148;
        float m_scalar152;
        std::uint8_t m_flag256;

        CScript m_script;
        std::vector<std::uint32_t> m_statTables[kGunStatTableCount];
        CMoveSetMesh m_moveSet;
    };

    CGun();
    ~CGun();
    CGun(const CGun &) = delete;
    CGun &operator=(const CGun &) = delete;
    /** Bind before OnEquip; overrides refer to this template's move set. */
    void Bind(const Template &data, const CMesh *mesh, bool beam = false);
    void OnEquip();
    /** Original Configure / OnRemove association, scoped to this gun instance. */
    void AddBullet(CBullet &bullet);
    void OnBulletRemoved(CBullet &bullet);
    void SetShooting(bool shooting);
    void Fire();
    void Update(std::int32_t deltaMs);
    void OnScriptStateEntered() override;
    std::int16_t FunctionResolver(std::uint8_t function,
        const std::int16_t *arguments, std::uint8_t argumentCount);
    std::int16_t *VariableResolver(std::uint8_t variable);

    const Template *GetTemplate() const { return m_template; }
    const std::vector<std::int32_t> &GetOverrides() const { return m_overrides; }
    CMeshAnimationController &GetAnimation() { return m_animation; }
    const CMeshAnimationController &GetAnimation() const { return m_animation; }
    int GetFireMode() const { return m_fireMode; }
    bool CanFire() const { return m_ammo != 0; }
    bool IsBeam() const { return m_beam; }
    bool IsShooting() const { return m_shooting; }
    void SetMasteryExperience(unsigned experience);
    unsigned GetMasteryLevel() const { return m_mastery; }
    unsigned GetFireRateMs() const;
    float GetMasteryDamageMultiplier(float randomUnit = 1) const;
    unsigned GetMasterySpeedMod() const;
    float GetHeatIntensity() const { return m_heatIntensity; }
    std::vector<GunCue> TakeCues();

private:
    void DetachBullets();
    std::vector<CBullet *> m_bullets;
    const Template *m_template;
    CScriptInterpreter m_interpreter;
    CMeshAnimationController m_animation;
    std::vector<std::int32_t> m_overrides;
    std::vector<GunCue> m_cues;
    std::int16_t m_ammo;
    std::int16_t m_mastery;
    std::int32_t m_functionTimer;
    std::uint8_t m_timerFunction;
    std::int32_t m_eventTimer;
    int m_fireMode;
    bool m_shooting;
    bool m_beam;
    float m_heatIntensity;
    float m_targetHeat;
};

#endif  // GUN_BROS_RE_GUN_BROS_CGUN_H
