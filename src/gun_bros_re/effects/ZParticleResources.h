#pragma once
/** BIG particle template cache. Players and their pools belong to callers. */
#include "gun_bros_re/effects/CParticleEffect.h"
#include "gun_bros_re/data/objects/CGunBros.h"
#include "gun_bros_re/data/objects/CGameAssetRef.h"
#include <map>

class ZParticleResources {
public:
    explicit ZParticleResources(CGunBros &tables) : m_tables(tables) {}
    const CParticleEffect *Get(const GameObjectRef &resource);
private:
    CGunBros &m_tables;
    std::map<std::uint64_t, CParticleEffect> m_effects;
};
