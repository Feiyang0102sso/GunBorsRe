#include "gun_bros_re/effects/CParticleEffectPlayer.h"
#include <algorithm>
#include "engine/glu/sprite/ZSpriteRenderer.h"
#include "engine/graphics/ZEffectProjection.h"

struct CParticleEffectPlayer::State {
    const CParticleEffect *effect = nullptr;
    std::shared_ptr<CParticlePool> pool;
    std::vector<std::size_t> particles;
    std::vector<int> remainingMs;
    int ageMs = 0;
    bool looping = true;
    bool stoppedSpawning = false;
    bool done = true;
    bool worldSpace = false; // Constructor :131272; AddEffect explicitly changes this.
    float x = 0, y = 0, z = 0, angle = 0;
    float scale = 1;
    int zOrderGroup = 3;
    Anchor anchor;

    void RefreshAnchor() {
        if (anchor && !anchor(x, y, z, angle)) {
            anchor = {};
            stoppedSpawning = true;
        }
    }

    ~State() { ReleaseParticles(); }
    void ReleaseParticles() {
        for (const auto index : particles) { pool->Release(index); }
        particles.clear();
    }
    void AdvanceParticles(int deltaMs, std::uint32_t &randomState) {
        for (std::size_t index = 0; index < particles.size();) {
            auto &particle = pool->Get(particles[index]);
            // Update :133782 reads the current player angle for every particle.
            particle.angle = angle;
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
                    float originX = 0, originY = 0;
                    particle.z = 0;
                    if (worldSpace) { originX = x; originY = y; particle.z = z; }
                    // CParticle::Spawn :133143 captures the player's group.
                    particle.zOrderGroup = zOrderGroup;
                    if (particle.Spawn(emitter, randomState, originX, originY, angle)) {
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
    m_state->anchor = {};
    m_state->scale = 1;
    m_state->zOrderGroup = 3;
    SetPosition(0, 0, 0, 0);
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
void CParticleEffectPlayer::SetWorldSpace(bool worldSpace) { m_state->worldSpace = worldSpace; }
void CParticleEffectPlayer::SetScale(float scale) { m_state->scale = scale; }
float CParticleEffectPlayer::GetScale() const { return m_state->scale; }
void CParticleEffectPlayer::SetZOrderGroup(int group) { m_state->zOrderGroup = group; }
void CParticleEffectPlayer::SetPosition(float x, float y, float z, float angle) {
    m_state->x = x; m_state->y = y; m_state->z = z; m_state->angle = angle;
}
void CParticleEffectPlayer::SetAnchor(Anchor anchor) {
    m_state->anchor = std::move(anchor);
    m_state->RefreshAnchor();
}
bool CParticleEffectPlayer::IsDone() const { return m_state->done; }
std::size_t CParticleEffectPlayer::GetParticleCount() const { return m_state->particles.size(); }
const CParticlePool::Particle &CParticleEffectPlayer::GetParticle(std::size_t index) const {
    return m_state->pool->Get(m_state->particles[index]);
}
void CParticleEffectPlayer::GetParticlePosition(std::size_t index, float &x, float &y, float &z) const {
    const auto &particle = GetParticle(index);
    x = particle.x; y = particle.y; z = particle.z;
    // Draw :133524 adds the player origin only for relative particles.
    if (!m_state->worldSpace) { x += m_state->x; y += m_state->y; z += m_state->z; }
}
void CParticleEffectPlayer::Update(int deltaMs, std::uint32_t &randomState) {
    if (deltaMs <= 0 || m_state->done) { return; }
    State &state = *m_state;
    state.RefreshAnchor();
    const int previousMs = state.ageMs;
    state.ageMs += deltaMs;
    // CParticleEffect::Init :131032 derives the period from emitter end times.
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

void CParticleEffectPlayer::QueueParticles(ZSpriteRenderer &renderer, const float *previewProjection) const {
    for (std::size_t index = 0; index < GetParticleCount(); ++index) {
        QueueParticle(index, renderer, previewProjection);
    }
}

void CParticleEffectPlayer::QueueParticle(std::size_t index, ZSpriteRenderer &renderer, const float *previewProjection) const {
    const ZEffectProjection projection(previewProjection);
    const auto &particle = GetParticle(index);
    const auto &emitter = m_state->effect->GetEmitters()[particle.emitterIndex];
    auto &animation = renderer.Animation(m_state->effect->GetSpritePackHash(), emitter.archetype, particle.animation);
    // CParticle::Draw :133466: channels 0/1 multiply uniform scale 2.
    // CParticle::Draw :133554 multiplies channel 2 by the player's scale.
    const float uniformScale = particle.Value(2) * m_state->scale * projection.scale;
    const float rotation = particle.Rotation(emitter) - particle.angle + m_state->angle;
    float angle = projection.Direction(rotation - 90) + 90;
    float x = 0, y = 0, z = 0;
    GetParticlePosition(index, x, y, z);
    projection.Position(x, y, z);
    renderer.AddSprite(animation, static_cast<float>(particle.ageMs), x, y,
        particle.Value(0) * uniformScale, particle.Value(1) * uniformScale,
        angle, std::clamp(particle.Value(3), 0.0f, 1.0f));
}

void CParticleEffectPlayer::Draw(ZSpriteRenderer &renderer, const float *matrix) const {
    glDisable(GL_DEPTH_TEST);
    renderer.Begin();
    QueueParticles(renderer);
    renderer.Draw(matrix);
}
