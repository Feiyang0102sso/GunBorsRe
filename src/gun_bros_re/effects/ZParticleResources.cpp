#include "gun_bros_re/effects/ZParticleResources.h"
#include <cstdio>

const CParticleEffect *ZParticleResources::Get(const GameObjectRef &resource) {
    const auto key = (static_cast<std::uint64_t>(resource.packHash) << 32) | resource.localIndex;
    auto found = m_effects.find(key);
    if (found != m_effects.end()) { return &found->second; }
    std::vector<std::uint8_t> bytes;
    if (!m_tables.ReadSectionResource(resource.packHash, ZGameSection::ParticleEffect, resource.localIndex, bytes)) {
        std::printf("[particles] missing resource %08x:%u\n", resource.packHash, resource.localIndex);
        return nullptr;
    }
    CParticleEffect effect;
    CArrayInputStream stream(bytes);
    if (!effect.Init(stream) || stream.Available() != 0) {
        std::printf("[particles] invalid resource %08x:%u remaining=%u\n", resource.packHash,
            resource.localIndex, static_cast<unsigned>(stream.Available()));
        return nullptr;
    }
    return &m_effects.emplace(key, std::move(effect)).first->second;
}
