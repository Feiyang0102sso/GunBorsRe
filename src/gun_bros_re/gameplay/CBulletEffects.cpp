/** CBullet attachments (:60518, 60895, 60941) and removal-pending ownership. */
#include "gun_bros_re/gameplay/CBullet.h"
#include "gun_bros_re/effects/ParticleEffectHolder.h"
#include "gun_bros_re/effects/TrailEffectHolder.h"
#include <cmath>
namespace { constexpr float kRadians = 3.14159265f / 180.0f; }

EffectHolder::Anchor CBullet::EffectAnchor(bool align, bool hit) const {
    EffectHolder::Anchor anchor;
    anchor.x = x; anchor.y = y; anchor.z = z;
    anchor.alive = !removed;
    if (align) { anchor.angle = direction + 90; }
    if (hit) {
        anchor.x += std::cos(direction * kRadians) * length;
        anchor.y += std::sin(direction * kRadians) * length;
    }
    return anchor;
}

void CBullet::AttachParticleEffect(const CParticleEffect &data,
    std::shared_ptr<CParticlePool> pool, bool trail, bool align) {
    if (trail) { StopTrail(); }
    const bool hit = beam && !trail;
    // An attached infinite emitter stays alive between emissions.
    auto holder = std::make_unique<ParticleEffectHolder>(data, std::move(pool), true);
    const auto handle = effects.Attach(std::move(holder), [this, align, hit]() { return EffectAnchor(align, hit); });
    if (trail) { m_trailHandle = handle; }
}

void CBullet::StopTrail() {
    auto *holder = effects.Get(m_trailHandle);
    if (holder != nullptr) { holder->Stop(); }
    m_trailHandle = 0;
}

void CBullet::StopAttachedEffects() {
    if (m_retirementStarted) { return; }
    m_retirementStarted = true;
    // OnRemove detaches the gun count immediately; rendering tails still belong here.
    OnRemove();
    // ActivateRemovalPending :62366 retains attached tails only for non-beams.
    if (beam) { effects.Clear(); return; }
    effects.Stop();
}

void CBullet::ApplyRibbonCue(const ZGunCue &cue) {
    // Native-order attachment is essential when a script fills all four slots.
    // A failed SetRibbonTrail does not silently retry on later update frames.
    if (cue.kind == ZGunCue::Kind::RibbonTrail && m_ribbonHandle == 0) {
        auto holder = std::make_unique<TrailEffectHolder>(cue.ribbon.width, cue.ribbon.capacity, cue.ribbon.intervalMs);
        // SetRibbonTrail does not inherit a color request made before creation.
        m_ribbonHandle = effects.Attach(std::move(holder), [this]() { return EffectAnchor(false, false); });
    }
    if (cue.kind == ZGunCue::Kind::RibbonColor) {
        auto *holder = static_cast<TrailEffectHolder *>(effects.Get(m_ribbonHandle));
        if (holder != nullptr) { holder->SetColor(cue.ribbon.color); }
    }
}

void CBullet::UpdateAttachedEffects(int deltaMs, std::uint32_t &randomState) {
    if (removed) { StopAttachedEffects(); }
    effects.Update(deltaMs, randomState);
}
