#pragma once
#include "gun_bros_re/effects/ParticleEffectHolder.h"
#include <array>

/** Original transient particle layer (:66802); separate from CParticleSystem. */
class CEffectLayer {
public:
    void SetPool(std::shared_ptr<CParticlePool> pool) { m_pool = std::move(pool); }
    bool AddParticleEffect(const CParticleEffect &data, float x, float y, float z, float angle);
    void Update(int deltaMs, std::uint32_t &randomState);
    void Draw(ZSpriteRenderer &sprites, ZEffectColors &colors, const ZEffectProjection &projection) const;
    void Clear();
    std::size_t GetParticleCount() const;
    std::size_t GetEffectCount() const;
private:
    struct ParticleEffect {
        std::unique_ptr<ParticleEffectHolder> holder;
        EffectHolder::Anchor position;
    };
    std::shared_ptr<CParticlePool> m_pool;
    // AddParticleEffect :66852 scans its own twenty slots, not the map system.
    std::array<ParticleEffect, 20> m_effects;
};
