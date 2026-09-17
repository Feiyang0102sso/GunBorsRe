#pragma once
/** BIG particle template cache. Players and their pools belong to callers. */
#include "gun_bros_re/effects/CParticleEffect.h"
#include "gun_bros_re/data/ZPackTables.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include <map>

class ZParticleResources {
public:
    explicit ZParticleResources(ZPackTables &tables) : m_tables(tables) {}
    const CParticleEffect *Get(const GameObjectRef &resource);
private:
    ZPackTables &m_tables;
    std::map<std::uint64_t, CParticleEffect> m_effects;
};
