#include "engine/graphics/CMeshCamera.h"
/** Original CBullet::Draw :62998, using shared Windows sprite/mesh adapters. */
#define NOMINMAX
#include "gun_bros_re/gameplay/weapon/CBullet.h"
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
    const float acrossScale = data->GetSpriteScale();
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
    const CGameSpriteGluRef &ref = data->GetSpriteRef();
    int bodyAnimation = animation;
    ZVisualAnimation &animation = sprites.Animation(ref.packHash, ref.archetype, bodyAnimation);
    const float scale = data->GetSpriteScale() * projection.scale;
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
        // :62974..62977 copy the body frame index, not elapsed time, to both caps.
        std::size_t frame = m_spritePlayer.GetStep();
        if (m_boundAnimation != bodyAnimation) { frame = 0; }
        if (frame < start.frames.size()) { m_beamSourceFrame = frame; }
        if (frame < end.frames.size()) { m_beamEndFrame = frame; }
        float startTop, startBottom, endTop, endBottom, bodyTop, bodyBottom;
        FrameBoundsAt(start, m_beamSourceFrame, startTop, startBottom);
        FrameBoundsAt(end, m_beamEndFrame, endTop, endBottom);
        FrameBoundsAt(animation, frame, bodyTop, bodyBottom);
        const int sourceHeight = static_cast<int>(startBottom - startTop);
        const int endHeight = static_cast<int>(endBottom - endTop);
        const int bodyHeight = static_cast<int>(bodyBottom - bodyTop);
        const bool caps = (flags & 0x200) == 0;
        int bodyLength = 0;
        if (scale > 0) { bodyLength = static_cast<int>(length / scale); }
        float origin = 0;
        if (caps) {
            // The source translation remains active for body and end (:62995).
            origin = (sourceHeight * 0.5f + startTop) * scale;
            sprites.AddFrame(start, m_beamSourceFrame, x + dx * origin, y + dy * origin,
                scale, scale, direction + 90, 1);
            bodyLength -= (sourceHeight + endHeight) / 2;
        }
        if (bodyHeight > 0 && bodyLength > 0) {
            const int count = bodyLength / bodyHeight + 1;
            const float stretch = static_cast<float>(bodyLength) / (count * bodyHeight);
            for (int tile = 0; tile < count; ++tile) {
                // Original Draw uses -height*i; it does not normalize the body pivot.
                const float distance = origin + tile * bodyHeight * stretch * scale;
                sprites.AddFrame(animation, frame, x + dx * distance, y + dy * distance,
                    scale, scale * stretch, direction + 90, 1);
            }
        }
        // CBullet::Draw :62998/:63026 always draws both caps. When the
        // remaining body is non-positive, the end uses the source origin.
        if (caps) {
            float endDistance = origin;
            if (bodyLength > 0) {
                const auto offset = static_cast<std::int16_t>(-(endHeight + bodyLength + static_cast<int>(endTop)));
                endDistance -= offset * scale;
            }
            sprites.AddFrame(end, m_beamEndFrame, x + dx * endDistance, y + dy * endDistance,
                scale, scale, direction + 90, 1);
        }
    } else if (!beam) {
        float angle = direction + 90;
        if ((flags & 0x80) != 0) { angle = 0; }
        const float fraction = GetTrajectoryFraction();
        const float phase = GetTrajectoryPhaseScale();
        const float shadowScale = scale + fraction * phase * projection.scale;
        const float alpha = 1 - 0.75f * fraction;
        std::size_t frame = m_spritePlayer.GetStep();
        if (m_boundAnimation != bodyAnimation) { frame = 0; }
        sprites.AddFrame(animation, frame, x, y, shadowScale, shadowScale, angle, alpha);
    }
    DrawLightning(sprites, colors, projection, lightningQuads);
    if (data->GetMesh() != nullptr) {
        const Template::Mesh &part = *data->GetMesh();
        float base[kMatrix4dElements];
        const float apparentScale = data->GetMeshScale() + 25 * GetTrajectoryHeight();
        const float meshScale = apparentScale * part.mesh.GetBounds().inverseExtent * projection.scale * meshCameraScale;
        MeshCameraBuildGameMatrix(sceneMvp, x, y, meshScale, direction + 90, base);
        part.buffer.Draw(program, base, part.texture);
    }
    if (beam) { beamQuads += sprites.Batch().GetQuadCount() - beforeQuads; }
}

int CBullet::GetZOrder(float ownerX, float ownerY, bool hasOwner) const {
    if ((ownerType | 2) == 2 && hasOwner) {
        const float dx = ownerX - x;
        const float dy = ownerY - y;
        if (dx * dx + dy * dy < 10000.0f) { return static_cast<int>(ownerY) - 1; }
    }
    return static_cast<int>(y + 10.0f);
}

void CBullet::PrepareSpriteAnimation(ZSpriteRenderer &sprites) {
    if (m_boundAnimation == animation) { return; }
    const auto &ref = data->GetSpriteRef();
    const auto &frames = sprites.Animation(ref.packHash, ref.archetype, animation);
    m_spriteDurations.clear();
    int previous = 0;
    for (const int end : frames.endMs) {
        m_spriteDurations.push_back(static_cast<std::uint16_t>(end - previous));
        previous = end;
    }
    m_spritePlayer.SetAnimation(&m_spriteDurations);
    m_boundAnimation = animation;
}
