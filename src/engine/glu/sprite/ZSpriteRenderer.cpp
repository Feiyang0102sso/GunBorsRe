#include "engine/glu/sprite/ZSpriteRenderer.h"
#include <algorithm>

std::size_t AnimationFrame(const ZVisualAnimation &animation, float ageMs) {
    int time = 0;
    if (animation.durationMs > 0) { time = static_cast<int>(ageMs) % animation.durationMs; }
    std::size_t frame = 0;
    while (frame + 1 < animation.frames.size() && time >= animation.endMs[frame]) { ++frame; }
    return frame;
}

/** The beam tiles the complete frame bounds, including all layered quads. */
void FrameBounds(const ZVisualAnimation &animation, float ageMs, float &top, float &bottom) {
    top = 0; bottom = 0;
    if (animation.frames.empty()) { return; }
    bool first = true;
    for (const ZSpriteQuad &quad : animation.frames[AnimationFrame(animation, ageMs)]) {
        if (first) { top = static_cast<float>(quad.offsetY); bottom = top; first = false; }
        top = std::min(top, static_cast<float>(quad.offsetY));
        bottom = std::max(bottom, static_cast<float>(quad.offsetY + quad.Height()));
    }
}

ZVisualAnimation &ZSpriteRenderer::Animation(std::uint32_t packHash, int archetype, int animation) {
    const std::uint64_t key = (static_cast<std::uint64_t>(packHash) << 32) |
        (static_cast<std::uint32_t>(archetype) << 16) | static_cast<std::uint16_t>(animation);
    auto found = animations.find(key);
    if (found != animations.end()) { return found->second; }
    ZVisualAnimation &out = animations[key];
    auto &glu = spritePacks[packHash];
    if (!glu) {
        glu.reset(new CSpriteGlu());
        if (!glu->Init(*toc.GetPack(toc.GetPackIndexFromHash(packHash)))) { return out; }
    }
    const ZSpriteArchetype *source = glu->GetArchetype(static_cast<std::uint8_t>(archetype));
    if (source == nullptr || animation < 0 || static_cast<std::uint32_t>(animation) >= source->GetAnimationCount()) { return out; }
    CSpriteIterator iterator(*glu, *source);
    const ZSpriteAnimation &sequence = source->GetAnimation(animation);
    for (std::size_t i = 0; i < sequence.steps.size(); ++i) {
        out.frames.emplace_back();
        iterator.Expand(static_cast<std::uint8_t>(animation), static_cast<std::uint32_t>(i), out.frames.back());
        out.durationMs += sequence.steps[i].durationMs;
        out.endMs.push_back(out.durationMs);
    }
    return out;
}

void ZSpriteRenderer::AddSprite(ZVisualAnimation &animation, float ageMs, float x, float y,
                   float scaleX, float scaleY, float angle, float alpha) {
    if (animation.frames.empty()) { return; }
    const std::size_t frame = AnimationFrame(animation, ageMs);
    for (const ZSpriteQuad &quad : animation.frames[frame]) {
        batch.AddTransformedQuad(*quad.page, x + quad.offsetX, y + quad.offsetY,
            static_cast<float>(quad.Width()), static_cast<float>(quad.Height()), quad.source, quad.flipHorizontal,
            quad.flipVertical, quad.blend, x, y, scaleX, scaleY, angle, alpha, quad.rotateTexture);
    }
}

ZSpriteRenderer::ZSpriteRenderer(CResTOCManager &manager, const ZShaderProgram &shader)
    : toc(manager), program(shader) { batch.Create(program); }
void ZSpriteRenderer::Begin() { batch.Begin(); }
void ZSpriteRenderer::Draw(const float *matrix) { batch.Upload(); batch.Draw(program, matrix); }
