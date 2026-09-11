#pragma once
#include "engine/core/Paths.h"
#include <cstdint>
namespace ResearchDefaults {
// Regression hosts exercise game flags; never parse or overwrite viewer preferences.
inline const std::string Filename = Paths::ConfigFilename("GunBrosTests");
// Defaults for the M2 image: a 512x512 RGB texture in the core pack.
inline constexpr const char * kDefaultImagePack = "pack0_core";
inline constexpr std::uint32_t kDefaultImageResourceId = 313;

// Defaults for the M3 map: the first of pack2's nine.
inline constexpr const char * kDefaultMapPack = "pack2";
inline constexpr std::uint32_t kDefaultMapIndex = 0;
}
