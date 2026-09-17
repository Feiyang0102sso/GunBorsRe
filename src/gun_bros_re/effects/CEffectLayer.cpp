#include "gun_bros_re/effects/CEffectLayer.h"

bool CEffectLayer::AddParticleEffect(const CParticleEffect &data, float x, float y, float z, float angle) {
    for (auto &effect : m_effects) {
        if (effect.holder) { continue; }
        effect.holder = std::make_unique<ParticleEffectHolder>(data, m_pool, false);
        effect.position = {x, y, z, angle, true};
        return true;
    }
    return false;
}

void CEffectLayer::Update(int deltaMs, std::uint32_t &randomState) {
    for (auto &effect : m_effects) {
        if (!effect.holder) { continue; }
        effect.holder->Update(effect.position, deltaMs, randomState);
        if (effect.holder->IsDone()) { effect.holder.reset(); }
    }
}

void CEffectLayer::Draw(ZSpriteRenderer &sprites, ZEffectColors &colors, const ZEffectProjection &projection) const {
    for (const auto &effect : m_effects) {
        if (effect.holder) { effect.holder->Draw(sprites, colors, projection); }
    }
}

std::size_t CEffectLayer::GetParticleCount() const {
    std::size_t count = 0;
    for (const auto &effect : m_effects) {
        if (effect.holder) { count += effect.holder->GetParticleCount(); }
    }
    return count;
}

std::size_t CEffectLayer::GetEffectCount() const {
    std::size_t count = 0;
    for (const auto &effect : m_effects) { if (effect.holder) { ++count; } }
    return count;
}

void CEffectLayer::Clear() {
    for (auto &effect : m_effects) { effect.holder.reset(); }
}
