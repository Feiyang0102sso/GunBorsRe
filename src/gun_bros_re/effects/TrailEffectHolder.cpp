#include "gun_bros_re/effects/TrailEffectHolder.h"
TrailEffectHolder::TrailEffectHolder(float width, unsigned capacity, unsigned intervalMs)
    : m_trail(width, capacity), m_intervalMs(intervalMs) {}
void TrailEffectHolder::Update(const Anchor &anchor, int deltaMs, std::uint32_t &randomState) {
    if (m_done || deltaMs <= 0) { return; }
    m_z = anchor.z;
    if (!anchor.alive) { m_stopped = true; }
    // TrailEffectHolder::Update :294907: at most one sample per tick,
    // including the strict boundary; intervening updates move the head.
    if (m_remainingMs >= static_cast<unsigned>(deltaMs)) {
        m_remainingMs -= deltaMs;
        m_trail.Update(anchor.x, anchor.y);
    } else {
        m_remainingMs = m_intervalMs;
        if (!m_stopped) {
            m_trail.Push(anchor.x, anchor.y);
            m_liveAmount = static_cast<unsigned>(m_trail.GetAmount());
        } else {
            m_trail.Pop();
            m_done = m_trail.GetAmount() == 0;
        }
    }
}
void TrailEffectHolder::Draw(ZSpriteRenderer &sprites, ZEffectColors &colors,
    const ZEffectProjection &projection) const {
    float fade = 1;
    if (m_stopped && m_liveAmount != 0) { fade = static_cast<float>(m_trail.GetAmount()) / m_liveAmount; }
    m_trail.Draw(sprites, colors, projection, m_z, m_color, fade);
}
