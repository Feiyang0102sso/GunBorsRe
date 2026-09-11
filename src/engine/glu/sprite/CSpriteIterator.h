/**
 * @file CSpriteIterator.h
 * @brief Walks one animation frame down to the rectangles it draws.
 *
 * Port of CSpriteIterator (src/spriteGlu3/spriteIterator.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:58283 (SetFrame), :58229 (SetLayer),
 *            :58033 (SetSprite), :58295 (NextSprite), :59035 (CSpritePlayer::Draw)
 *
 * The original is a cursor: SetFrame positions it, Draw blits whatever it is
 * pointing at, NextSprite advances, repeat. That shape exists because the
 * engine draws immediately. This one batches, so the same walk is done in one
 * pass and handed back as a list. The traversal order is copied exactly,
 * including the part that is easy to miss: both levels count DOWN, so the
 * last frame part is drawn first and ends up underneath.
 *
 * The walk, all five levels of it:
 *
 *   animation step -> frame
 *   frame part     -> sprite, at an offset
 *   sprite part    -> sprite map, at a further offset
 *   sprite map     -> image slot -> image index          (pack-wide tables)
 *   image index    -> rectangle on an atlas page         (this archetype)
 */

#ifndef GUN_BROS_RE_SPRITE_GLU_CSPRITEITERATOR_H
#define GUN_BROS_RE_SPRITE_GLU_CSPRITEITERATOR_H

#include "engine/graphics/CQuadBatch.h"
#include "engine/graphics/CTexture.h"
#include "engine/glu/sprite/CSpriteGlu.h"
#include "engine/glu/sprite/CSpriteGluArchetype.h"

#include <cstdint>
#include <vector>

/** One rectangle to draw, positioned relative to the object's own origin. */
struct SpriteQuad {
    const CTexture *page;
    SourceRect source;
    std::int32_t offsetX;
    std::int32_t offsetY;
    bool flipHorizontal;
    bool flipVertical;
    bool rotateTexture = false;
    BlendMode blend;
    int Width() const { if (rotateTexture) { return source.height; } return source.width; }
    int Height() const { if (rotateTexture) { return source.width; } return source.height; }
};

/**
 * Expands frames of one archetype into quads.
 *
 * Holds references, so it must not outlive the glu or the archetype. That is
 * the same lifetime the original assumes -- its cursor holds raw pointers into
 * both -- and the caller builds one per prop draw, inside the load.
 */
class CSpriteIterator {
public:
    CSpriteIterator(const CSpriteGlu &glu, const CSpriteGluArchetype &archetype);

    /**
     * Append every quad of one animation step to `out`.
     *
     * @param animationIndex Animation within the archetype; 255 means the slot
     *                       is unused and nothing is appended.
     * @param stepIndex      Step within the animation, i.e. which frame.
     * @return false when the animation or its frame is out of range.
     */
    bool Expand(std::uint8_t animationIndex, std::uint32_t stepIndex,
                std::vector<SpriteQuad> &out);

    /** Parts dropped because their sprite map resolved to nothing drawable. */
    std::uint32_t GetSkippedPartCount() const { return m_skippedParts; }

    /** Parts drawn without their transform, because it is not a plain flip. */
    std::uint32_t GetUnsupportedTransformCount() const {
        return m_unsupportedTransforms;
    }

private:
    /** Append the quads of one sprite, offset by its frame part. */
    void ExpandSprite(std::uint16_t spriteIndex, std::int32_t offsetX,
                      std::int32_t offsetY, std::vector<SpriteQuad> &out);

    const CSpriteGlu &m_glu;
    const CSpriteGluArchetype &m_archetype;

    std::uint32_t m_skippedParts;
    std::uint32_t m_unsupportedTransforms;
};

#endif  // GUN_BROS_RE_SPRITE_GLU_CSPRITEITERATOR_H
