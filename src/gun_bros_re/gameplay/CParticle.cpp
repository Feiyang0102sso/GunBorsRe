#include "gun_bros_re/gameplay/CParticle.h"
#include <cmath>

namespace {
constexpr float kRadians = 3.14159265f / 180.0f;
}

float CParticle::Random(std::uint32_t &state, float minimum, float maximum) {
    state = state * 1664525u + 1013904223u;
    return minimum + (maximum - minimum) * static_cast<float>(state >> 8) / 16777215.0f;
}

void CParticle::SetKey(std::size_t channel, const ZParticleInterpolatorKey &key, std::uint32_t &randomState) {
    Channel &value = m_channels[channel];
    // RefreshInterpolator retains the previous START when the flag is set.
    // Start/end draw separate random values on entry, not one fixed fraction.
    if (!key.keepPreviousStart) { value.start = Random(randomState, key.startMinimum, key.startMaximum); }
    value.end = Random(randomState, key.endMinimum, key.endMaximum);
    value.startMs = key.startMs;
    value.durationMs = key.durationMs;
}

bool CParticle::Spawn(const ZParticleEmitterTemplate &emitter, std::uint32_t &randomState,
    float originX, float originY, float rotation) {
    ageMs = 0;
    lifetimeMs = emitter.GetParticleLifetimeMs();
    animation = emitter.SelectAnimation(Random(randomState, 0, 1));
    angle = rotation;
    x = originX;
    y = originY;
    // Spawn does not invent a lifetime or suppress the birth frame. IsDone
    // removes a zero-lifetime particle when the player next updates it.
    if (animation < 0) { return false; }
    if (emitter.pattern == ZParticleSpawnPattern::Line) {
        // CParticleSpawnPatternLine::GetPosition :132066 returns the delta
        // times one fraction; unlike Rectangle it does not add the first point.
        const float fraction = Random(randomState, 0, 1);
        x += (emitter.patternValues[2] - emitter.patternValues[0]) * fraction;
        y += (emitter.patternValues[3] - emitter.patternValues[1]) * fraction;
    } else if (emitter.pattern == ZParticleSpawnPattern::Rectangle) {
        x += Random(randomState, emitter.patternValues[0], emitter.patternValues[2]);
        y += Random(randomState, emitter.patternValues[1], emitter.patternValues[3]);
    } else {
        const float outer = Random(randomState, emitter.patternValues[2], emitter.patternValues[3]);
        const float width = Random(randomState, emitter.patternValues[4], emitter.patternValues[5]);
        const float radius = Random(randomState, outer - width, outer);
        const float direction = Random(randomState, 0, 360) * kRadians;
        x += emitter.patternValues[0] + std::sin(direction) * radius;
        y += emitter.patternValues[1] - std::cos(direction) * radius;
    }
    if (emitter.velocity == ZParticleSpawnVelocity::Linear) {
        velocityX = Random(randomState, emitter.velocityValues[0], emitter.velocityValues[1]);
        velocityY = Random(randomState, emitter.velocityValues[2], emitter.velocityValues[3]);
    } else {
        const float direction = Random(randomState, emitter.velocityValues[0], emitter.velocityValues[1]) * kRadians;
        const float speed = Random(randomState, emitter.velocityValues[2], emitter.velocityValues[3]);
        velocityX = std::sin(direction) * speed;
        velocityY = std::cos(direction) * speed;
    }
    for (std::size_t channel = 0; channel < m_channels.size(); ++channel) {
        m_channels[channel] = {};
        if (channel < 4 || channel == 5) {
            m_channels[channel].start = 1;
            m_channels[channel].end = 1;
        }
        for (const auto &key : emitter.interpolators[channel]) {
            if (key.startMs == 0) { SetKey(channel, key, randomState); break; }
        }
    }
    return true;
}

float CParticle::Value(std::size_t channel) const {
    const Channel &value = m_channels[channel];
    if (value.durationMs == 0) { return value.end; }
    const float progress = static_cast<float>(ageMs - value.startMs) / value.durationMs;
    return value.start + (value.end - value.start) * progress;
}

void CParticle::Update(const ZParticleEmitterTemplate &emitter, int deltaMs, std::uint32_t &randomState) {
    if (deltaMs <= 0) { return; }
    const unsigned previous = ageMs;
    ageMs += deltaMs;
    for (std::size_t channel = 0; channel < m_channels.size(); ++channel) {
        // The original selects the last key crossed by this update.
        const auto &keys = emitter.interpolators[channel];
        for (auto key = keys.rbegin(); key != keys.rend(); ++key) {
            if (key->startMs > previous && key->startMs <= ageMs) {
                SetKey(channel, *key, randomState);
                break;
            }
        }
    }
    const float seconds = deltaMs * 0.001f;
    velocityX += emitter.accelerationX * seconds;
    velocityY += emitter.accelerationY * seconds;
    const float distance = seconds * Value(5);
    const float cosine = std::cos(angle * kRadians);
    const float sine = std::sin(angle * kRadians);
    // Acceleration stays in emitter coordinates; rotate the displacement.
    x += (velocityX * cosine - velocityY * sine) * distance;
    y += (velocityX * sine + velocityY * cosine) * distance;
}

float CParticle::Rotation(const ZParticleEmitterTemplate &emitter) const {
    float result = angle + Value(4);
    if (emitter.alignToVelocity && (velocityX != 0 || velocityY != 0)) {
        result += std::atan2(velocityY, velocityX) / kRadians + 90;
    }
    return result;
}
