#include "gun_bros_re/gameplay/CParticleEffectPlayer.h"
#include <algorithm>

struct CParticleEffectPlayer::State {
    const CParticleEffect *effect = nullptr;
    std::shared_ptr<CParticlePool> pool;
    std::vector<std::size_t> particles;
    std::vector<int> remainingMs;
    int ageMs = 0;
    bool looping = true;
    bool stoppedSpawning = false;
    bool done = true;
    float x = 0, y = 0, z = 0, angle = 0;

    ~State() { ReleaseParticles(); }
    void ReleaseParticles() {
        for (const auto index : particles) { pool->Release(index); }
        particles.clear();
    }
    void AdvanceParticles(int deltaMs, std::uint32_t &randomState) {
        for (std::size_t index = 0; index < particles.size();) {
            auto &particle = pool->Get(particles[index]);
            particle.Update(effect->GetEmitters()[particle.emitterIndex], deltaMs, randomState);
            if (particle.IsDone()) {
                pool->Release(particles[index]);
                particles.erase(particles.begin() + index);
            } else { ++index; }
        }
    }
    /** Create one live particle from an emitter's pattern and velocity. */
    void UpdateEmitters(int deltaMs, int previousMs, std::uint32_t &randomState) {
        const auto &emitters = effect->GetEmitters();
        for (std::size_t index = 0; index < emitters.size(); ++index) {
            const auto &emitter = emitters[index];
            const int start = static_cast<int>(emitter.startSeconds * 1000);
            const int end = static_cast<int>(emitter.endSeconds * 1000);
            const bool continuous = start == -1000 && end == -1000;
            if (!continuous) {
                // Preserve the original previous-time window tests. In
                // particular, crossing only the start waits until next Update.
                if (start >= previousMs && end < ageMs) { deltaMs = end - start; }
                else if (start <= previousMs && end > previousMs) {
                    if (end < ageMs) { deltaMs = end - previousMs; }
                } else { continue; }
            }
            remainingMs[index] -= deltaMs;
            while (remainingMs[index] <= 0) {
                // UpdateEmitters draws an interval before spawning, and truncates
                // it to integer milliseconds. There is no host per-frame cap.
                const int interval = static_cast<int>(CParticle::Random(randomState,
                    emitter.intervalMinimumSeconds, emitter.intervalMaximumSeconds) * 1000);
                const auto slot = pool->Acquire();
                if (slot != 0) {
                    auto &particle = pool->Get(slot);
                    particle.emitterIndex = index;
                    particle.z = z;
                    if (particle.Spawn(emitter, randomState, x, y, angle)) {
                        // Original linked list inserts newborn particles at its head.
                        particles.insert(particles.begin(), slot);
                    } else { pool->Release(slot); }
                }
                // UpdateEmitters stops after one spawn when the authored
                // interval rounds to zero; it does not emit 1000 per second.
                // A full pool consumes the interval without queuing a later burst.
                if (interval <= 0) { break; }
                remainingMs[index] += interval;
            }
        }
    }
};

CParticleEffectPlayer::CParticleEffectPlayer() : m_state(std::make_unique<State>()) {}
CParticleEffectPlayer::~CParticleEffectPlayer() = default;
CParticleEffectPlayer::CParticleEffectPlayer(CParticleEffectPlayer &&) noexcept = default;
CParticleEffectPlayer &CParticleEffectPlayer::operator=(CParticleEffectPlayer &&) noexcept = default;

void CParticleEffectPlayer::Init(const CParticleEffect &effect, std::shared_ptr<CParticlePool> pool) {
    m_state->ReleaseParticles();
    m_state->effect = &effect;
    m_state->pool = std::move(pool);
    m_state->remainingMs.assign(effect.GetEmitters().size(), 0);
    Start();
}
void CParticleEffectPlayer::Start() {
    m_state->ReleaseParticles();
    m_state->ageMs = 0;
    m_state->stoppedSpawning = false;
    m_state->done = m_state->effect == nullptr;
    std::fill(m_state->remainingMs.begin(), m_state->remainingMs.end(), 0);
}
void CParticleEffectPlayer::StopSpawning() { m_state->stoppedSpawning = true; }
void CParticleEffectPlayer::Stop() { m_state->ReleaseParticles(); m_state->done = true; }
void CParticleEffectPlayer::SetLooping(bool looping) { m_state->looping = looping; }
void CParticleEffectPlayer::SetPosition(float x, float y, float z, float angle) {
    m_state->x = x; m_state->y = y; m_state->z = z; m_state->angle = angle;
}
bool CParticleEffectPlayer::IsDone() const { return m_state->done; }
std::size_t CParticleEffectPlayer::GetParticleCount() const { return m_state->particles.size(); }
const CParticlePool::Particle &CParticleEffectPlayer::GetParticle(std::size_t index) const {
    return m_state->pool->Get(m_state->particles[index]);
}
void CParticleEffectPlayer::Update(int deltaMs, std::uint32_t &randomState) {
    if (deltaMs <= 0 || m_state->done) { return; }
    State &state = *m_state;
    const int previousMs = state.ageMs;
    state.ageMs += deltaMs;
    const int period = state.effect->GetDurationMs();
    if (!state.stoppedSpawning && !state.looping) {
        if (period > 0 && previousMs >= period) { state.stoppedSpawning = true; }
        else if (period == 0) {
            const int lifetime = state.effect->GetMaximumLifetimeMs();
            if (lifetime > 0 && previousMs >= lifetime) {
                // Non-looping effects without an emission period clear all
                // particles at the maximum emitter lifetime (:131604-131640).
                Stop();
                return;
            }
        }
    }
    if (period == 1) { state.looping = false; }
    // CParticleEffectPlayer::Update :131499 wraps the authored
    // effect period and preserves each emitter's interval remainder.
    // Finite menu sparkles need this in addition to infinite emitters.
    if (state.looping && state.ageMs > period) { state.ageMs -= period; }
    state.AdvanceParticles(deltaMs, randomState);
    if (state.stoppedSpawning) {
        if (state.particles.empty()) { state.done = true; }
        return;
    }
    state.UpdateEmitters(deltaMs, previousMs, randomState);
}
