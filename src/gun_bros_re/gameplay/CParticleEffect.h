/**
 * @file CParticleEffect.h
 * @brief Particle-effect templates embedded in Section 12 resources.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CPARTICLEEFFECT_H
#define GUN_BROS_RE_GUN_BROS_CPARTICLEEFFECT_H

#include "engine/resources/CArrayInputStream.h"

#include <array>
#include <cstdint>
#include <vector>

constexpr std::size_t kParticleInterpolatorChannelCount = 8;

/** One timed range used by a particle property such as scale or opacity. */
struct ZParticleInterpolatorKey {
    bool keepPreviousStart;
    std::uint32_t startMs;
    std::uint32_t durationMs;
    float startMinimum;
    float startMaximum;
    float endMinimum;
    float endMaximum;
};

enum class ZParticleSpawnPattern : std::uint8_t {
    Line,
    Rectangle,
    Circle,
};

enum class ZParticleSpawnVelocity : std::uint8_t {
    Linear,
    Radial,
};

/** One emitter from a CParticleEffect template. */
struct ZParticleEmitterTemplate {
    std::uint8_t archetype;
    std::uint32_t animationMask;
    float intervalMinimumSeconds;
    float intervalMaximumSeconds;
    float startSeconds;
    float endSeconds;
    bool alignToVelocity;
    float accelerationX;
    float accelerationY;
    std::array<std::vector<ZParticleInterpolatorKey>,
               kParticleInterpolatorChannelCount>
        interpolators;
    ZParticleSpawnPattern pattern;
    std::array<float, 6> patternValues;
    ZParticleSpawnVelocity velocity;
    std::array<float, 4> velocityValues;

    ZParticleEmitterTemplate();

    /** Latest interpolator end, which is the lifetime of one particle. */
    std::uint32_t GetParticleLifetimeMs() const;
    /** Utility::RandomBit chooses one set bit, which is an animation ordinal. */
    int SelectAnimation(float random) const;
};

/** A complete Section 12 particle effect. */
class CParticleEffect {
public:
    CParticleEffect();

    bool Init(CArrayInputStream &stream);

    std::uint32_t GetSpritePackHash() const { return m_spritePackHash; }
    const std::vector<ZParticleEmitterTemplate> &GetEmitters() const {
        return m_emitters;
    }

private:
    bool ReadEmitter(CArrayInputStream &stream, ZParticleEmitterTemplate &emitter);
    void ReadInterpolator(CArrayInputStream &stream,
                          ZParticleInterpolatorKey &interpolator);

    std::uint32_t m_spritePackHash;
    std::vector<ZParticleEmitterTemplate> m_emitters;
};

#endif  // GUN_BROS_RE_GUN_BROS_CPARTICLEEFFECT_H
