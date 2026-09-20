#pragma once
#include "engine/core/ZPaths.h"
namespace GameConfig {
inline const std::string Filename = Paths::ConfigFilename(Paths::GameName);
inline constexpr const char *Title = "Title";
inline constexpr const char *DefaultTitle = "GunBroRe";
inline constexpr const char *SoundVolume = "SoundVolume";
inline constexpr int DefaultSoundVolume = 3;
inline constexpr const char *EffectsVolume = "EffectsVolume";
inline constexpr int DefaultEffectsVolume = 3;
inline constexpr const char *DebugMode = "DebugMode";
inline constexpr const char *DrawFPS = "DrawFPS";
inline constexpr const char *IsConnected = "IsConnected";
inline constexpr const char *Control = "control";
}
