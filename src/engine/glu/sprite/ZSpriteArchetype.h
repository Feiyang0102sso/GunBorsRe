/**
 * @file ZSpriteArchetype.h
 * @brief One SpriteGlu character: its drawing tree and its atlas pages.
 *
 * Port of the archetype half of CSpriteGlu (src/spriteGlu3/spriteGlu.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:57552 (LoadArcheType),
 *            :57348 (LoadTexturePack)
 *
 * The original spreads an archetype across two resources and keeps them in one
 * heap block; this keeps that grouping because the two halves index each other
 * and are useless apart.
 *
 * SPRITEGLU__BINARY_ARCHETYPE_000 + index -- the drawing tree:
 *   uint16 n, { uint8 c, { uint16 spriteMap, int16 dx, int16 dy }[c] }[n]
 *   uint16 n, { uint8 c, { uint16 sprite,    int16 dx, int16 dy }[c] }[n]
 *   uint16 n, { uint8, uint8 c, { uint16 frame, uint16 duration }[c] }[n]
 *   uint8  n, { bitmask, uint8 }[n]
 *
 * BASE_TEXTURE_MAP + index -- where those sprite maps live on the atlases:
 *   uint8  pageCount, uint8 pageFormats[pageCount]
 *   uint16 n, { uint8 page, uint16 x, uint16 y, uint16 w, uint16 h }[n]
 *   uint16 n, uint16 rectIndices[n]
 *
 * Note the read order inside a rect: the page byte comes FIRST on the wire and
 * is stored last. Reading it in field order silently shifts every rectangle.
 */

#ifndef GUN_BROS_RE_SPRITE_GLU_ZSPRITEARCHETYPE_H
#define GUN_BROS_RE_SPRITE_GLU_ZSPRITEARCHETYPE_H

#include "engine/resources/CArrayInputStream.h"
#include "engine/graphics/ZTexture.h"

#include <cstdint>
#include <memory>
#include <vector>

// An animation id, action id or archetype index of 255 means "none".
constexpr std::uint8_t kNoSpriteGluIndex = 255;

/** One piece of a sprite: a sprite map placed at an offset. */
struct ZSpritePart {
    std::uint16_t spriteMapIndex;
    std::int16_t offsetX;
    std::int16_t offsetY;
};

/** One piece of a frame: a sprite placed at an offset. */
struct ZFramePart {
    std::uint16_t spriteIndex;
    std::int16_t offsetX;
    std::int16_t offsetY;
};

/** One step of an animation: show a frame for a while. */
struct ZAnimationStep {
    std::uint16_t frameIndex;
    std::uint16_t durationMs;  // the file stores tens of milliseconds
};

/** One animation. */
struct ZSpriteAnimation {
    std::uint8_t unknown0;  // read by the engine, never used in the draw path
    std::vector<ZAnimationStep> steps;
};

/** A rectangle on one of the archetype's atlas pages. */
struct ZAtlasRect {
    std::uint16_t x;
    std::uint16_t y;
    std::uint16_t width;
    std::uint16_t height;
    std::uint8_t page;
};

/**
 * One archetype: sprites, frames, animations, and the atlas they cut from.
 *
 * The parts vectors are nested because the wire format is, and because the
 * draw walk is frame -> its parts -> sprite -> its parts. Flattening them
 * would buy nothing and cost the shape that makes the walk readable.
 */
class ZSpriteArchetype {
public:
    ZSpriteArchetype();

    /** Read SPRITEGLU__BINARY_ARCHETYPE_000 + index. */
    bool InitDrawingTree(CArrayInputStream &stream, std::uint16_t spriteMapCount);

    /** Read BASE_TEXTURE_MAP + index. */
    bool InitTextureMap(CArrayInputStream &stream);

    /** Take ownership of the decoded atlas pages, in page order. */
    void SetPages(std::vector<std::unique_ptr<ZTexture>> &&pages);

    std::uint32_t GetSpriteCount() const {
        return static_cast<std::uint32_t>(m_sprites.size());
    }
    std::uint32_t GetFrameCount() const {
        return static_cast<std::uint32_t>(m_frames.size());
    }
    std::uint32_t GetAnimationCount() const {
        return static_cast<std::uint32_t>(m_animations.size());
    }
    std::uint32_t GetActionCount() const { return m_actionCount; }
    std::uint32_t GetRectCount() const {
        return static_cast<std::uint32_t>(m_rects.size());
    }
    std::uint32_t GetPageCount() const {
        return static_cast<std::uint32_t>(m_pages.size());
    }
    /** Pages the texture map declares, which is what SetPages must supply. */
    std::uint8_t GetDeclaredPageCount() const { return m_declaredPageCount; }

    const std::vector<ZSpritePart> &GetSpriteParts(std::uint32_t sprite) const {
        return m_sprites[sprite];
    }
    const std::vector<ZFramePart> &GetFrameParts(std::uint32_t frame) const {
        return m_frames[frame];
    }
    const ZSpriteAnimation &GetAnimation(std::uint32_t animation) const {
        return m_animations[animation];
    }

    /**
     * The rectangle an image index names, or null when it does not name one.
     *
     * The index comes from the pack-wide slot table and is turned into a
     * rectangle by THIS archetype's own index table -- the same slot means
     * different rectangles in different archetypes.
     */
    const ZAtlasRect *FindRect(std::uint16_t imageIndex) const;

    /** The page a rectangle sits on, or null when it was never loaded. */
    const ZTexture *GetPage(std::uint8_t page) const;

private:
    std::vector<std::vector<ZSpritePart>> m_sprites;
    std::vector<std::vector<ZFramePart>> m_frames;
    std::vector<ZSpriteAnimation> m_animations;
    std::uint32_t m_actionCount;

    std::uint8_t m_declaredPageCount;
    std::vector<ZAtlasRect> m_rects;
    std::vector<std::uint16_t> m_rectIndices;  // image index -> rect ordinal
    std::vector<std::unique_ptr<ZTexture>> m_pages;
};

#endif  // GUN_BROS_RE_SPRITE_GLU_ZSPRITEARCHETYPE_H
