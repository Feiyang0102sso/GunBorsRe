#pragma once
#include "gun_bros_re/effects/EffectHolder.h"
#include "gun_bros_re/effects/CRibbonTrailEffect.h"
/** CRibbonTrailEffect / TrailEffectHolder state; detached trails drain in place.
 * Original sampling/retirement clock (:294891); at most one sample per update.
 */
class TrailEffectHolder : public EffectHolder {
public:
    TrailEffectHolder(float width, unsigned capacity, unsigned intervalMs);
    void SetColor(const std::array<std::uint16_t, 4> &color) { m_color = color; }
    void Update(const Anchor &anchor, int deltaMs, std::uint32_t &randomState) override;
    void Draw(ZSpriteRenderer &sprites, ZEffectColors &colors, const ZEffectProjection &projection) const override;
    void Stop() override { m_stopped = true; }
    void StopInstant() override { m_stopped = true; m_done = true; }
    bool IsDone() const override { return m_done; }
    bool IsRibbon() const override { return true; }
    std::size_t GetAmount() const { return m_trail.GetAmount(); }
private:
    CRibbonTrailEffect m_trail;
    std::array<std::uint16_t, 4> m_color{};
    unsigned m_intervalMs = 0;
    unsigned m_remainingMs = 0;
    unsigned m_liveAmount = 0;
    float m_z = 0;
    bool m_stopped = false;
    bool m_done = false;
};
