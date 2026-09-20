#pragma once
#include "engine/core/ZPaths.h"
namespace GameConfig {
inline const std::string Filename = Paths::ConfigFilename(Paths::GameName);
inline constexpr const char *Title = "Title";
inline constexpr const char *DefaultTitle = "GunBroRe";
inline constexpr const char *StartDialog = "StartDialog";
inline constexpr bool DefaultStartDialog = true;
inline constexpr const char *ScreenX = "ScreenX";
inline constexpr const char *ScreenY = "ScreenY";
inline constexpr int DefaultScreenX = 1600;
inline constexpr int DefaultScreenY = 1200;
// Bound desktop surface requests to 16K per axis; these are host dimensions, not BIG data.
inline constexpr int MaximumScreenSize = 16384;
inline constexpr const char *SoundVolume = "SoundVolume";
inline constexpr int DefaultSoundVolume = 3;
inline constexpr const char *EffectsVolume = "EffectsVolume";
inline constexpr int DefaultEffectsVolume = 3;
inline constexpr const char *DebugMode = "DebugMode";
inline constexpr const char *DrawFPS = "DrawFPS";
inline constexpr const char *IsConnected = "IsConnected";
inline constexpr const char *Control = "control";
}
