/** CSpritePlayer::Draw :59035 and sprite_archetype.bt; Windows batched drawing. */
#include "engine/glu/sprite/CSpritePlayer.h"
#include "engine/glu/sprite/CSpriteIterator.h"

struct CSpritePlayer::DrawData {
    std::vector<std::uint16_t> durations;
    std::vector<std::vector<ZSpriteQuad>> frames;
};

bool CSpritePlayer::Init(CSpriteGlu &glu, std::uint8_t archetypeIndex, std::uint8_t animation) {
    const auto *archetype = glu.GetArchetype(archetypeIndex);
    if (archetype == nullptr || archetype->GetAnimationCount() == 0) { return false; }
    // Original SetAnimation :58861, also used by beam source/end players.
    if (animation >= archetype->GetAnimationCount()) {
        animation = static_cast<std::uint8_t>(archetype->GetAnimationCount() - 1);
    }
    const auto &steps = archetype->GetAnimation(animation).steps;
    if (steps.empty()) { return false; }
    auto data = std::make_shared<DrawData>();
    CSpriteIterator iterator(glu, *archetype);
    data->frames.resize(steps.size());
    for (unsigned index = 0; index < steps.size(); ++index) {
        data->durations.push_back(steps[index].durationMs);
        if (!iterator.Expand(animation, index, data->frames[index])) { return false; }
    }
    if (iterator.GetSkippedPartCount() != 0 || iterator.GetUnsupportedTransformCount() != 0) { return false; }
    // SetAnimation retains the existing clock, including its overshoot behavior.
    SetAnimation(&data->durations);
    m_drawData = std::move(data);
    return true;
}

void CSpritePlayer::Draw(ZQuadBatch &batch, float x, float y, float scale) const {
    if (m_drawData == nullptr) { return; }
    for (const ZSpriteQuad &quad : m_drawData->frames[GetStep()]) {
        batch.AddTransformedQuad(*quad.page, x + quad.offsetX * scale,
            y + quad.offsetY * scale, quad.Width() * scale, quad.Height() * scale,
            quad.source, quad.flipHorizontal, quad.flipVertical, quad.blend, 0, 0, 1, 1, 0, 1, quad.rotateTexture);
    }
}
