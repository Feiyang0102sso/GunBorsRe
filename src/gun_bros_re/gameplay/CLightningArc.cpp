/** @file CLightningArc.cpp
 * @brief Port of CLightningArc and CMeshLine mode 2; Windows uses the effect RNG.
 */
#include "gun_bros_re/gameplay/CLightningArc.h"
#include <cmath>

namespace {
// CBullet::SetLightning overrides the arc constructor's interval with 20 ms.
constexpr unsigned kFrameIntervalMs = 20;
}

void CLightningArc::GenerateArc(std::vector<Vertex> &points, unsigned first,
    unsigned count, std::uint32_t &randomState) {
    if (count < 3) { return; }
    const Vertex start = points[first];
    const Vertex end = points[first + count - 1];
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    randomState = randomState * 1664525u + 1013904223u;
    float displacement = m_settings.displacement;
    if ((randomState & 0x10000) == 0) { displacement = -displacement; }
    const unsigned middle = first + count / 2;
    // GenerateArc :243259: perpendicular unit vector * length cancels here.
    points[middle] = {(start.x + end.x) * 0.5f - dy * displacement,
        (start.y + end.y) * 0.5f + dx * displacement};
    GenerateArc(points, first, count / 2 + 1, randomState);
    GenerateArc(points, middle, count - count / 2, randomState);
}

void CLightningArc::Update(const BulletLightningSettings &settings, int deltaMs,
    std::uint32_t &randomState) {
    if (settings.revision != m_settings.revision) {
        m_settings = settings;
        m_frames.clear();
        if (settings.pointCount < 2 || settings.frameCount == 0 || settings.length <= 0) { return; }
        for (unsigned frame = 0; frame < settings.frameCount; ++frame) {
            std::vector<Vertex> points(settings.pointCount);
            points.back().y = settings.length;
            GenerateArc(points, 0, settings.pointCount, randomState);
            std::vector<Vertex> vertices;
            for (unsigned index = 0; index < settings.pointCount; ++index) {
                unsigned first = 0, last = 1;
                if (index > 0) { first = index - 1; last = index; }
                const float dx = points[last].x - points[first].x;
                const float dy = points[last].y - points[first].y;
                const float length = std::hypot(dx, dy);
                const float nx = -dy / length * settings.halfWidth;
                const float ny = dx / length * settings.halfWidth;
                // CMeshLine::InsertVertex mode 2: both sides have full opacity.
                vertices.push_back({points[index].x + nx, points[index].y + ny});
                vertices.push_back({points[index].x - nx, points[index].y - ny});
            }
            m_frames.push_back(std::move(vertices));
        }
    }
    if (m_frames.empty()) { return; }
    m_ageMs = (m_ageMs + deltaMs) % (kFrameIntervalMs * m_settings.frameCount);
}

std::vector<CLightningArc::Vertex> CLightningArc::Interpolate(unsigned segment) const {
    if (m_frames.empty()) { return {}; }
    const unsigned first = (m_ageMs / kFrameIntervalMs + segment) % m_frames.size();
    const unsigned next = (first + 1) % m_frames.size();
    const float fraction = static_cast<float>(m_ageMs % kFrameIntervalMs) / kFrameIntervalMs;
    std::vector<Vertex> vertices = m_frames[first];
    // CMeshLine::Interpolate blends final edge vertices, not just center points.
    for (unsigned index = 0; index < vertices.size(); ++index) {
        vertices[index].x += (m_frames[next][index].x - vertices[index].x) * fraction;
        vertices[index].y += (m_frames[next][index].y - vertices[index].y) * fraction;
    }
    return vertices;
}
