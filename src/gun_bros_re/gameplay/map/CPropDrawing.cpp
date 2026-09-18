/** CProp::Bind/Draw/GetZOrderGroup, original prop.cpp :124863/:123406/:123506.
 * Sprite frame expansion and GL batch submission are desktop adaptations.
 */
#include "gun_bros_re/gameplay/map/CPropResources.h"
#include "engine/graphics/ZQuadBatch.h"
#include <algorithm>

void CProp::BindResources() {
    if (!resources) { return; }
    hitFlashRemainingMs = 0;
    Bind(resources->data, &resources->durations);
}

const CProp::Animation &CProp::GetAnimationFrames(unsigned slot) const {
    static const Animation empty;
    if (!active || IsRemoved() || !resources || slot >= 3) { return empty; }
    const int animation = GetAnimation(slot);
    if (animation < 0 || animation >= static_cast<int>(resources->animations.size())) { return empty; }
    return resources->animations[animation];
}

int CProp::GetZOrderGroup() const {
    if (!GetAnimationFrames(1).quadsByStep.empty()) { return 3; }
    if (!GetAnimationFrames(0).quadsByStep.empty()) { return 0; }
    if (!GetAnimationFrames(2).quadsByStep.empty()) { return 6; }
    return 3;
}

/** Emit one prop's slot, positioned at the prop and offset by each quad. */
void CProp::DrawSlot(ZQuadBatch &batch, unsigned slot) const {
    const auto &quads = GetCurrentQuads(slot);
    for (std::size_t i = 0; i < quads.size(); ++i) {
        const ZSpriteQuad &quad = quads[i];

        batch.AddQuad(*quad.page, x + static_cast<float>(quad.offsetX),
                      y + static_cast<float>(quad.offsetY),
                      static_cast<float>(quad.source.width),
                      static_cast<float>(quad.source.height), quad.source,
                      quad.flipHorizontal, quad.flipVertical, quad.blend);

        if (hitFlashRemainingMs > 0.0f) {
            const float flashAlpha = hitFlashRemainingMs / 500.0f;
            batch.AddTransformedQuad(
                *quad.page, x + static_cast<float>(quad.offsetX),
                y + static_cast<float>(quad.offsetY),
                static_cast<float>(quad.source.width),
                static_cast<float>(quad.source.height), quad.source,
                quad.flipHorizontal, quad.flipVertical, ZBlendMode::Additive,
                x, y, 1.0f, 1.0f, 0.0f, flashAlpha);
        }
    }
}

void CProp::GetBoundsCenter(float &centerX, float &centerY) const {
    // CProp::GetBounds :123561 unions the active animation bounds of its
    // three SpritePlayers. GetOrientation :191317 tracks the rectangle center.
    float left = 0, top = 0, right = 0, bottom = 0;
    bool hasBounds = false;
    for (unsigned slot = 0; slot < 3; ++slot) {
        for (const auto &frame : GetAnimationFrames(slot).quadsByStep) {
            for (const auto &quad : frame) {
                if (!hasBounds) {
                    left = right = static_cast<float>(quad.offsetX);
                    top = bottom = static_cast<float>(quad.offsetY);
                    hasBounds = true;
                }
                left = std::min(left, float(quad.offsetX));
                top = std::min(top, float(quad.offsetY));
                right = std::max(right, float(quad.offsetX + quad.Width()));
                bottom = std::max(bottom, float(quad.offsetY + quad.Height()));
            }
        }
    }
    centerX = x + left + int(right - left) / 2;
    centerY = y + top + int(bottom - top) / 2;
}

const std::vector<ZSpriteQuad> &CProp::GetCurrentQuads(unsigned slot) const {
    static const std::vector<ZSpriteQuad> empty;
    if (slot >= 3) { return empty; }
    const auto &frames = GetAnimationFrames(slot).quadsByStep;
    const unsigned step = GetPlayer(slot).GetStep();
    if (step >= frames.size()) { return empty; }
    return frames[step];
}
