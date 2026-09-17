#include "gun_bros_re/gameplay/CParticlePool.h"
#include <cassert>

CParticlePool::CParticlePool(std::size_t count) : m_particles(count) {
    for (std::size_t index = 1; index < count; ++index) { m_free.push_back(index); }
}
std::size_t CParticlePool::Acquire() {
    if (m_free.empty()) { return 0; }
    const std::size_t index = m_free.back();
    m_free.pop_back();
    return index;
}
void CParticlePool::Release(std::size_t index) {
    assert(index > 0 && index < m_particles.size());
    m_free.push_back(index);
}
