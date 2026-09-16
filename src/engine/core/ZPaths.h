#pragma once
#include <filesystem>
#include <string>

/** Runtime directory conventions. The platform layer resolves the directory from the executable path. */
namespace Paths {
inline constexpr const char *GameName = "GunBrosRe";
inline constexpr const char *ViewerName = "GunBrosViewer";
inline constexpr const char *BigDirectory = "big";
inline constexpr const char *SaveDirectory = "saves";
inline constexpr const char *LogDirectory = "logs";
inline constexpr const char *ShaderDirectory = "assets/shaders";
inline constexpr const char *AudioDirectory = "assets/audio";
inline constexpr const char *StartupDirectory = "assets/startup";

inline std::string ConfigFilename(const char *productName) { return std::string(productName) + ".cfg"; }
inline std::string ExecutableFilename(const char *productName) { return std::string(productName) + ".exe"; }

const std::filesystem::path &Root();
std::filesystem::path Resolve(const std::filesystem::path &path);
const std::string &Shaders();
}
