#pragma once
#include "gun_bros_re/gameplay/CParticleEffect.h"

/** Shared particle behavior, independent of map/weapon rendering caches.
 * Source: CParticle::Spawn :133114, RefreshInterpolator :133375,
 * Update :133703 and IsDone :133278; entries/particle_effect.bt.
 */
class CParticle {
public:
    bool Spawn(const ZParticleEmitterTemplate &emitter, std::uint32_t &randomState,
        float originX, float originY, float rotation);
    void Update(const ZParticleEmitterTemplate &emitter, int deltaMs, std::uint32_t &randomState);
    /** Resolve one particle channel at an age using the original timed ranges. */
    float Value(std::size_t channel) const;
    float Rotation(const ZParticleEmitterTemplate &emitter) const;
    bool IsDone() const { return ageMs >= lifetimeMs; }

    /** Desktop deterministic stream, not the original global Utility RNG. */
    /** Deterministic local random stream, independent from map animation timing. */
    static float Random(std::uint32_t &state, float minimum, float maximum);

    float x = 0, y = 0, velocityX = 0, velocityY = 0;
    float angle = 0;
    unsigned ageMs = 0, lifetimeMs = 0;
    int animation = 0;
private:
    struct Channel {
        float start = 0, end = 0;
        unsigned startMs = 0, durationMs = 0;
    };
    void SetKey(std::size_t channel, const ZParticleInterpolatorKey &key, std::uint32_t &randomState);
    std::array<Channel, kParticleInterpolatorChannelCount> m_channels{};
};
