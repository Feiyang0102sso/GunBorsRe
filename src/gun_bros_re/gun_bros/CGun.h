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
 * The six tables are per-tier stats. The original keeps only the first three
 * entries of each and floors three of the tables at 100; that is upgrade
 * logic, not parsing, so it stays out of here until M5 needs it.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CGUN_H
#define GUN_BROS_RE_GUN_BROS_CGUN_H

#include "engine/CArrayInputStream.h"
#include "glu_script/CScript.h"
#include "gun_bros/CGameAssetRef.h"
#include "gun_bros/CMoveSetMesh.h"

#include <cstdint>
#include <vector>

// Stat tables in a gun template, all read the same way.
constexpr std::uint32_t kGunStatTableCount = 6;

class CGun {
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
};

#endif  // GUN_BROS_RE_GUN_BROS_CGUN_H
