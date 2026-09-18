#pragma once
/** Cached resources and transient CMap effects; original map.cpp and particleEffectPlayer.cpp. */
#include "gun_bros_re/gameplay/map/CMapResources.h"
namespace MapDetail {
bool EnsureParticleEffectVisual(CResTOCManager &tocManager, CMap &loaded,
                                const ZScriptResourceRef &reference,
                                std::uint64_t &visualKey);
void StartParticleEffect(CMap &loaded, std::uint64_t visualKey,
                         float x, float y, int zOrderGroup,
                         std::uint32_t randomSalt);
void AdvanceParticleEffects(CMap &loaded, std::uint16_t deltaMs);
std::size_t ParticleAnimationStep(const CProp::Animation &animation, float ageMs);
void AddParticleQuads(const CMap &loaded, ZQuadBatch &batch,
                      int minimumZGroup, int maximumZGroup);
}
