#include "gun_bros_re/host/ZGameScriptObject.h"
/**
 * @file CBrother.h
 * @brief The player template: a script, a model set, and a shadow sprite.
 *
 * Port of CBrother::Template (src/gunbros/brother.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:134571 (Init)
 *
 * Wire format (section 16, PLAYER):
 *   CScript           script
 *   CMoveSetMesh      moveSet
 *   GameObjectRef     objectRef
 *   uint16            unknown        -- kept as a float by the original
 *   GameObjectRef     [2]            -- read and thrown away
 *   CGameSpriteGluRef shadowSprite
 *
 * The character is a mesh, not a sprite: CBrother::Draw goes through
 * CMeshCamera::DrawHeirarchy and the sprite reference is only used by
 * DrawBackground, which is the shadow on the ground.
 *
 * Shallow enough to port whole, so it doubles as the check that the move set
 * is read correctly: a player template that parses to its last byte proves
 * everything in front of that byte, CMoveSetMesh included.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CBROTHER_H
#define GUN_BROS_RE_GUN_BROS_CBROTHER_H

#include "engine/resources/CArrayInputStream.h"
#include "engine/glu/script/CScript.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include "engine/graphics/CMoveSetMesh.h"
#include "gun_bros_re/gameplay/weapon/CGun.h"
#include "gun_bros_re/gameplay/collision/Collision.h"
#include "engine/graphics/CMoveSetMeshController.h"
#include "gun_bros_re/effects/CParticleEffectPlayer.h"
#include <array>
#include <map>
#include "gun_bros_re/gameplay/armor/CArmor.h"
#include "gun_bros_re/data/ZPackTables.h"
class ZShaderProgram;
// CGameSpriteGluRef lives here, next to its first user
// Historical location above; now declared in original gameAssetRef module.

class CPowerUpSelector;

class CBrother : public ZGameScriptObject {
public:
    /** The actor owns health across equipment changes. Legacy harnesses start invincible. */
    struct Vitals {
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


    /** Original six strengthening players share the brother's 25-slot pool.
     * Stable actor state survives this port's interchangeable weapon banks.
     */
    struct PowerupParticles {
        std::shared_ptr<CParticlePool> pool = std::make_shared<CParticlePool>(25);
        std::array<CParticleEffectPlayer, 6> players;
        float x = 0, y = 0;
        void Apply(const ZGunCue &cue, const CParticleEffect *effect);
        void Update(int deltaMs, std::uint32_t &randomState);
        void Draw(ZSpriteRenderer &renderer, const float *projection) const;
        void Stop();
        std::size_t GetParticleCount() const;
        std::size_t GetEffectCount() const;
    };
    /** Actor-owned state survives equipment changes, as in original CBrother. */
    struct PowerupState {
        int shieldMs = 0;
        int autoFireMs = 0;
        int legacyFrenzyMs = 0;
        float legacyFrenzyMultiplier[3]{1, 1, 1};
        bool turretActive = false;
        int frenzyMs[3]{};
        float frenzyMultiplier[3]{1, 1, 1};
        GameObjectRef effects[6];
        std::shared_ptr<PowerupParticles> particles;
    };
    /** Project the animated weapon node through the actor world matrix. */
    bool ProjectMuzzle(const float *matrix, int hand, int node, float &x, float &y, float &z);
    class Template {
    public:
        Template();
        /**
         * Find the player template, whichever pack it lives in.
         *
         * There is exactly one in the whole library, so the first hit is the answer.
         *
         * @return false when no pack carries a readable one.
         */
        bool Load(CResTOCManager &toc, ZPackTables &tables);
        const std::string &GetOwner() const { return m_owner; }

        bool Init(CArrayInputStream &stream);

        const CScript &GetScript() const { return m_script; }
        const CMoveSetMesh &GetMoveSet() const { return m_moveSet; }
        const GameObjectRef &GetObjectRef() const { return m_objectRef; }

        /** The sprite drawn under the model. Not the character itself. */
        const CGameSpriteGluRef &GetShadowSprite() const { return m_shadowSprite; }

        /**
         * The scale the player is drawn at in the world.
         *
         * CBrother::Bind (:135608) copies this straight into this[495], and
         * CBrother::Draw (:134960) multiplies it into the draw scale next to
         * the mesh's inverse extent and the camera's scale -- the same product
         * an enemy's template word 66 goes into.
         */
        float GetGameScale() const { return m_gameScale; }

    private:
        std::string m_owner;
        CScript m_script;
        CMoveSetMesh m_moveSet;

        // TODO: template offset 104. CBrother copies it around but no reader
        // has been traced to a meaning yet.
        GameObjectRef m_objectRef;

        // Template offset 112. A uint16 on the wire, a float in memory.
        float m_gameScale;

        CGameSpriteGluRef m_shadowSprite;
    };

    /**
     * A player and his parts.
     *
     * Held by pointer per part because a CMeshBuffer owns a GL name and cannot be
     * copied or moved, and because controller mesh arrays point back at each owned `mesh`.
     * Actor state and equipment have independent ownership and lifetimes.
     */
    CBrother();
    ~CBrother();
    CBrother(const CBrother &) = delete;
    CBrother &operator=(const CBrother &) = delete;
    // Owned here so the controllers can point at it.
    CMoveSetMesh moveSet;
    unsigned brotherIndex = 0;
    unsigned friendCount = 0;
    PowerupState powerups;
    std::unique_ptr<CGun> weapon;
    // CBrother owns two guns in the original. UI keeps both mesh banks alive
    // while the old torso finishes raising after native 3 switches the gun.
    std::unique_ptr<CGun> uiOtherWeapon;
    CGun *uiActiveWeapon = nullptr;
    std::unique_ptr<CArmor> armor[kArmorSlotCount];
    GameObjectRef gunResource;
    unsigned gunSlot = 0; // Retained on projectiles after the player switches guns.
    unsigned masteryExperience = 0;
    std::map<std::uint64_t, unsigned> masteryByWeapon;
    /** Both menu and combat retain the outgoing torso until its move ends. */
    CGun &ActiveWeapon() const {
        if (uiActiveWeapon != nullptr) { return *uiActiveWeapon; }
        return *weapon;
    }

    /**
     * Build the torso and the legs. No weapon: that is a separate call.
     *
     * @return false when the set has fewer than the two configs a player needs.
     */
    bool BuildBody(ZPackTables &tables, const CMoveSetMesh &moveSet);

    /** Compose a muzzle in the same raw coordinate space as Draw. */
    bool GetMuzzle(int hand, int node, ZMeshBoneTransform &out);

    /** Create the GL buffers for every part. */
    bool CreateBuffers(const ZShaderProgram &program);

    /**
     * The box the whole player occupies.
     *
     * Only the parts that hang off nothing count: an attached part sits inside the
     * body anyway, and letting a long rifle drive the framing would make the
     * character shrink every time the gun changed.
     */
    ZMeshBounds GetBounds() const;

    /**
     * Draw every part against one base matrix.
     *
     * @param base Row-major, and it must already carry the scale: the vertices go
     *        in raw.
     */
    void Draw(const ZShaderProgram &program, const float *base);
    /** CBrother::DrawUI + CMeshCamera::OrientForUI, in full-menu pixel coordinates.
     * The region height scales the active torso's raw Z extent; weapons do not
     * change framing. Returns false when the active torso cannot be resolved. */
    bool BuildUIMatrix(float centerX, float top, float height,
        float facingRadians, float screenWidth, float screenHeight, float *out) const;

    /**
     * How much to scale a player's RAW vertices by to put him in the world.
     *
     * `torso.inverseExtent * runtimeScale * gameScale * cameraScale`, read off
     * CBrother::Draw (:134960): the inverse extent comes from the mesh at
     * this[454] -- the TORSO, not the combined body -- and the runtime factor
     * this[494] is 1 from CBrother::Bind onwards. Same shape as an enemy's, and
     * the same trap: the product applies to raw vertices, because the inverse
     * extent in it is what normalises them.
     *
     * @return 0 when there is no torso to measure.
     */
    float GetWorldScale(float gameScale, float cameraScale) const;

    /** Last uploaded torso vertices; null when its resource is unresolved. */
    const std::vector<float> *GetTorsoPose() const;

    /** Equip the actual template, including its player move overrides and scripts. */
    bool EquipWeapon(ZPackTables &tables, const CScript &playerScript,
        const CGun::Template &weapon, const std::string &owner);
    /** Load the second UI gun from BIG; the primary brother remains the sole host. */
    bool PrepareSecondaryWeapon(ZPackTables &tables, const CGun::Template &weapon,
        const std::string &owner);
    void SelectWeapon(bool primary);
    bool SelectCachedWeapon(ZPackTables &tables, const CGun::Template &data,
        const std::string &owner, std::uint64_t key, const ZShaderProgram &program);
    /** Replace only the template's own armour slot; other equipment stays equipped. */
    bool EquipArmor(ZPackTables &tables, const CArmor::Template &data,
        const ZShaderProgram &program);
    void ClearArmor();
    float GetArmorMultiplier(std::uint32_t attribute) const;
    const CScript &GetScript() const { return m_script; }
    CBrother::Vitals *GetVitals() const { return m_vitals; }
    void ClearScript();

    bool UsePowerup(CPowerUpSelector &selector, bool fromSelector = false);
    void SetVitals(CBrother::Vitals *vitals) { m_vitals = vitals; }
    std::shared_ptr<PowerupParticles> GetPowerupParticles() const { return m_powerupParticles; }
    void StartShield(const GameObjectRef &effect, int durationMs);
    void StartAutoFire(const GameObjectRef &effect, int durationSeconds);
    bool IsAutoFire() const { return powerups.autoFireMs > 0; }
    bool IsTurretActive() const { return powerups.turretActive; }
    void SetTurretIsActive(bool active) { powerups.turretActive = active; }
    void StartFrenzyType(const GameObjectRef &effect, int durationMs, float multiplier, unsigned type);
    void StartFrenzy(const GameObjectRef &effect, int durationMs, float attack, float defense, float speed);
    void StopFrenzy();
    bool IsFrenzy() const { return powerups.legacyFrenzyMs > 0; }
    bool IsShield() const { return powerups.shieldMs > 0; }
    bool IsFrenzyType(unsigned type) const { return type < 3 && powerups.frenzyMs[type] > 0; }
    float GetFrenzyMultiplier(unsigned type) const;
    float GetProjectilePowerupMultiplier() const;
    void SetHuman(bool human) { m_human = human; m_variables[2] = human; }
    bool CanMove() const { return m_spawned && m_variables[1] != 0; }
    /** CPlayer::Move :100724 skips enemy bodies only for this Flow timer. */
    bool CanPassEnemies() const { return m_variables[3] != 0; }
    /** CBrother::Draw :134741 consumes the independent immunity blink flag. */
    bool IsImmunityHidden() const {
        return m_variables[3] > 0 && m_immunityHidden && m_vitals != nullptr && m_vitals->health > 0;
    }
    /** CBrother constructor :139098; wall resolution uses a separate half radius. */
    float GetRadius() const { return 22.0f; }
    bool CanShoot() const { return m_spawned && m_variables[0] != 0; }
    Collision::HitResult ReceiveDamage(float damage);
    /** CBrother::SetForce :137709 starts export 4, including its authored sound. */
    bool BeginKnockback(int durationMs);
    /** Sample the original force envelope before advancing this frame's timers. */
    float GetKnockbackStepSeconds(int deltaMs) const;
    /** Shared fatal transition; desktop suicide bypasses damage protection. */
    bool StartDeath();
    /** Native 18 controls the entire actor, independently of immunity blinking. */
    bool IsVisible() const { return m_visible; }
    /** CLevel::OnStart :120748 skips Spawn for DM until the selector completes. */
    void WaitForSpawn();
    bool HasSpawned() const { return m_spawned; }
    /** CBrother::Respawn :135976 invokes PLAYER export 8, including its effect. */
    bool Respawn();
    void Stun(int durationMs);
    /** Original CBrother::OnWaveCleared (:135964), including script recovery. */
    void OnWaveCleared();
    /** CBrother::OnRevive :135970 -> original PLAYER export 7 (normal revive). */
    bool OnRevive(unsigned reason = 0);
    /** CPlayer::OnSwapGun :101048 forwards input event 5 to this script. */
    bool OnSwapGun() { return m_interpreter.HandleEvent(5, 5); }
    bool TakeWeaponSwap() { bool requested = m_weaponSwapRequested; m_weaponSwapRequested = false; return requested; }
    /** Queue original events 10/11; inventory is committed only after spawning. */
    void SetGrenade(unsigned slot, const GameObjectRef &resource, unsigned count);
    bool OnThrowGrenade(unsigned slot);
    bool CanThrowGrenade(unsigned slot) const;
    void OnGrenadeThrown(unsigned slot);
    unsigned TakeThrownGrenades(unsigned slot);
    bool HasGrenadeRequest(unsigned slot) const { return slot < 2 && (m_grenadePending[slot] || m_grenadeAnimating[slot]); }
    std::vector<ZGunCue> TakeCues();
    /** Run player and weapon scripts against decoded, stable mesh banks. */
    void Bind(const CScript &script, const CMoveSetMesh &moves,
        const std::vector<const CMesh *> &bodyMeshes, CGun &gun,
        const std::vector<const CMesh *> &weaponMeshes);
    void SetInput(bool moving, bool shooting);
    void Update(std::int32_t deltaMs);
    /** Menu-specific Flow export and animation update, from SpawnForUI/UpdateUI. */
    bool SpawnForUI();
    void UpdateUI(std::int32_t deltaMs);
    /** Native 3 replaces the active gun without restarting the current torso. */
    void SetUIGun(CGun &gun, const std::vector<const CMesh *> &weaponMeshes);
    void SetScriptSequenceFrame(std::uint8_t frame) override;
    bool IsScriptSequenceFrameFinished() override;
    void OnScriptStateEntered() override;
    std::int16_t FunctionResolver(std::uint8_t function,
        const std::int16_t *arguments, std::uint8_t argumentCount);
    std::int16_t *VariableResolver(std::uint8_t variable);
    CMoveSetMeshController &GetTorso() { return m_torso; }
    const CMoveSetMeshController &GetTorso() const { return m_torso; }
    CMoveSetMeshController &GetLegs() { return m_legs; }
    bool TorsoUsesWeapon() const { return m_torsoUsesWeapon; }
    int GetStateId() const { return m_interpreter.GetStateId(); }

private:
    struct Drawing;
    struct TorsoDrawing;
    std::unique_ptr<Drawing> m_drawing;
    TorsoDrawing ResolveTorsoDrawing() const;
    /** Put every part's current pose in its buffer. Still parts show frame 0. */
    void UploadPose();
    // Stable gun/mesh banks keep torso sequences valid across PvP swaps.
    std::map<std::uint64_t, std::unique_ptr<CGun>> m_cachedWeapons;
    CScript m_script;
    bool m_human = true;
    bool m_visible = true;
    bool m_spawned = true;
    bool m_immunityHidden = false;
    bool m_weaponSwapRequested = false;
    int m_knockbackMs = 0;
    int m_knockbackDurationMs = 0;
    CBrother::Vitals *m_vitals = nullptr;
    void RestorePowerupEffects();
    std::shared_ptr<PowerupParticles> m_powerupParticles;
    void PowerupEffect(const GameObjectRef &effect, int slot, bool active);
    GameObjectRef m_grenades[2];
    unsigned m_grenadeStock[2]{};
    unsigned m_grenadesThrown[2]{};
    bool m_grenadePending[2]{};
    bool m_grenadeAnimating[2]{};
    std::vector<ZGunCue> m_cues;
    void SetShooting(bool shooting);
    bool m_triggerHeld;
    CScriptInterpreter m_interpreter;
    CMoveSetMeshController m_torso;
    CMoveSetMeshController m_legs;
    const CMoveSetMesh *m_baseMoves;
    CGun *m_gun;
    std::vector<const CMesh *> m_bodyMeshes;
    std::vector<const CMesh *> m_weaponMeshes;
    std::int32_t m_moveAliases[11];
    std::int16_t m_variables[7];
    std::int32_t m_timer;
    std::int32_t m_fireElapsed;
    bool m_torsoUsesWeapon;
    bool m_moving;
    bool m_shooting;
    bool m_canFire;
};

#endif  // GUN_BROS_RE_GUN_BROS_CBROTHER_H
