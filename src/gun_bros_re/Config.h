#pragma once
#include "engine/core/Paths.h"
namespace GameConfig {
inline const std::string Filename = Paths::ConfigFilename(Paths::GameName);
inline constexpr const char *EffectsVolume = "EffectsVolume";
inline constexpr int DefaultEffectsVolume = 3;
inline constexpr const char *DebugMode = "DebugMode";
inline constexpr const char *IsConnected = "IsConnected";
}
