/**
 * @file CSpriteGlu.h
 * @brief One pack's SpriteGlu system: the shared tables and its archetypes.
 *
 * Port of CSpriteGlu (src/spriteGlu3/spriteGlu.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:57762 (Init),
 *            :57721 (LoadTexturePackData), :57348 (LoadTexturePack)
 *
 * There is one of these per resource pack, reached through Engine::SpriteGlu.
 * It holds the tables every archetype in the pack shares, and hands out the
 * archetypes themselves.
 *
 * SPRITEGLU__BINARY_GLOBAL:
 *   uint8  n, { uint16 len, char name[len] }[n]     -- texture pack names
 *   uint16                                          -- read and discarded
 *   uint16 n, { uint16 imageIndex, uint8 }[n]       -- image slots
 *   uint16 n, { uint16 slot, uint8 transform, uint8 }[n]  -- sprite maps
 *   uint16 n, { uint32 colour, uint16 w, uint16 h, uint8 }[n]  -- primitives
 *   uint8  n, { uint16 c, { uint16, uint16, uint8, uint16, uint16 }[c] }[n]
 *   uint8  archetypeCount
 *
 * TEXTURE_MAP_GLOBAL:
 *   uint16                                          -- read and discarded
 *   uint8  pagesPerArchetype[archetypeCount]
 *
 * The sprite-map field order is the trap: the slot uint16 is written first but
 * lives after the transform byte in memory.
 *
 * An atlas page is addressed as BASE_TEXTURE_PAGE_0 + (pages of every earlier
 * archetype) + page. There is no per-archetype page base in the data; it has
 * to be summed from TEXTURE_MAP_GLOBAL.
 */

#ifndef GUN_BROS_RE_SPRITE_GLU_CSPRITEGLU_H
#define GUN_BROS_RE_SPRITE_GLU_CSPRITEGLU_H

#include "gun_bros/CResPackTOC.h"
#include "sprite_glu/CSpriteGluArchetype.h"

#include <cstdint>
#include <memory>
#include <vector>

// Resources CSpriteGlu::Init reads by name.
extern const char *const kSpriteGluGlobalName;
extern const char *const kSpriteGluArchetypeBaseName;
extern const char *const kTextureMapGlobalName;
extern const char *const kBaseTextureMapName;
extern const char *const kBaseTexturePageName;

// Blend bits on a sprite map, tested in CSpritePlayer::Draw before the blit.
// Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:59160
//   bit 6 -> blend factors (7, 2), which is (GL_SRC_ALPHA, GL_ONE)
//   bit 7 -> blend factors (2, 2), which is (GL_ONE, GL_ONE), plus a colour
//            push this port has no vertex colour to reproduce
// The factor numbering is CRasterizerState_v1_OGLES's, decoded at :342068.
constexpr std::uint8_t kSpriteMapBlendAdditive = 0x40;
constexpr std::uint8_t kSpriteMapBlendAdditiveOpaque = 0x80;

/** Which slot on the atlas a sprite map draws, how it is flipped, how it blends. */
struct SpriteMap {
    std::uint16_t imageSlot;   // index into the image slot table
    std::uint8_t transform;    // flip and rotate bits
    std::uint8_t blendFlags;
};

/**
 * One pack's SpriteGlu tables plus its archetypes.
 *
 * Archetypes are loaded on demand: a pack has up to six, a map usually draws
 * from one, and the big one costs four atlas pages.
 */
class CSpriteGlu {
public:
    CSpriteGlu();
    ~CSpriteGlu();

    CSpriteGlu(const CSpriteGlu &) = delete;
    CSpriteGlu &operator=(const CSpriteGlu &) = delete;

    /** Read the pack's global tables. Does not touch any archetype. */
    bool Init(CResPackTOC &pack);

    bool IsInitialised() const { return m_initialised; }

    std::uint8_t GetArchetypeCount() const { return m_archetypeCount; }
    std::uint16_t GetSpriteMapCount() const {
        return static_cast<std::uint16_t>(m_spriteMaps.size());
    }

    /**
     * The archetype at an index, loading it the first time it is asked for.
     *
     * Needs a GL context, because loading one uploads its atlas pages.
     * Returns null when the index is out of range or the load fails.
     */
    const CSpriteGluArchetype *GetArchetype(std::uint8_t index);

    /**
     * Turn a sprite map index into the image index an archetype can look up.
     *
     * This is the pack-wide half of the walk: sprite map -> image slot ->
     * image index. The archetype turns that last number into a rectangle.
     * Returns false for a sprite map that is a primitive rather than an image.
     */
    bool ResolveImageIndex(std::uint16_t spriteMapIndex,
                           std::uint16_t &imageIndex) const;

    /** The sprite map's transform bits, or 0 when the index is out of range. */
    std::uint8_t GetSpriteMapTransform(std::uint16_t spriteMapIndex) const;

    /** The sprite map's blend bits, or 0 when the index is out of range. */
    std::uint8_t GetSpriteMapBlendFlags(std::uint16_t spriteMapIndex) const;
    const CTexture *GetPrimitiveTexture(std::uint16_t spriteMapIndex) const;

private:
    /** Read SPRITEGLU__BINARY_GLOBAL into the tables. */
    bool ReadGlobalTables(CResPackTOC &pack);

    /** Read TEXTURE_MAP_GLOBAL into m_pagesPerArchetype. */
    bool ReadPageCounts(CResPackTOC &pack);

    /** Decode and upload one archetype's atlas pages. */
    bool LoadPages(std::uint8_t index, CSpriteGluArchetype &archetype);

    CResPackTOC *m_pack;
    bool m_initialised;

    std::uint8_t m_archetypeCount;
    std::vector<std::uint16_t> m_imageSlots;  // image slot -> image index
    std::vector<SpriteMap> m_spriteMaps;
    struct Primitive {
        std::uint32_t color = 0;
        std::uint16_t width = 0, height = 0;
        std::uint8_t type = 0;
        mutable std::unique_ptr<CTexture> texture;
    };
    std::vector<Primitive> m_primitives;

    // How many atlas pages each archetype owns, and where its run starts.
    std::vector<std::uint8_t> m_pagesPerArchetype;

    std::vector<std::unique_ptr<CSpriteGluArchetype>> m_archetypes;
    std::vector<bool> m_archetypeFailed;  // do not retry a bad load every frame
};

#endif  // GUN_BROS_RE_SPRITE_GLU_CSPRITEGLU_H
