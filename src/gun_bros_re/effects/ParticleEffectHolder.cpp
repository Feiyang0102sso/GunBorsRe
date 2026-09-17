#include "gun_bros_re/effects/ParticleEffectHolder.h"
#include "engine/graphics/ZEffectProjection.h"

ParticleEffectHolder::ParticleEffectHolder(const CParticleEffect &effect,
    std::shared_ptr<CParticlePool> pool, bool looping) {
    m_player.Init(effect, std::move(pool));
    // Both CBullet attachment consumers pass world-space=1 (:60917/60971).
    // The independent effect layer has a fixed origin, so this is equivalent
    // to its original relative player plus the layer's draw translation.
    m_player.SetWorldSpace(true);
    m_player.SetLooping(looping);
}

void ParticleEffectHolder::Update(const Anchor &anchor, int deltaMs, std::uint32_t &randomState) {
    m_player.SetPosition(anchor.x, anchor.y, anchor.z, anchor.angle);
    m_player.Update(deltaMs, randomState);
}

void ParticleEffectHolder::Draw(ZSpriteRenderer &sprites, ZEffectColors &colors,
    const ZEffectProjection &projection) const {
    m_player.QueueParticles(sprites, projection.matrix);
}

void ParticleEffectHolder::Stop() {
    m_player.SetLooping(false);
    m_player.StopSpawning();
}

void ParticleEffectHolder::StopInstant() {
    m_player.SetLooping(false);
    m_player.Stop();
}
