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

#include "engine/CArrayInputStream.h"
#include "glu_script/CScript.h"
#include "gun_bros/CGameAssetRef.h"
#include "gun_bros/CProp.h"  // CGameSpriteGluRef

#include <cstdint>

// What a CGameAssetRef holds when it points at nothing.
constexpr std::int32_t kNoAssetId = -1;

class CBullet {
public:
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
};

#endif  // GUN_BROS_RE_GUN_BROS_CBULLET_H
