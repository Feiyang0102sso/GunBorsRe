/**
 * @file CProp.h
 * @brief The template behind a placed prop: which sprite it draws.
 *
 * Port of CProp::Template (src/gunbros/prop.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:123346 (Init), :124863 (Bind)
 *
 * Wire format, read through both collision shapes here:
 *   CGameSpriteGluRef sprite      -- 7 bytes
 *   uint8             foregroundAnimation
 *   uint8             backgroundAnimation
 *   CCollisionData    collision
 *   CCollisionData    bulletCollision
 *   uint8             persistent
 *   CScript           script
 *   CMoveSet          moveSet
 *
 * Parsing stops after the script. The move set is not needed yet, but the
 * script resource table supplies the original transition particles and sound.
 *
 * The two animation bytes are stored in the opposite order to the one they are
 * written in: the first byte on the wire is the FOREGROUND animation.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CPROP_H
#define GUN_BROS_RE_GUN_BROS_CPROP_H

#include "engine/CArrayInputStream.h"
#include "glu_script/CScript.h"
#include "gun_bros/CCollisionData.h"
#include "gun_bros/CMoveSet.h"
#include "gun_bros/CGameAssetRef.h"
#include "glu_script/CScriptInterpreter.h"
#include "sprite_glu/CSpritePlayer.h"
#include <array>

#include <cstdint>

/**
 * A prop template, read as far as its three sprite slots.
 *
 * A prop draws in up to three passes and each has its own animation, all on
 * the same archetype. Most templates use exactly one: 85 of the 261 in these
 * packs fill only the background slot, so reading just the main one would draw
 * a third of the map's scenery and silently drop the rest.
 */
struct PropAction {
    enum class Kind { Effect, Sound, Splash, Destroyed, Entered, Portal, AttachedEffect, StopEffect };
    Kind kind = Kind::Effect;
    GameObjectRef resource;
    int group = 3;
    int damage = 0;
    int radius = 0;
    int force = 0;
    int forceMs = 0;
    bool playersOnly = false;
    int damageOwner = 0;
};

class CProp : public IScriptObject {
public:
    class Template {
    public:
        Template();

        bool Init(CArrayInputStream &stream);

        const CGameSpriteGluRef &GetSpriteRef() const { return m_sprite; }

        /** Animation for the middle pass, from the sprite reference. */
        std::uint8_t GetMainAnimation() const { return m_sprite.animation; }
        std::uint8_t GetForegroundAnimation() const { return m_foregroundAnimation; }
        std::uint8_t GetBackgroundAnimation() const { return m_backgroundAnimation; }

        /** Local-space collision used by players and enemies. */
        const CCollisionData &GetCollision() const { return m_collision; }

        /** Local-space collision used by bullets. Parsed for wire fidelity. */
        const CCollisionData &GetBulletCollision() const {
            return m_bulletCollision;
        }

        const CScript &GetScript() const { return m_script; }
        const CMoveSet &GetMoveSet() const { return m_moveSet; }
        // CProp::IsDone :123389: this wire flag means remove when health is zero.
        bool RemoveWhenDead() const { return m_persistent != 0; }

    private:
        CGameSpriteGluRef m_sprite;
        std::uint8_t m_foregroundAnimation;
        std::uint8_t m_backgroundAnimation;
        CCollisionData m_collision;
        CCollisionData m_bulletCollision;
        std::uint8_t m_persistent;
        CScript m_script;
        CMoveSet m_moveSet;
    };

    /** Runtime uses original slot numbers: 0 foreground, 1 main, 2 background. */
    void Bind(const Template &data, const std::vector<std::vector<std::uint16_t>> *durations = nullptr);
    void Update(int deltaMs, bool playerInside);
    void HandleMessage(int message);
    void Damage(float amount, std::uint32_t flags);
    std::int16_t FunctionResolver(std::uint8_t function, const std::int16_t *arguments, std::uint8_t count);
    std::int16_t *VariableResolver(std::uint8_t variable);
    void SetScriptSequenceFrame(std::uint8_t move) override;
    bool IsScriptSequenceFrameFinished() override;
    std::vector<PropAction> TakeActions();
    const CCollisionData &GetCollision(bool bullets = false) const;
    int GetAnimation(unsigned slot) const { return m_animations[slot]; }
    const CSpritePlayer &GetPlayer(unsigned slot) const { return m_players[slot]; }
    unsigned GetStateId() const { return m_interpreter.GetStateId(); }
    unsigned GetUnsupportedCount() const { return m_unsupported; }
    bool IsRemoved() const { return m_template != nullptr && m_template->RemoveWhenDead() && m_health <= 0; }
    const CCollisionData &GetEntryCollision() const { return m_collision; }
    void SetResearchState(std::uint8_t state) { m_interpreter.SetState(state); }
    bool CollisionChanged() const { return m_collisionChanged; }
    void ClearCollisionChanged() { m_collisionChanged = false; }
    float GetHealth() const { return m_health; }
private:
    void SetAnimation(int slot, int animation);
    void QueueResource(PropAction::Kind kind, int resource, int group = 3);
    const Template *m_template = nullptr;
    const std::vector<std::vector<std::uint16_t>> *m_durations = nullptr;
    CScriptInterpreter m_interpreter;
    std::array<int, 3> m_animations = {255, 255, 255};
    std::array<CSpritePlayer, 3> m_players;
    CCollisionData m_collision;
    CCollisionData m_bulletCollision;
    std::vector<PropAction> m_actions;
    float m_health = 0;
    float m_damage = 0;
    std::uint32_t m_damageFlags = 0;
    std::int16_t m_damageOwner = 0;
    int m_timerMs = 0;
    int m_move = -1;
    int m_moveSlot = 1;
    int m_lastMoveStep = -1;
    unsigned m_unsupported = 0;
    bool m_checkEntry = false;
    bool m_inside = false;
    bool m_collisionChanged = false;
};

#endif  // GUN_BROS_RE_GUN_BROS_CPROP_H
