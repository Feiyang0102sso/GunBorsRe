/**
 * @file CParticleEffect.cpp
 * @brief Parsing the original particle emitter and interpolator format.
 *
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:130974, :131811, :132332.
 */

#include "gun_bros_re/effects/CParticleEffect.h"

#include <algorithm>
#include <cstring>
#include <cmath>

namespace {

float ReadFloat(CArrayInputStream &stream) {
    const std::uint32_t bits = stream.ReadUInt32();
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

}  // namespace

ZParticleEmitterTemplate::ZParticleEmitterTemplate()
    : archetype(255),
      animationMask(0),
      intervalMinimumSeconds(0.0f),
      intervalMaximumSeconds(0.0f),
      startSeconds(0.0f),
      endSeconds(0.0f),
      alignToVelocity(false),
      accelerationX(0.0f),
      accelerationY(0.0f),
      pattern(ZParticleSpawnPattern::Line),
      velocity(ZParticleSpawnVelocity::Linear) {
    patternValues.fill(0.0f);
    velocityValues.fill(0.0f);
}

std::uint32_t ZParticleEmitterTemplate::GetParticleLifetimeMs() const {
    std::uint32_t lifetime = 0;
    for (std::size_t channel = 0; channel < interpolators.size(); ++channel) {
        const std::vector<ZParticleInterpolatorKey> &keys = interpolators[channel];
        // CParticle::IsDone checks the last key of each channel (:133278).
        if (keys.empty()) { continue; }
        const std::uint32_t end = keys.back().startMs + keys.back().durationMs;
        if (end > lifetime) {
            lifetime = end;
        }
    }
    return lifetime;
}

CParticleEffect::CParticleEffect() : m_spritePackHash(0) {}

std::uint32_t ZParticleEmitterTemplate::GetMaximumLifetimeMs() const {
    std::uint32_t lifetime = 0;
    for (const auto &channel : interpolators) {
        for (const auto &key : channel) { lifetime = std::max(lifetime, key.startMs + key.durationMs); }
    }
    return lifetime;
}

std::size_t ZParticleEmitterTemplate::GetParticleCount() const {
    // CParticleEmitter::GetParticleCount :131970 rounds lifetime/min interval.
    if (intervalMinimumSeconds == 0 || intervalMaximumSeconds == 0) { return 1; }
    const float lifetimeSeconds = GetMaximumLifetimeMs() * 0.001f;
    return static_cast<std::size_t>(std::round(lifetimeSeconds / intervalMinimumSeconds));
}

int CParticleEffect::GetDurationMs() const {
    int duration = 0;
    for (const auto &emitter : m_emitters) {
        duration = std::max(duration, static_cast<int>(emitter.endSeconds * 1000));
    }
    return duration;
}

int CParticleEffect::GetMaximumLifetimeMs() const {
    int lifetime = 0;
    for (const auto &emitter : m_emitters) {
        // Init stores this maximum in seconds; Update truncates it back to ms.
        const float seconds = emitter.GetMaximumLifetimeMs() * 0.001f;
        lifetime = std::max(lifetime, static_cast<int>(seconds * 1000));
    }
    return lifetime;
}

std::size_t CParticleEffect::GetParticleCount() const {
    std::size_t count = 0;
    for (const auto &emitter : m_emitters) { count += emitter.GetParticleCount(); }
    return count;
}

bool CParticleEffect::Init(CArrayInputStream &stream) {
    m_spritePackHash = stream.ReadUInt32();
    const std::uint8_t emitterCount = stream.ReadUInt8();
    m_emitters.resize(emitterCount);
    for (std::uint8_t emitter = 0; emitter < emitterCount; ++emitter) {
        if (!ReadEmitter(stream, m_emitters[emitter])) {
            return false;
        }
    }
    return !stream.Overran();
}

bool CParticleEffect::ReadEmitter(CArrayInputStream &stream,
                                  ZParticleEmitterTemplate &emitter) {
    emitter.archetype = stream.ReadUInt8();
    emitter.animationMask = stream.ReadUInt32();
    emitter.intervalMinimumSeconds = ReadFloat(stream);
    emitter.intervalMaximumSeconds = ReadFloat(stream);
    emitter.startSeconds = ReadFloat(stream);
    emitter.endSeconds = ReadFloat(stream);
    emitter.alignToVelocity = stream.ReadUInt8() != 0;
    emitter.accelerationX = ReadFloat(stream);
    emitter.accelerationY = ReadFloat(stream);

    for (std::size_t channel = 0; channel < emitter.interpolators.size();
         ++channel) {
        const std::uint8_t keyCount = stream.ReadUInt8();
        emitter.interpolators[channel].resize(keyCount);
        for (std::uint8_t key = 0; key < keyCount; ++key) {
            ReadInterpolator(stream, emitter.interpolators[channel][key]);
        }
    }

    const std::uint8_t pattern = stream.ReadUInt8();
    emitter.pattern = static_cast<ZParticleSpawnPattern>(pattern);
    std::size_t patternValueCount = 4;
    if (emitter.pattern == ZParticleSpawnPattern::Circle) {
        patternValueCount = 6;
    }
    for (std::size_t value = 0; value < patternValueCount; ++value) {
        emitter.patternValues[value] = ReadFloat(stream);
    }

    const std::uint8_t velocity = stream.ReadUInt8();
    emitter.velocity = static_cast<ZParticleSpawnVelocity>(velocity);
    for (std::size_t value = 0; value < emitter.velocityValues.size(); ++value) {
        emitter.velocityValues[value] = ReadFloat(stream);
    }

    return !stream.Overran();
}

void CParticleEffect::ReadInterpolator(
    CArrayInputStream &stream, ZParticleInterpolatorKey &interpolator) {
    interpolator.keepPreviousStart = stream.ReadUInt8() != 0;
    interpolator.startMs = stream.ReadUInt32();
    interpolator.durationMs = stream.ReadUInt32();
    interpolator.startMinimum = 0.0f;
    interpolator.startMaximum = 0.0f;
    if (!interpolator.keepPreviousStart) {
        interpolator.startMinimum = ReadFloat(stream);
        interpolator.startMaximum = ReadFloat(stream);
    }
    interpolator.endMinimum = ReadFloat(stream);
    interpolator.endMaximum = ReadFloat(stream);
}

int ZParticleEmitterTemplate::SelectAnimation(float random) const {
    int count = 0;
    for (int bit = 0; bit < 32; ++bit) {
        if ((animationMask & (1u << bit)) != 0) { ++count; }
    }
    if (count == 0) { return -1; }
    int selected = std::min(count - 1, static_cast<int>(random * count));
    for (int bit = 0; bit < 32; ++bit) {
        if ((animationMask & (1u << bit)) == 0) { continue; }
        if (selected == 0) { return bit; }
        --selected;
    }
    return -1;
}
