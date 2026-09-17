#pragma once
#include "gun_bros_re/gameplay/CParticlePool.h"
#include <memory>

/** Particle ownership, emitter clock and completion, independent of rendering.
 * CParticleEffectPlayer::UpdateEmitters :131393 / Update :131499.
 * Stopping emission does not discard particles owned by a draining host.
 * StopSpawning drains; Stop and Start release all existing particles (:131311).
 */
class CParticleEffectPlayer {
public:
    CParticleEffectPlayer();
    ~CParticleEffectPlayer();
    CParticleEffectPlayer(CParticleEffectPlayer &&) noexcept;
    CParticleEffectPlayer &operator=(CParticleEffectPlayer &&) noexcept;
    CParticleEffectPlayer(const CParticleEffectPlayer &) = delete;
    CParticleEffectPlayer &operator=(const CParticleEffectPlayer &) = delete;

    void Init(const CParticleEffect &effect, std::shared_ptr<CParticlePool> pool);
    void Start();
    void StopSpawning();
    void Stop();
    void SetLooping(bool looping);
    void SetPosition(float x, float y, float z, float angle);
    void Update(int deltaMs, std::uint32_t &randomState);
    bool IsDone() const;
    std::size_t GetParticleCount() const;
    const CParticlePool::Particle &GetParticle(std::size_t index) const;
private:
    struct State;
    std::unique_ptr<State> m_state;
};
