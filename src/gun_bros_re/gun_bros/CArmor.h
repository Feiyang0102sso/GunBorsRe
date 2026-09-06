/**
 * @file CArmor.h
 * @brief The armour template: two model variants, one per brother.
 *
 * Port of CArmor::Template (src/gunbros/armor.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:176438 (Init), :176493 (Load),
 *            :176635 (LoadMesh)
 *
 * Wire format (section 3, ARMOR):
 *   uint8         flag4
 *   CGameAssetRef meshRef[0], imageRef[0]
 *   uint8         flag224
 *   CGameAssetRef meshRef[1], imageRef[1]
 *   uint8         flag225
 *   CGameAssetRef spriteImageRef[0], spriteImageRef[1]
 *   CScript       script
 *
 * The two variants are selected by `12 * variant` indexing in Validate and
 * Load, so they are the same armour on two different characters. Each variant
 * has a model with its own atlas, plus a flat image for when there is no model
 * -- Load (:176493) picks imageRef when the mesh is present and falls back to
 * spriteImageRef when it is not.
 *
 * Armour is the largest owner of section 31: 136 of the 334 meshes, more than
 * players, enemies, guns and bullets put together. Nothing else addresses
 * those, which is why pack4 looks ownerless until this template is read.
 *
 * **A ref is absent when its assetId is -1**, the same rule bullets follow.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CARMOR_H
#define GUN_BROS_RE_GUN_BROS_CARMOR_H

#include "engine/CArrayInputStream.h"
#include "glu_script/CScript.h"
#include "gun_bros/CBullet.h"  // kNoAssetId
#include "gun_bros/CGameAssetRef.h"

#include <cstdint>

// One model per brother.
constexpr std::uint32_t kArmorVariantCount = 2;

class CArmor {
public:
    class Template {
    public:
        Template();

        bool Init(CArrayInputStream &stream);

        const CGameAssetRef &GetMeshRef(std::uint32_t variant) const {
            return m_meshRef[variant];
        }
        const CGameAssetRef &GetImageRef(std::uint32_t variant) const {
            return m_imageRef[variant];
        }

        bool HasMesh(std::uint32_t variant) const {
            return m_meshRef[variant].assetId != kNoAssetId;
        }

        const CScript &GetScript() const { return m_script; }

    private:
        std::uint8_t m_flag4;
        std::uint8_t m_flag224;
        std::uint8_t m_flag225;

        CGameAssetRef m_meshRef[kArmorVariantCount];
        CGameAssetRef m_imageRef[kArmorVariantCount];

        // Used instead of imageRef when the variant has no model.
        CGameAssetRef m_spriteImageRef[kArmorVariantCount];

        CScript m_script;
    };
};

#endif  // GUN_BROS_RE_GUN_BROS_CARMOR_H
