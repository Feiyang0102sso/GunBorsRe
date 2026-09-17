#include "gun_bros_re/effects/CRibbonTrailEffect.h"
#include "gun_bros_re/effects/ZEffectColors.h"
#include "engine/glu/sprite/ZSpriteRenderer.h"
#include "engine/graphics/ZEffectProjection.h"
#include <cmath>
void CRibbonTrailEffect::Push(float x, float y) {
    if (m_capacity == 0) { return; }
    if (m_points.size() == m_capacity) { m_points.erase(m_points.begin()); }
    m_points.push_back({x, y});
}
void CRibbonTrailEffect::Pop() {
    if (!m_points.empty()) { m_points.erase(m_points.begin()); }
}
void CRibbonTrailEffect::Update(float x, float y) {
    if (!m_points.empty()) { m_points.back() = {x, y}; }
}
void CRibbonTrailEffect::Draw(ZSpriteRenderer &sprites, ZEffectColors &colors,
    const ZEffectProjection &projection, float z, const std::array<std::uint16_t, 4> &rgba, float opacity) const {
    const std::size_t count = m_points.size();
    if (count < 3) { return; } // CRibbonTrailEffect::Draw :243694.
    const ZTexture *color = colors.Get(rgba);
    if (color == nullptr) { return; }
    std::vector<ZCollisionPoint> points = m_points;
    std::vector<ZCollisionPoint> normals;
    for (std::size_t index = 0; index < count; ++index) {
        projection.Position(points[index].x, points[index].y, z);
    }
    for (std::size_t index = 0; index < count; ++index) {
        std::size_t first = index;
        std::size_t last = index + 1;
        if (index > 0) { first = index - 1; last = index; }
        const float dx = points[last].x - points[first].x;
        const float dy = points[last].y - points[first].y;
        const float length = std::hypot(dx, dy);
        ZCollisionPoint normal;
        if (length > 0) {
            const float halfWidth = m_width * 0.5f * projection.scale;
            normal = {-dy / length * halfWidth, dx / length * halfWidth};
        }
        normals.push_back(normal);
    }
    // CMeshLine::Update :243450 interpolates tail alpha 0 toward the
    // authored head color. InsertVertex :242774/:242847 fades each side.
    for (std::size_t index = 1; index < count; ++index) {
        const auto &a = points[index - 1], &b = points[index];
        const auto &na = normals[index - 1], &nb = normals[index];
        const float alpha[] = {0, opacity * (index - 1) / count, 0, opacity * index / count};
        for (int side = -1; side <= 1; side += 2) {
            const float positions[] = {a.x + na.x * side, a.y + na.y * side, a.x, a.y,
                b.x + nb.x * side, b.y + nb.y * side, b.x, b.y};
            sprites.Batch().AddGradientQuad(*color, positions, alpha);
        }
    }
}
