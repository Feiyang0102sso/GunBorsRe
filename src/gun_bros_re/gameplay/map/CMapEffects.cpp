#include "gun_bros_re/gameplay/map/CMapEffects.h"
#include "engine/graphics/ZQuadBatch.h"
#include "engine/resources/CResTOCManager.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace MapDetail {

/** Parse and expand one original particle template the first time it is used. */
bool EnsureParticleEffectVisual(CResTOCManager &tocManager, CMap &loaded,
                                const ZScriptResourceRef &reference,
                                std::uint64_t &visualKey) {
    visualKey = AssetKey(reference.packHash, reference.resourceId);
    if (loaded.GetResources().particleEffects.find(visualKey) != loaded.GetResources().particleEffects.end()) {
        return true;
    }

    std::vector<std::uint8_t> payload;
    if (!ReadSectionResource(tocManager, loaded, reference.packHash,
                             ZGameSection::ParticleEffect,
                             reference.resourceId, payload)) {
        return false;
    }

    CMap::Resources::ParticleEffectVisual visual;
    CArrayInputStream stream(payload);
    if (!visual.effect.Init(stream)) {
        std::printf("[particle] effect %08X/%u could not be parsed\n",
                    reference.packHash, reference.resourceId);
        return false;
    }

    const int spritePackIndex = tocManager.GetPackIndexFromHash(
        visual.effect.GetSpritePackHash());
    CMap::Resources::Pack *spritePack = GetPackResources(tocManager, loaded,
                                                  spritePackIndex);
    if (spritePack == nullptr || !spritePack->spriteGluReady) {
        return false;
    }

    const std::vector<ZParticleEmitterTemplate> &emitters = visual.effect.GetEmitters();
    visual.emitters.resize(emitters.size());
    for (std::size_t emitterIndex = 0; emitterIndex < emitters.size(); ++emitterIndex) {
        const auto *archetype = spritePack->spriteGlu.GetArchetype(emitters[emitterIndex].archetype);
        if (archetype == nullptr) { continue; }
        CSpriteIterator iterator(spritePack->spriteGlu, *archetype);
        for (int animation = 0; animation < 32; ++animation) {
            if ((emitters[emitterIndex].animationMask & (1u << animation)) != 0) {
                ExpandSlot(iterator, *archetype, static_cast<std::uint8_t>(animation), visual.emitters[emitterIndex].animations[animation]);
            }
        }
    }
    loaded.GetResources().particleEffects.insert(std::make_pair(visualKey, visual));
    return true;
}

/** Queue one cached effect at a prop's world position. */
void StartParticleEffect(CMap &loaded, std::uint64_t visualKey,
    float x, float y, int zOrderGroup, std::uint32_t randomSalt) {
    const auto found = loaded.GetResources().particleEffects.find(visualKey);
    if (found == loaded.GetResources().particleEffects.end()) { return; }
    CMap::Resources::ActiveParticleEffect active;
    active.visualKey = visualKey;
    active.x = x;
    active.y = y;
    active.zOrderGroup = zOrderGroup;
    active.randomState = static_cast<std::uint32_t>(visualKey) ^ randomSalt;
    active.player.Init(found->second.effect, loaded.GetResources().particlePool);
    active.player.SetWorldSpace(true);
    active.player.SetLooping(false);
    active.player.SetPosition(x, y, 0, 0);

    loaded.GetResources().activeParticleEffects.push_back(std::move(active));
}

/** Original script resource indices and z groups for one state transition. */

/** Start each matching prop's script-authored particle cues. */

/** Advance emitters and their live particles. */
// Original UpdateEmitters emits once per update at zero interval.
void AdvanceParticleEffects(CMap &loaded, std::uint16_t deltaMs) {
    for (std::size_t index = 0; index < loaded.GetResources().activeParticleEffects.size();) {
        auto &active = loaded.GetResources().activeParticleEffects[index];
        // Match CParticleEffectPlayer: update existing particles before births.
        active.player.Update(deltaMs, active.randomState);
        if (active.player.IsDone()) {
            loaded.GetResources().activeParticleEffects.erase(loaded.GetResources().activeParticleEffects.begin() + index);
        } else { ++index; }
    }
}

/** Select the looping sprite step belonging to a particle's current age. */
std::size_t ParticleAnimationStep(const CProp::Animation &animation, float ageMs) {
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
void AddParticleQuads(const CMap &loaded, ZQuadBatch &batch,
                      int minimumZGroup, int maximumZGroup) {
    for (std::size_t activeIndex = 0;
         activeIndex < loaded.GetResources().activeParticleEffects.size(); ++activeIndex) {
        const CMap::Resources::ActiveParticleEffect &active =
            loaded.GetResources().activeParticleEffects[activeIndex];
        if (active.zOrderGroup < minimumZGroup ||
            active.zOrderGroup > maximumZGroup) {
            continue;
        }

        const std::map<std::uint64_t, CMap::Resources::ParticleEffectVisual>::const_iterator found =
            loaded.GetResources().particleEffects.find(active.visualKey);
        if (found == loaded.GetResources().particleEffects.end()) {
            continue;
        }
        const CMap::Resources::ParticleEffectVisual &visual = found->second;
        const std::vector<ZParticleEmitterTemplate> &emitters =
            visual.effect.GetEmitters();

        for (std::size_t particleIndex = 0;
             particleIndex < active.player.GetParticleCount(); ++particleIndex) {
            const auto &particle = active.player.GetParticle(particleIndex);
            if (particle.emitterIndex >= emitters.size() ||
                particle.emitterIndex >= visual.emitters.size()) {
                continue;
            }

            const ZParticleEmitterTemplate &emitter =
                emitters[particle.emitterIndex];
            const CMap::Resources::ParticleEmitterVisual &emitterVisual =
                visual.emitters[particle.emitterIndex];
            const CProp::Animation *animation =
                &emitterVisual.animations[particle.animation];
            if (animation->quadsByStep.empty()) {
                continue;
            }

            const std::size_t step = ParticleAnimationStep(*animation,
                                                           particle.ageMs);
            if (step >= animation->quadsByStep.size()) {
                continue;
            }

            const float scaleX = particle.Value(0);
            const float scaleY = particle.Value(1);
            const float uniformScale = particle.Value(2);
            float alpha = particle.Value(3);
            if (alpha < 0.0f) {
                alpha = 0.0f;
            }
            if (alpha > 1.0f) {
                alpha = 1.0f;
            }
            const float rotation = particle.Rotation(emitter);

            const std::vector<ZSpriteQuad> &quads = animation->quadsByStep[step];
            for (std::size_t quadIndex = 0; quadIndex < quads.size();
                 ++quadIndex) {
                const ZSpriteQuad &quad = quads[quadIndex];
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
}
