#pragma once
#include "gun_bros_re/effects/CParticlePool.h"
#include <memory>
#include <functional>

class ZSpriteRenderer;

/** Particle ownership, emitter clock and completion, with separate Sprite submission.
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
    void SetWorldSpace(bool worldSpace);
    void GetParticlePosition(std::size_t index, float &x, float &y, float &z) const;
    void SetScale(float scale);
    float GetScale() const;
    void SetZOrderGroup(int group);
    /** SetAnchor :131280 refreshes immediately and before every Update.
     * A missing host actor detaches the anchor and drains existing particles.
     */
    using Anchor = std::function<bool(float &, float &, float &, float &)>;
    void SetAnchor(Anchor anchor);
    void Update(int deltaMs, std::uint32_t &randomState);
    /** Original Draw :131724 traverses this player's particles in list order. */
    void QueueParticles(ZSpriteRenderer &renderer, const float *previewProjection = nullptr) const;
    void QueueParticle(std::size_t index, ZSpriteRenderer &renderer, const float *previewProjection = nullptr) const;
    void Draw(ZSpriteRenderer &renderer, const float *matrix) const;
    bool IsDone() const;
    std::size_t GetParticleCount() const;
    const CParticlePool::Particle &GetParticle(std::size_t index) const;
private:
    struct State;
    std::unique_ptr<State> m_state;
};
