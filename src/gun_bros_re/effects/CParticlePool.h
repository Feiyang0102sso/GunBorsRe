#pragma once
#include "gun_bros_re/effects/CParticle.h"

/** Fixed storage shared by effect players; source Allocate :92882.
 * The original free-stack cursor starts at count - 1 and zero means empty.
 * Slot zero is therefore reserved, including when the allocation has one slot.
 */
class CParticlePool {
public:
    /** One particle emitted by an active effect. */
    struct Particle : CParticle {
        std::size_t emitterIndex = 0;
        float z = 0;
        int zOrderGroup = 3;
    };
    explicit CParticlePool(std::size_t count);
    std::size_t Acquire();
    void Release(std::size_t index);
    Particle &Get(std::size_t index) { return m_particles[index]; }
    const Particle &Get(std::size_t index) const { return m_particles[index]; }
    std::size_t GetAvailableCount() const { return m_free.size(); }
private:
    std::vector<Particle> m_particles;
    std::vector<std::size_t> m_free;
};
