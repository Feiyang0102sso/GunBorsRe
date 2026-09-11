#pragma once
/** Viewer-only host preferences. Game resources and gameplay settings stay separate. */
#include <filesystem>
#include <string>

class CWindow;

namespace ViewerDefaults {
inline constexpr int WindowWidth = 1600;
inline constexpr int WindowHeight = 1200;
inline constexpr int EffectsVolume = 3;
}

struct ViewerSettings {
    int windowWidth = ViewerDefaults::WindowWidth;
    int windowHeight = ViewerDefaults::WindowHeight;
    int effectsVolume = ViewerDefaults::EffectsVolume;

    /** Missing files receive viewer defaults; existing files retain their comments. */
    bool Load(const std::filesystem::path &path);
};

ViewerSettings &GetViewerSettings();
std::string ViewerWindowTitle(const std::string &view, const std::string &details = "");
bool OpenViewerWindow(CWindow &window, const std::string &view);
