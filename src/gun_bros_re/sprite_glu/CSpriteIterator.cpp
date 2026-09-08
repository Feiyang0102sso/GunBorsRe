/**
 * @file CSpriteIterator.cpp
 * @brief Walks one animation frame down to the rectangles it draws.
 */

#include "sprite_glu/CSpriteIterator.h"

namespace {

// Transform bits on a sprite map. CSpriteGlu::FlipTransform XORs these with
// the player's own flip flags, and CSpriteIterator::SetSprite then mirrors the
// part offset on X for bit 1 and on Y for bit 0 -- which is what fixes the two
// bits to those axes. A prop standing on a map has no flip of its own, so the
// sprite map's byte is the whole transform.
// Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:56870, :58200
constexpr std::uint8_t kTransformFlipVertical = 0x01;
constexpr std::uint8_t kTransformFlipHorizontal = 0x02;
constexpr std::uint8_t kTransformRotate = 0x04;

/**
 * Turn a transform byte into the pair of flips CQuadBatch can do.
 *
 * The blit takes a quadrant rather than two flags, but the quadrant is only a
 * re-encoding of the same two bits: the chain from v48 to v51 at :59180 maps
 * 0 -> none, bit 0 -> flip Y, bit 1 -> flip X, both -> both. Bit 2 is the one
 * that does not survive: it takes the branch that rotates the blit by ninety
 * degrees, which an axis-aligned quad cannot express.
 *
 * @return false when the transform rotates, in which case the flips are still
 *         set and the quad is worth drawing unrotated rather than dropping.
 */
bool TransformToFlips(std::uint8_t transform, bool &flipHorizontal,
                      bool &flipVertical) {
    flipHorizontal = (transform & kTransformFlipHorizontal) != 0;
    flipVertical = (transform & kTransformFlipVertical) != 0;

    return (transform & kTransformRotate) == 0;
}

/** Turn a sprite map's blend bits into the mode the batch draws it with. */
BlendMode BlendFlagsToMode(std::uint8_t blendFlags) {
    // The opaque variant also modulates colour, which needs a vertex colour
    // this port's shader does not carry. The blend factors are still right.
    if ((blendFlags & kSpriteMapBlendAdditiveOpaque) != 0) {
        return BlendMode::AdditiveOpaque;
    }
    if ((blendFlags & kSpriteMapBlendAdditive) != 0) {
        return BlendMode::Additive;
    }
    return BlendMode::Alpha;
}

}  // namespace

CSpriteIterator::CSpriteIterator(const CSpriteGlu &glu,
                                 const CSpriteGluArchetype &archetype)
    : m_glu(glu),
      m_archetype(archetype),
      m_skippedParts(0),
      m_unsupportedTransforms(0) {}

bool CSpriteIterator::Expand(std::uint8_t animationIndex, std::uint32_t stepIndex,
                             std::vector<SpriteQuad> &out) {
    // An unused slot, which is how a prop says it has no main or foreground
    // sprite. Not an error; most props leave two of the three empty.
    if (animationIndex == kNoSpriteGluIndex) {
        return true;
    }
    if (animationIndex >= m_archetype.GetAnimationCount()) {
        return false;
    }

    const SpriteAnimation &animation = m_archetype.GetAnimation(animationIndex);
    if (stepIndex >= animation.steps.size()) {
        return false;
    }

    const std::uint16_t frameIndex = animation.steps[stepIndex].frameIndex;
    if (frameIndex >= m_archetype.GetFrameCount()) {
        return false;
    }

    // Layers count down: SetFrame starts at the last part and NextSprite walks
    // backwards, so the last part is drawn first and everything else lands on
    // top of it.
    const std::vector<FramePart> &parts = m_archetype.GetFrameParts(frameIndex);
    for (std::size_t i = parts.size(); i > 0; --i) {
        const FramePart &part = parts[i - 1];
        ExpandSprite(part.spriteIndex, part.offsetX, part.offsetY, out);
    }

    return true;
}

void CSpriteIterator::ExpandSprite(std::uint16_t spriteIndex, std::int32_t offsetX,
                                   std::int32_t offsetY,
                                   std::vector<SpriteQuad> &out) {
    if (spriteIndex >= m_archetype.GetSpriteCount()) {
        m_skippedParts++;
        return;
    }

    // Sprite parts count down too, for the same reason.
    const std::vector<SpritePart> &parts = m_archetype.GetSpriteParts(spriteIndex);
    for (std::size_t i = parts.size(); i > 0; --i) {
        const SpritePart &part = parts[i - 1];

        std::uint16_t imageIndex = 0;
        if (!m_glu.ResolveImageIndex(part.spriteMapIndex, imageIndex)) {
            // A sprite map past the table is a coloured primitive rather than
            // an image. Only pack7 has one, and nothing draws primitives yet.
            m_skippedParts++;
            continue;
        }

        const AtlasRect *rect = m_archetype.FindRect(imageIndex);
        if (rect == nullptr) {
            m_skippedParts++;
            continue;
        }

        const CTexture *page = m_archetype.GetPage(rect->page);
        if (page == nullptr) {
            m_skippedParts++;
            continue;
        }

        SpriteQuad quad;
        quad.page = page;
        quad.source.x = rect->x;
        quad.source.y = rect->y;
        quad.source.width = rect->width;
        quad.source.height = rect->height;
        quad.offsetX = offsetX + part.offsetX;
        quad.offsetY = offsetY + part.offsetY;

        const std::uint8_t transform = m_glu.GetSpriteMapTransform(part.spriteMapIndex);
        if (!TransformToFlips(transform, quad.flipHorizontal, quad.flipVertical)) {
            m_unsupportedTransforms++;
            // Keep the historical count for older axis-aligned consumers.
            // Transformed batches can now reproduce the quarter-turn blit.
            quad.rotateTexture = true;
        }
        quad.blend =
            BlendFlagsToMode(m_glu.GetSpriteMapBlendFlags(part.spriteMapIndex));

        out.push_back(quad);
    }
}
