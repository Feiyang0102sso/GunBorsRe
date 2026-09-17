#pragma once
#include "gun_bros_re/effects/EffectHolder.h"
#include "gun_bros_re/effects/CParticleEffectPlayer.h"

/** Original holder (:294780-294856), retaining its player while it drains. */
class ParticleEffectHolder : public EffectHolder {
public:
    ParticleEffectHolder(const CParticleEffect &effect, std::shared_ptr<CParticlePool> pool, bool looping);
    void Update(const Anchor &anchor, int deltaMs, std::uint32_t &randomState) override;
    void Draw(ZSpriteRenderer &sprites, ZEffectColors &colors, const ZEffectProjection &projection) const override;
    void Stop() override;
    void StopInstant() override;
    bool IsDone() const override { return m_player.IsDone(); }
    bool IsParticle() const override { return true; }
    std::size_t GetParticleCount() const override { return m_player.GetParticleCount(); }
private:
    CParticleEffectPlayer m_player;
};
