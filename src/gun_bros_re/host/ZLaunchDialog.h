#pragma once
#include <filesystem>

struct ZHostSettings;

/** Native desktop settings before SDL initialization; no original Movie data is involved. */
enum class ZLaunchResult { Start, Cancel, Error };
ZLaunchResult ShowLaunchDialog(ZHostSettings &settings, const std::filesystem::path &configPath,
                              const std::filesystem::path &headerPath);
