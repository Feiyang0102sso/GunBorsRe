#define NOMINMAX
#include "gun_bros_re/effects/ZEffectColors.h"
#include "engine/graphics/ZPNG.h"
#include <algorithm>
const ZTexture *ZEffectColors::Get(const std::array<std::uint16_t, 4> &color) {
    const std::uint64_t key = (static_cast<std::uint64_t>(color[0]) << 48) |
        (static_cast<std::uint64_t>(color[1]) << 32) |
        (static_cast<std::uint64_t>(color[2]) << 16) | color[3];
    auto &texture = m_colors[key];
    if (!texture) {
        ZPNGImage pixel;
        pixel.width = 1; pixel.height = 1;
        for (unsigned channel = 0; channel < 4; ++channel) {
            pixel.pixels.push_back(static_cast<std::uint8_t>(std::min<unsigned>(255, color[channel])));
        }
        texture = std::make_unique<ZTexture>();
        if (!texture->Create(pixel)) { return nullptr; }
    }
    return texture.get();
}
