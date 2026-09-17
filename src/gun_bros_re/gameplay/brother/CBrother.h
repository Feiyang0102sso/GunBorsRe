#include "gun_bros_re/gameplay/ZGameScriptObject.h"
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
#include "gun_bros_re/gameplay/CGun.h"
#include "gun_bros_re/gameplay/ZCombatTypes.h"
#include "engine/graphics/CMoveSetMeshController.h"
// CGameSpriteGluRef lives here, next to its first user
// Historical location above; now declared in original gameAssetRef module.

class CPowerUpSelector;

class CBrother : public ZGameScriptObject {
public:
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
    };
    class Template {
    public:
        Template();

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
        CScript m_script;
        CMoveSetMesh m_moveSet;

        // TODO: template offset 104. CBrother copies it around but no reader
        // has been traced to a meaning yet.
        GameObjectRef m_objectRef;

        // Template offset 112. A uint16 on the wire, a float in memory.
        float m_gameScale;

        CGameSpriteGluRef m_shadowSprite;
    };

    CBrother();
    bool UsePowerup(CPowerUpSelector &selector, bool fromSelector = false);
    void SetVitals(ZPlayerVitals *vitals) { m_vitals = vitals; }
    void SetPowerupState(PowerupState *powerups);
    void StartShield(const GameObjectRef &effect, int durationMs);
    void StartAutoFire(const GameObjectRef &effect, int durationSeconds);
    bool IsAutoFire() const { return m_powerups != nullptr && m_powerups->autoFireMs > 0; }
    bool IsTurretActive() const { return m_powerups != nullptr && m_powerups->turretActive; }
    void SetTurretIsActive(bool active) { if (m_powerups != nullptr) { m_powerups->turretActive = active; } }
    void StartFrenzyType(const GameObjectRef &effect, int durationMs, float multiplier, unsigned type);
    void StartFrenzy(const GameObjectRef &effect, int durationMs, float attack, float defense, float speed);
    void StopFrenzy();
    bool IsFrenzy() const { return m_powerups != nullptr && m_powerups->legacyFrenzyMs > 0; }
    bool IsShield() const { return m_powerups != nullptr && m_powerups->shieldMs > 0; }
    bool IsFrenzyType(unsigned type) const { return type < 3 && m_powerups != nullptr && m_powerups->frenzyMs[type] > 0; }
    float GetFrenzyMultiplier(unsigned type) const;
    float GetProjectilePowerupMultiplier() const;
    void SetHuman(bool human) { m_variables[2] = human; }
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
    ZHitResult ReceiveDamage(float damage);
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
    bool m_visible = true;
    bool m_spawned = true;
    bool m_immunityHidden = false;
    bool m_weaponSwapRequested = false;
    int m_knockbackMs = 0;
    int m_knockbackDurationMs = 0;
    ZPlayerVitals *m_vitals = nullptr;
    PowerupState *m_powerups = nullptr;
    std::shared_ptr<CParticlePool> m_particlePool;
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
