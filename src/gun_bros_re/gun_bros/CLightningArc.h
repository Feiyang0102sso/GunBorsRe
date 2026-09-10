/** @file CLightningArc.h
 * @brief Original recursive lightning geometry, independent of the host renderer.
 * Sources: CLightningArc/GenerateArc :243194-243900; CBullet::SetLightning :60480.
 */
#pragma once
#include <cstdint>
#include <vector>

struct BulletLightningSettings {
    float displacement = 0;
    float halfWidth = 0;
    float length = 0;
    unsigned pointCount = 0;
    unsigned frameCount = 0;
    unsigned revision = 0;
};

class CLightningArc {
public:
    struct Vertex { float x = 0, y = 0; };
    void Update(const BulletLightningSettings &settings, int deltaMs, std::uint32_t &randomState);
    std::vector<Vertex> Interpolate(unsigned segment) const;
    float GetLength() const { return m_settings.length; }
    bool IsReady() const { return !m_frames.empty(); }

private:
    void GenerateArc(std::vector<Vertex> &points, unsigned first, unsigned count, std::uint32_t &randomState);
    BulletLightningSettings m_settings;
    std::vector<std::vector<Vertex>> m_frames;
    unsigned m_ageMs = 0;
};
