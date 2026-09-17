#pragma once
#include <array>
#include <cstdint>
/** CBullet native12/13: authored point count, width, sample time and RGBA. */
struct ZBulletRibbonSettings {
    unsigned capacity = 0;
    float width = 0;
    unsigned intervalMs = 0;
    std::array<std::uint16_t, 4> color{};
};
