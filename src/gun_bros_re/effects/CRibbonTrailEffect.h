#pragma once
#include "gun_bros_re/gameplay/collision/CCollisionData.h"
#include <array>
#include <cstdint>
class ZSpriteRenderer;
class ZEffectColors;
struct ZEffectProjection;
/** Original ribbon geometry (:243116-243694); sampling belongs to its holder. */
class CRibbonTrailEffect {
public:
    CRibbonTrailEffect(float width, unsigned capacity) : m_width(width), m_capacity(capacity) {}
    void Push(float x, float y);
    void Pop();
    void Update(float x, float y);
    std::size_t GetAmount() const { return m_points.size(); }
    void Draw(ZSpriteRenderer &sprites, ZEffectColors &colors, const ZEffectProjection &projection,
        float z, const std::array<std::uint16_t, 4> &rgba, float opacity) const;
private:
    float m_width;
    unsigned m_capacity;
    std::vector<ZCollisionPoint> m_points;
};
