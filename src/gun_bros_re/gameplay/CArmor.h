#include "gun_bros_re/gameplay/ZGameScriptObject.h"
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

#include "engine/resources/CArrayInputStream.h"
#include "engine/glu/script/CScript.h"
#include "engine/glu/script/CScriptInterpreter.h"
#include "gun_bros_re/gameplay/CBullet.h"  // kNoAssetId
#include "gun_bros_re/data/CGameAssetRef.h"

#include <cstdint>
#include <memory>

// One model per brother.
constexpr std::uint32_t kArmorVariantCount = 2;

// Correction (2026-09-08): the historical "one per brother" description above
// only applies to selected textures. Slot 1 can draw BOTH meshes at its two
// attachment nodes (CBrother::Draw :134891). Slot is template offset 4;
// offsets 224/225 are node indices, not flags. Keep the old note for provenance.
constexpr std::uint32_t kArmorSlotCount = 4;
constexpr std::uint32_t kArmorAttributeCount = 5;

class ZPackTables;
class ZShaderProgram;

class CArmor : public ZGameScriptObject {
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
        std::uint8_t GetSlot() const { return m_slot; }
        std::uint8_t GetAttachmentNode(std::uint32_t variant) const {
            return m_attachmentNode[variant];
        }
        const CGameAssetRef &GetFallbackImageRef(std::uint32_t variant) const {
            return m_spriteImageRef[variant];
        }
        const CGameAssetRef &GetLoadedImageRef(std::uint32_t variant) const {
            if (HasMesh(variant)) {
                return m_imageRef[variant];
            }
            return m_spriteImageRef[variant];
        }

    private:
        std::uint8_t m_slot;
        std::uint8_t m_attachmentNode[kArmorVariantCount];

        CGameAssetRef m_meshRef[kArmorVariantCount];
        CGameAssetRef m_imageRef[kArmorVariantCount];

        // Used instead of imageRef when the variant has no model.
        CGameAssetRef m_spriteImageRef[kArmorVariantCount];

        CScript m_script;
    };

    CArmor();
    ~CArmor();
    CArmor(const CArmor &) = delete;
    CArmor &operator=(const CArmor &) = delete;
    const Template &GetTemplate() const { return m_templateData; }
    /** Load the original BIG mesh/atlas references, then bind the owned template. */
    bool Load(ZPackTables &tables, const Template &data, const ZShaderProgram &program);

    /** Bind stable template data, then run original OnEquip (export 0). */
    void Bind(const Template &data);
    void Equip();
    std::int16_t *VariableResolver(std::uint8_t variable);
    std::int16_t GetAttribute(std::uint32_t index) const;

private:
    friend class CBrother;
    struct Drawing;
    std::unique_ptr<Drawing> m_drawing;
    Template m_templateData;
    CScriptInterpreter m_interpreter;
    std::int16_t m_attributes[kArmorAttributeCount] = {};
};

#endif  // GUN_BROS_RE_GUN_BROS_CARMOR_H
