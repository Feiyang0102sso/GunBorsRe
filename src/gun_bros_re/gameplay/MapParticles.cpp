#include "gun_bros_re/gameplay/MapWorldInternal.h"
using namespace MapDetail;

namespace MapDetail {

/** Parse and expand one original particle template the first time it is used. */
bool EnsureParticleEffectVisual(CResTOCManager &tocManager, LoadedMap &loaded,
                                const ScriptResourceRef &reference,
                                std::uint64_t &visualKey) {
    visualKey = AssetKey(reference.packHash, reference.resourceId);
    if (loaded.particleEffects.find(visualKey) != loaded.particleEffects.end()) {
        return true;
    }

    std::vector<std::uint8_t> payload;
    if (!ReadSectionResource(tocManager, loaded, reference.packHash,
                             GameSection::ParticleEffect,
                             reference.resourceId, payload)) {
        return false;
    }

    ParticleEffectVisual visual;
    CArrayInputStream stream(payload);
    if (!visual.effect.Init(stream)) {
        std::printf("[particle] effect %08X/%u could not be parsed\n",
                    reference.packHash, reference.resourceId);
        return false;
    }

    const int spritePackIndex = tocManager.GetPackIndexFromHash(
        visual.effect.GetSpritePackHash());
    PackResources *spritePack = GetPackResources(tocManager, loaded,
                                                  spritePackIndex);
    if (spritePack == nullptr || !spritePack->spriteGluReady) {
        return false;
    }

    const std::vector<ParticleEmitterTemplate> &emitters =
        visual.effect.GetEmitters();
    visual.emitters.resize(emitters.size());
    for (std::size_t emitterIndex = 0; emitterIndex < emitters.size();
         ++emitterIndex) {
        const CSpriteGluArchetype *archetype =
            spritePack->spriteGlu.GetArchetype(emitters[emitterIndex].archetype);
        if (archetype == nullptr) {
            continue;
        }

        CSpriteIterator iterator(spritePack->spriteGlu, *archetype);
        for (int animation = 0; animation < 32; ++animation) {
            if ((emitters[emitterIndex].animationMask & (1u << animation)) != 0) {
                ExpandSlot(iterator, *archetype, static_cast<std::uint8_t>(animation),
                           visual.emitters[emitterIndex].animations[animation]);
            }
        }
    }

    loaded.particleEffects.insert(std::make_pair(visualKey, visual));
    return true;
}

/** Deterministic local random stream, independent from map animation timing. */
float NextParticleRandom(std::uint32_t &state) {
    state = state * 1664525u + 1013904223u;
    const std::uint32_t fraction = (state >> 8) & 0x00FFFFFFu;
    return static_cast<float>(fraction) / 16777215.0f;
}

float ParticleRandomRange(std::uint32_t &state, float minimum, float maximum) {
    return minimum + (maximum - minimum) * NextParticleRandom(state);
}

float ParticleRangeAt(float minimum, float maximum, float randomValue) {
    return minimum + (maximum - minimum) * randomValue;
}

/** Queue one cached effect at a prop's world position. */
void StartParticleEffect(LoadedMap &loaded, std::uint64_t visualKey,
                         float x, float y, int zOrderGroup,
                         std::uint32_t randomSalt) {
    const std::map<std::uint64_t, ParticleEffectVisual>::const_iterator found =
        loaded.particleEffects.find(visualKey);
    if (found == loaded.particleEffects.end()) {
        return;
    }

    ActiveParticleEffect active;
    active.visualKey = visualKey;
    active.x = x;
    active.y = y;
    active.zOrderGroup = zOrderGroup;
    active.ageMs = 0.0f;
    active.randomState = static_cast<std::uint32_t>(visualKey) ^ randomSalt;

    const std::vector<ParticleEmitterTemplate> &emitters =
        found->second.effect.GetEmitters();
    active.nextSpawnMs.resize(emitters.size());
    for (std::size_t emitter = 0; emitter < emitters.size(); ++emitter) {
        float startMs = emitters[emitter].startSeconds * kSecondsToMilliseconds;
        if (startMs < 0.0f) {
            startMs = 0.0f;
        }
        active.nextSpawnMs[emitter] = startMs;
        active.randomState ^= static_cast<std::uint32_t>(emitter * 7919u);
    }
    loaded.activeParticleEffects.push_back(active);
}

/** Original script resource indices and z groups for one state transition. */
void StartTransitionParticlesForProp(CResTOCManager &tocManager,
                                     LoadedMap &loaded,
                                     const PlacedProp &prop,
                                     std::uint8_t state,
                                     std::uint32_t randomSalt) {
    int firstResource = -1;
    int firstZGroup = kZGroupNormal;
    int secondResource = -1;
    int secondZGroup = kZGroupNormal;

    const InteractivePropKind kind = prop.sprite->interactiveKind;
    if (kind == InteractivePropKind::Cover && state >= 1 && state <= 3) {
        firstResource = 3;
        firstZGroup = 5;
        secondResource = 1;
        secondZGroup = 2;
    } else if (kind == InteractivePropKind::Barrel && state == 3) {
        firstResource = 0;
        secondResource = 1;
    } else if (kind == InteractivePropKind::Spire && state == 1) {
        firstResource = 0;
    } else if (kind == InteractivePropKind::Spire && state == 2) {
        firstResource = 1;
    }

    const int resources[2] = {firstResource, secondResource};
    const int zGroups[2] = {firstZGroup, secondZGroup};
    for (std::size_t cue = 0; cue < 2; ++cue) {
        const int resourceIndex = resources[cue];
        if (resourceIndex < 0 ||
            resourceIndex >=
                static_cast<int>(prop.sprite->transitionResources.size())) {
            continue;
        }

        const ScriptResourceRef &reference =
            prop.sprite->transitionResources[resourceIndex];
        std::uint64_t visualKey = 0;
        if (!EnsureParticleEffectVisual(tocManager, loaded, reference,
                                        visualKey)) {
            continue;
        }
        StartParticleEffect(loaded, visualKey, prop.x, prop.y, zGroups[cue],
                            randomSalt + static_cast<std::uint32_t>(cue));
    }
}

/** Start each matching prop's script-authored particle cues. */
void StartTransitionParticles(CResTOCManager &tocManager, LoadedMap &loaded,
                              InteractivePropKind kind, std::uint8_t state) {
    for (std::size_t i = 0; i < loaded.props.size(); ++i) {
        const PlacedProp &prop = loaded.props[i];
        if (prop.sprite->interactiveKind != kind) {
            continue;
        }
        StartTransitionParticlesForProp(tocManager, loaded, prop, state,
                                        static_cast<std::uint32_t>(i * 2654435761u));
    }
}

/** Create one live particle from an emitter's pattern and velocity. */
void SpawnParticle(ActiveParticleEffect &active,
                   const ParticleEmitterTemplate &emitter,
                   std::uint32_t emitterIndex) {
    if (active.particles.size() >= kMaximumParticlesPerEffect) {
        return;
    }

    LiveParticle particle;
    particle.emitterIndex = emitterIndex;
    const int animation = emitter.SelectAnimation(NextParticleRandom(active.randomState));
    if (animation < 0) { return; }
    particle.animationIndex = static_cast<std::uint8_t>(animation);
    particle.x = active.x;
    particle.y = active.y;
    particle.velocityX = 0.0f;
    particle.velocityY = 0.0f;
    particle.ageMs = 0.0f;
    particle.lifetimeMs = static_cast<float>(emitter.GetParticleLifetimeMs());
    if (particle.lifetimeMs <= 0.0f) {
        particle.lifetimeMs = kParticleFallbackLifetimeMs;
    }
    for (std::size_t channel = 0; channel < particle.randomValues.size();
         ++channel) {
        particle.randomValues[channel] = NextParticleRandom(active.randomState);
    }

    if (emitter.pattern == ParticleSpawnPattern::Line) {
        const float distance = NextParticleRandom(active.randomState);
        particle.x += emitter.patternValues[0] +
                      (emitter.patternValues[2] - emitter.patternValues[0]) *
                          distance;
        particle.y += emitter.patternValues[1] +
                      (emitter.patternValues[3] - emitter.patternValues[1]) *
                          distance;
    } else if (emitter.pattern == ParticleSpawnPattern::Rectangle) {
        particle.x += ParticleRandomRange(active.randomState,
                                          emitter.patternValues[0],
                                          emitter.patternValues[2]);
        particle.y += ParticleRandomRange(active.randomState,
                                          emitter.patternValues[1],
                                          emitter.patternValues[3]);
    } else {
        const float outerRadius = ParticleRandomRange(
            active.randomState, emitter.patternValues[2],
            emitter.patternValues[3]);
        const float innerWidth = ParticleRandomRange(
            active.randomState, emitter.patternValues[4],
            emitter.patternValues[5]);
        const float radius = ParticleRandomRange(active.randomState,
                                                 outerRadius - innerWidth,
                                                 outerRadius);
        const float angle = ParticleRandomRange(active.randomState, 0.0f,
                                                360.0f) /
                            kRadiansToDegrees;
        particle.x += emitter.patternValues[0] + std::sin(angle) * radius;
        particle.y += emitter.patternValues[1] - std::cos(angle) * radius;
    }

    if (emitter.velocity == ParticleSpawnVelocity::Linear) {
        particle.velocityX = ParticleRandomRange(
            active.randomState, emitter.velocityValues[0],
            emitter.velocityValues[1]);
        particle.velocityY = ParticleRandomRange(
            active.randomState, emitter.velocityValues[2],
            emitter.velocityValues[3]);
    } else {
        const float direction = ParticleRandomRange(
            active.randomState, emitter.velocityValues[0],
            emitter.velocityValues[1]) /
                                kRadiansToDegrees;
        const float speed = ParticleRandomRange(
            active.randomState, emitter.velocityValues[2],
            emitter.velocityValues[3]);
        particle.velocityX = std::sin(direction) * speed;
        particle.velocityY = std::cos(direction) * speed;
    }

    active.particles.push_back(particle);
}

/** Resolve one particle channel at an age using the original timed ranges. */
float ParticleChannelValue(const ParticleEmitterTemplate &emitter,
                           const LiveParticle &particle,
                           std::size_t channel, float defaultValue) {
    const std::vector<ParticleInterpolatorKey> &keys =
        emitter.interpolators[channel];
    float value = defaultValue;
    const float randomValue = particle.randomValues[channel];
    for (std::size_t keyIndex = 0; keyIndex < keys.size(); ++keyIndex) {
        const ParticleInterpolatorKey &key = keys[keyIndex];
        float startValue = value;
        if (!key.keepPreviousStart) {
            startValue = ParticleRangeAt(key.startMinimum, key.startMaximum,
                                         randomValue);
        }
        const float endValue = ParticleRangeAt(key.endMinimum, key.endMaximum,
                                               randomValue);
        const float startMs = static_cast<float>(key.startMs);
        const float endMs = startMs + static_cast<float>(key.durationMs);
        if (particle.ageMs < startMs) {
            return value;
        }
        if (key.durationMs == 0 || particle.ageMs >= endMs) {
            value = endValue;
            continue;
        }

        const float progress = (particle.ageMs - startMs) /
                               static_cast<float>(key.durationMs);
        return startValue + (endValue - startValue) * progress;
    }
    return value;
}

/** Advance emitters and their live particles. */
void AdvanceParticleEffects(LoadedMap &loaded, std::uint16_t deltaMs) {
    const float elapsedSeconds = static_cast<float>(deltaMs) /
                                 kSecondsToMilliseconds;
    std::size_t activeIndex = 0;
    while (activeIndex < loaded.activeParticleEffects.size()) {
        ActiveParticleEffect &active =
            loaded.activeParticleEffects[activeIndex];
        const std::map<std::uint64_t, ParticleEffectVisual>::const_iterator found =
            loaded.particleEffects.find(active.visualKey);
        if (found == loaded.particleEffects.end()) {
            loaded.activeParticleEffects.erase(
                loaded.activeParticleEffects.begin() + activeIndex);
            continue;
        }

        const std::vector<ParticleEmitterTemplate> &emitters =
            found->second.effect.GetEmitters();
        active.ageMs += static_cast<float>(deltaMs);
        bool futureSpawnExists = false;
        for (std::size_t emitterIndex = 0; emitterIndex < emitters.size();
             ++emitterIndex) {
            const ParticleEmitterTemplate &emitter = emitters[emitterIndex];
            float &nextSpawnMs = active.nextSpawnMs[emitterIndex];
            if (nextSpawnMs < 0.0f) {
                continue;
            }

            float startMs = emitter.startSeconds * kSecondsToMilliseconds;
            if (startMs < 0.0f) {
                startMs = 0.0f;
            }
            const float endMs = emitter.endSeconds * kSecondsToMilliseconds;
            const bool oneShot = endMs <= startMs;
            std::size_t spawned = 0;
            while (nextSpawnMs <= active.ageMs &&
                   spawned < kMaximumSpawnsPerEmitterPerFrame) {
                if (!oneShot && nextSpawnMs > endMs) {
                    nextSpawnMs = -1.0f;
                    break;
                }

                SpawnParticle(active, emitter,
                              static_cast<std::uint32_t>(emitterIndex));
                spawned++;
                if (oneShot) {
                    nextSpawnMs = -1.0f;
                    break;
                }

                float intervalMs = ParticleRandomRange(
                    active.randomState, emitter.intervalMinimumSeconds,
                    emitter.intervalMaximumSeconds) *
                                   kSecondsToMilliseconds;
                if (intervalMs < 1.0f) {
                    // Original UpdateEmitters emits once per update at zero interval.
                    nextSpawnMs = active.ageMs + 1.0f;
                    break;
                }
                nextSpawnMs += intervalMs;
            }
            if (nextSpawnMs >= 0.0f &&
                (oneShot || nextSpawnMs <= endMs)) {
                futureSpawnExists = true;
            }
        }

        std::size_t particleIndex = 0;
        while (particleIndex < active.particles.size()) {
            LiveParticle &particle = active.particles[particleIndex];
            if (particle.emitterIndex >= emitters.size()) {
                active.particles.erase(active.particles.begin() + particleIndex);
                continue;
            }

            const ParticleEmitterTemplate &emitter =
                emitters[particle.emitterIndex];
            particle.ageMs += static_cast<float>(deltaMs);
            if (particle.ageMs >= particle.lifetimeMs) {
                active.particles.erase(active.particles.begin() + particleIndex);
                continue;
            }

            const float speedScale = ParticleChannelValue(
                emitter, particle, 5, 1.0f);
            particle.velocityX += emitter.accelerationX * elapsedSeconds;
            particle.velocityY += emitter.accelerationY * elapsedSeconds;
            particle.x += particle.velocityX * speedScale * elapsedSeconds;
            particle.y += particle.velocityY * speedScale * elapsedSeconds;
            particleIndex++;
        }

        if (!futureSpawnExists && active.particles.empty()) {
            loaded.activeParticleEffects.erase(
                loaded.activeParticleEffects.begin() + activeIndex);
            continue;
        }
        activeIndex++;
    }
}

/** Select the looping sprite step belonging to a particle's current age. */
std::size_t ParticleAnimationStep(const PropSlot &animation, float ageMs) {
    if (animation.stepDurationsMs.empty()) {
        return 0;
    }

    std::uint32_t totalDuration = 0;
    for (std::size_t i = 0; i < animation.stepDurationsMs.size(); ++i) {
        totalDuration += animation.stepDurationsMs[i];
    }
    if (totalDuration == 0) {
        return 0;
    }

    const std::uint32_t wrappedAge =
        static_cast<std::uint32_t>(ageMs) % totalDuration;
    std::uint32_t stepEnd = 0;
    for (std::size_t step = 0; step < animation.stepDurationsMs.size(); ++step) {
        stepEnd += animation.stepDurationsMs[step];
        if (wrappedAge < stepEnd) {
            return step;
        }
    }
    return animation.stepDurationsMs.size() - 1;
}

/** Emit live particle sprites in the requested z-order interval. */
void AddParticleQuads(const LoadedMap &loaded, CQuadBatch &batch,
                      int minimumZGroup, int maximumZGroup) {
    for (std::size_t activeIndex = 0;
         activeIndex < loaded.activeParticleEffects.size(); ++activeIndex) {
        const ActiveParticleEffect &active =
            loaded.activeParticleEffects[activeIndex];
        if (active.zOrderGroup < minimumZGroup ||
            active.zOrderGroup > maximumZGroup) {
            continue;
        }

        const std::map<std::uint64_t, ParticleEffectVisual>::const_iterator found =
            loaded.particleEffects.find(active.visualKey);
        if (found == loaded.particleEffects.end()) {
            continue;
        }
        const ParticleEffectVisual &visual = found->second;
        const std::vector<ParticleEmitterTemplate> &emitters =
            visual.effect.GetEmitters();

        for (std::size_t particleIndex = 0;
             particleIndex < active.particles.size(); ++particleIndex) {
            const LiveParticle &particle = active.particles[particleIndex];
            if (particle.emitterIndex >= emitters.size() ||
                particle.emitterIndex >= visual.emitters.size()) {
                continue;
            }

            const ParticleEmitterTemplate &emitter =
                emitters[particle.emitterIndex];
            const ParticleEmitterVisual &emitterVisual =
                visual.emitters[particle.emitterIndex];
            const PropSlot *animation =
                &emitterVisual.animations[particle.animationIndex];
            if (animation->quadsByStep.empty()) {
                continue;
            }

            const std::size_t step = ParticleAnimationStep(*animation,
                                                           particle.ageMs);
            if (step >= animation->quadsByStep.size()) {
                continue;
            }

            const float scaleX = ParticleChannelValue(
                emitter, particle, 0, 1.0f);
            const float scaleY = ParticleChannelValue(
                emitter, particle, 1, 1.0f);
            const float uniformScale = ParticleChannelValue(
                emitter, particle, 2, 1.0f);
            float alpha = ParticleChannelValue(emitter, particle, 3, 1.0f);
            if (alpha < 0.0f) {
                alpha = 0.0f;
            }
            if (alpha > 1.0f) {
                alpha = 1.0f;
            }
            float rotation = ParticleChannelValue(emitter, particle, 4, 0.0f);
            if (emitter.alignToVelocity) {
                rotation += std::atan2(particle.velocityY,
                                       particle.velocityX) *
                            kRadiansToDegrees;
            }

            const std::vector<SpriteQuad> &quads = animation->quadsByStep[step];
            for (std::size_t quadIndex = 0; quadIndex < quads.size();
                 ++quadIndex) {
                const SpriteQuad &quad = quads[quadIndex];
                batch.AddTransformedQuad(
                    *quad.page,
                    particle.x + static_cast<float>(quad.offsetX),
                    particle.y + static_cast<float>(quad.offsetY),
                    static_cast<float>(quad.Width()),
                    static_cast<float>(quad.Height()), quad.source,
                    quad.flipHorizontal, quad.flipVertical, quad.blend,
                    particle.x, particle.y, scaleX * uniformScale,
                    scaleY * uniformScale, rotation, alpha, quad.rotateTexture);
            }
        }
    }
}

/** The original disables both collision shapes in the destroyed state. */
bool PropHasCollision(const PlacedProp &prop) {
    if (!prop.active) { return false; }
    if (prop.runtime != nullptr) { return !prop.runtime->IsRemoved(); }
    if (prop.sprite->interactiveKind == InteractivePropKind::None) {
        return true;
    }
    if (prop.interactiveState >= prop.sprite->stateCount) {
        return true;
    }
    return prop.sprite->states[prop.interactiveState].collisionEnabled;
}
}
