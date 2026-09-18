#include "engine/graphics/CMeshCamera.h"
/** Original CBullet::Draw :62998, using shared Windows sprite/mesh adapters. */
#define NOMINMAX
#include "gun_bros_re/gameplay/CBullet.h"
#include "gun_bros_re/gameplay/ZBulletResources.h"
#include "gun_bros_re/effects/ZEffectColors.h"
#include "engine/glu/sprite/ZSpriteRenderer.h"
#include "engine/graphics/ZEffectProjection.h"
#include "engine/core/ZMatrix4d.h"
#include <algorithm>
#include <cmath>
namespace {
constexpr float kRadians = 3.14159265f / 180.0f;
/**
 * Whether a beam slot is the source cap rather than the tiled body.
 *
 * Beam sprite sets are authored body / source / end in consecutive slots.
 * CBullet::Draw :62998 walks the tiles towards -Y and centres the source on
 * the muzzle, so body and end frames hang backwards from their origin
 * (bounds span [-h, 0]) while the source frame hangs forwards ([0, +h]).
 * pack5 archetype 0 (player lasers) and 139 (Kraken / mech boss) both follow
 * it, which is what makes the bounds a usable test rather than a guess.
 */
// Historical bounds hypothesis above is rejected by the BIG/Bind/Flow audit.
// Draw must use the authored slot, even when a different frame looks smoother.

/**
 * The slot CBullet::Draw tiles as the beam body.
 *
 * Bind :63647 tiles the slot the sprite ref names and caps it with the
 * next two. Every beam checked so far names its body slot, except pack5
 * BULLET104 -- the pack7 mech boss beam -- whose ref names the source slot
 * (archetype 139, animation 1), so the unpatched trio tiles the muzzle
 * flare and the beam draws as a bead chain instead of a line. This steps
 * back to the body slot when the named one is a source cap. It is a
 * deliberate deviation from the original bytes, written as the authoring
 * rule rather than as a patch on one resource id; drop it to get the
 * original data back verbatim.
 */
// Correction (R03): the historical authoring-rule workaround above has
// been removed. pack5 BULLET104 really binds animation 1 and caps 2/3.

}
void CBullet::DrawLightning(ZSpriteRenderer &sprites, ZEffectColors &colors,
    const ZEffectProjection &projection, std::size_t &lightningQuads) const {
    if (!lightningArc.IsReady() || removed || length <= 0) { return; }
    ZBulletRibbonSettings white;
    white.color = {255, 255, 255, 255}; // CLightningArc constructor :243194.
    const ZTexture *texture = colors.Get(white.color);
    if (texture == nullptr) { return; }
    const float arcLength = std::floor(lightningArc.GetLength());
    const unsigned count = static_cast<unsigned>(length / arcLength) + 1;
    const float alongScale = length / (count * arcLength);
    const float acrossScale = visual->data.GetSpriteScale();
    const float dx = std::cos(direction * kRadians);
    const float dy = std::sin(direction * kRadians);
    // CBullet::Draw :63049: repeat arcs, fit their total length to the ray,
    // and shift each segment's interpolation by one animation frame.
    for (unsigned segment = 0; segment < count; ++segment) {
        auto vertices = lightningArc.Interpolate(segment);
        for (auto &vertex : vertices) {
            const float across = vertex.x * acrossScale;
            const float along = (segment * arcLength + vertex.y) * alongScale;
            vertex.x = x - dy * across + dx * along;
            vertex.y = y + dx * across + dy * along;
            projection.Position(vertex.x, vertex.y, z);
        }
        for (unsigned index = 2; index < vertices.size(); index += 2) {
            const float positions[] = {vertices[index - 2].x, vertices[index - 2].y,
                vertices[index - 1].x, vertices[index - 1].y,
                vertices[index].x, vertices[index].y,
                vertices[index + 1].x, vertices[index + 1].y};
            const float alpha[] = {1, 1, 1, 1};
            sprites.Batch().AddGradientQuad(*texture, positions, alpha);
            ++lightningQuads;
        }
    }
}

void CBullet::DrawProjectile(ZSpriteRenderer &sprites, ZEffectColors &colors, const ZShaderProgram &program,
    const float *sceneMvp, const ZEffectProjection &projection, float meshCameraScale,
    std::size_t &beamQuads, std::size_t &lightningQuads) {
    if (!visible) { return; }
    std::size_t beforeQuads = 0;
    if (beam) { beforeQuads = sprites.Batch().GetQuadCount(); }
    const CGameSpriteGluRef &ref = visual->data.GetSpriteRef();
    int bodyAnimation = animation;
    ZVisualAnimation &animation = sprites.Animation(ref.packHash, ref.archetype, bodyAnimation);
    const float age = static_cast<float>(animationAgeMs);
    const float scale = visual->data.GetSpriteScale() * projection.scale;
    float x = this->x, y = this->y;
    projection.Position(x, y, z);
    const float direction = projection.Direction(this->direction);
    if (beam && (flags & 0x400) == 0) {
        // Beam sprites have body / end / source animations in consecutive slots.
        // Corrected from CBullet::Draw: base+1 is source, base+2 is end.
        ZVisualAnimation &start = sprites.Animation(ref.packHash, ref.archetype, beamSourceAnimation);
        ZVisualAnimation &end = sprites.Animation(ref.packHash, ref.archetype, beamEndAnimation);
        float endX = this->x + std::cos(this->direction * kRadians) * this->length;
        float endY = this->y + std::sin(this->direction * kRadians) * this->length;
        projection.Position(endX, endY, z);
        const float length = std::hypot(endX - x, endY - y);
        const float dx = std::cos(direction * kRadians), dy = std::sin(direction * kRadians);
        float startTop, startBottom, endTop, endBottom, bodyTop, bodyBottom;
        FrameBounds(start, age, startTop, startBottom);
        FrameBounds(end, age, endTop, endBottom);
        FrameBounds(animation, age, bodyTop, bodyBottom);
        float startHalf = (startBottom - startTop) * scale * 0.5f;
        float endHalf = (endBottom - endTop) * scale * 0.5f;
        const bool caps = (flags & 0x200) == 0;
        if (!caps) { startHalf = 0; endHalf = 0; }
        const float bodyLength = std::max(0.0f, length - startHalf - endHalf);
        const float tileHeight = (bodyBottom - bodyTop) * scale;
        if (tileHeight > 0 && bodyLength > 0) {
            const int count = static_cast<int>(bodyLength / tileHeight) + 1;
            const float tileLength = bodyLength / count;
            const float scaleY = tileLength / (bodyBottom - bodyTop);
            for (int tile = 0; tile < count; ++tile) {
                const float distance = startHalf + tile * tileLength + bodyBottom * scaleY;
                sprites.AddSprite(animation, age, x + dx * distance, y + dy * distance,
                                scale, scaleY, direction + 90, 1);
            }
        }
        // CBullet::Draw :62998/:63026 always draws both caps. When the
        // remaining body is non-positive, the end uses the source origin.
        if (caps) {
            const float startOffset = (startTop + startBottom) * scale * 0.5f;
            const float endOffset = (endTop + endBottom) * scale * 0.5f;
            sprites.AddSprite(start, age, x + dx * startOffset, y + dy * startOffset,
                            scale, scale, direction + 90, 1);
            float endPivotX = endX + dx * endOffset;
            float endPivotY = endY + dy * endOffset;
            if (bodyLength <= 0) {
                endPivotX = x + dx * startOffset;
                endPivotY = y + dy * startOffset;
            }
            sprites.AddSprite(end, age, endPivotX, endPivotY,
                            scale, scale, direction + 90, 1);
        }
    } else if (!beam) {
        float angle = direction + 90;
        if ((flags & 0x80) != 0) { angle = 0; }
        const float fraction = GetTrajectoryFraction();
        const float phase = GetTrajectoryPhaseScale();
        const float shadowScale = scale + fraction * phase * projection.scale;
        const float alpha = 1 - 0.75f * fraction;
        sprites.AddSprite(animation, age, x, y, shadowScale, shadowScale, angle, alpha);
    }
    DrawLightning(sprites, colors, projection, lightningQuads);
    if (visual->mesh) {
        ZBulletVisual::Mesh &part = *visual->mesh;
        float base[kMatrix4dElements];
        const float apparentScale = visual->data.GetMeshScale() + 25 * GetTrajectoryHeight();
        const float meshScale = apparentScale * part.mesh.GetBounds().inverseExtent * projection.scale * meshCameraScale;
        MeshCameraBuildGameMatrix(sceneMvp, x, y, meshScale, direction + 90, base);
        part.buffer.Draw(program, base, part.texture);
    }
    if (beam) { beamQuads += sprites.Batch().GetQuadCount() - beforeQuads; }
}
