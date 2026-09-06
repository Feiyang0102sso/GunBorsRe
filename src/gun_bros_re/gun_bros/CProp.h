/**
 * @file CProp.h
 * @brief The template behind a placed prop: which sprite it draws.
 *
 * Port of CProp::Template (src/gunbros/prop.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:123346 (Init), :124863 (Bind)
 *
 * Wire format, of which only the head is read here:
 *   CGameSpriteGluRef sprite      -- 7 bytes
 *   uint8             foregroundAnimation
 *   uint8             backgroundAnimation
 *   CCollisionData    collision
 *   CCollisionData    bulletCollision
 *   uint8             persistent
 *   CScript           script
 *   CMoveSet          moveSet
 *
 * Everything after the ninth byte belongs to collision, scripting and movement,
 * none of which draws. Parsing stops there rather than growing three more
 * subsystems to reach a field nobody reads.
 *
 * The two animation bytes are stored in the opposite order to the one they are
 * written in: the first byte on the wire is the FOREGROUND animation.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CPROP_H
#define GUN_BROS_RE_GUN_BROS_CPROP_H

#include "engine/CArrayInputStream.h"

#include <cstdint>

/**
 * Reference to a SpriteGlu character.
 *
 * Port of CGameSpriteGluRef (src/gunbros/gameAssetRef.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:191859
 *
 * Wire format: uint32 packHash, uint8 archetype, uint8 action, uint8 animation.
 *
 * Unlike CGameAssetRef this carries no resource id at all -- the archetype
 * index is resolved against whichever pack the hash names. That is why the
 * sprite atlases looked unaddressable from the section tables alone.
 */
struct CGameSpriteGluRef {
    std::uint32_t packHash;
    std::uint8_t archetype;
    std::uint8_t action;
    std::uint8_t animation;

    CGameSpriteGluRef();

    void Init(CArrayInputStream &stream);
};

/**
 * A prop template, read as far as its three sprite slots.
 *
 * A prop draws in up to three passes and each has its own animation, all on
 * the same archetype. Most templates use exactly one: 85 of the 261 in these
 * packs fill only the background slot, so reading just the main one would draw
 * a third of the map's scenery and silently drop the rest.
 */
class CProp {
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

    private:
        CGameSpriteGluRef m_sprite;
        std::uint8_t m_foregroundAnimation;
        std::uint8_t m_backgroundAnimation;
    };
};

#endif  // GUN_BROS_RE_GUN_BROS_CPROP_H
