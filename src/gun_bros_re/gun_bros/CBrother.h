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

#include "engine/CArrayInputStream.h"
#include "glu_script/CScript.h"
#include "gun_bros/CGameAssetRef.h"
#include "gun_bros/CMoveSetMesh.h"
#include "gun_bros/CGun.h"
#include "gun_bros/CMoveSetMeshController.h"
#include "gun_bros/CProp.h"  // CGameSpriteGluRef lives here, next to its first user

class CBrother : public IScriptObject {
public:
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
    /** Run player and weapon scripts against decoded, stable mesh banks. */
    void Bind(const CScript &script, const CMoveSetMesh &moves,
        const std::vector<const CMesh *> &bodyMeshes, CGun &gun,
        const std::vector<const CMesh *> &weaponMeshes);
    void SetInput(bool moving, bool shooting);
    void Update(std::int32_t deltaMs);
    void SetScriptSequenceFrame(std::uint8_t frame) override;
    bool IsScriptSequenceFrameFinished() override;
    void OnScriptStateEntered() override;
    std::int16_t FunctionResolver(std::uint8_t function,
        const std::int16_t *arguments, std::uint8_t argumentCount);
    std::int16_t *VariableResolver(std::uint8_t variable);
    CMoveSetMeshController &GetTorso() { return m_torso; }
    CMoveSetMeshController &GetLegs() { return m_legs; }
    bool TorsoUsesWeapon() const { return m_torsoUsesWeapon; }
    int GetStateId() const { return m_interpreter.GetStateId(); }

private:
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
