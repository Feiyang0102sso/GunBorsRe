#pragma once
/** Windows solid-color textures shared by ribbon and lightning drawing. */
#include "engine/graphics/ZTexture.h"
#include <array>
#include <map>
#include <memory>
class ZEffectColors {
public:
    const ZTexture *Get(const std::array<std::uint16_t, 4> &color);
private:
    std::map<std::uint64_t, std::unique_ptr<ZTexture>> m_colors;
};
