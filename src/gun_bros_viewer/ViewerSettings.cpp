#include "gun_bros_viewer/ViewerSettings.h"
#include "engine/core/Paths.h"
#include "engine/platform/CWindow.h"
#include <charconv>
#include <cstdio>
#include <fstream>

namespace {
std::string Trim(const std::string &text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) { return {}; }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}
}

ViewerSettings &GetViewerSettings() {
    static ViewerSettings settings;
    return settings;
}

bool ViewerSettings::Load(const std::filesystem::path &path) {
    // Load into a fresh value so missing fields never inherit a previous file.
    ViewerSettings loaded;
    if (!std::filesystem::exists(path)) {
        std::ofstream output(path);
        output << "# GunBrosViewer preferences\n"
            << "WindowWidth=" << loaded.windowWidth << "\n"
            << "WindowHeight=" << loaded.windowHeight << "\n"
            << "EffectsVolume=" << loaded.effectsVolume << "\n";
        output.close();
        if (!output) {
            std::printf("[viewer-config] cannot create: %s\n", path.u8string().c_str());
            return false;
        }
    } else {
        std::ifstream input(path);
        if (!input) {
            std::printf("[viewer-config] cannot read: %s\n", path.u8string().c_str());
            return false;
        }
        std::string line;
        unsigned lineNumber = 0;
        while (std::getline(input, line)) {
            ++lineNumber;
            const auto comment = line.find_first_of("#;");
            if (comment != std::string::npos) { line.erase(comment); }
            line = Trim(line);
            if (line.empty()) { continue; }
            const auto equals = line.find('=');
            if (equals == std::string::npos) {
                std::printf("[viewer-config] expected key=value at line %u\n", lineNumber);
                return false;
            }
            const std::string name = Trim(line.substr(0, equals));
            int *setting = nullptr;
            if (name == "WindowWidth") { setting = &loaded.windowWidth; }
            else if (name == "WindowHeight") { setting = &loaded.windowHeight; }
            else if (name == "EffectsVolume") { setting = &loaded.effectsVolume; }
            else {
                // Old viewer files may contain game-only DebugMode/IsConnected fields.
                // Ignore them without importing game configuration or rewriting the file.
                std::printf("[viewer-config] ignored setting: %s\n", name.c_str());
                continue;
            }
            const std::string value = Trim(line.substr(equals + 1));
            int parsedValue = 0;
            const char *end = value.data() + value.size();
            const auto parsed = std::from_chars(value.data(), end, parsedValue);
            bool valid = parsed.ec == std::errc() && parsed.ptr == end;
            if (name == "EffectsVolume") { valid = valid && parsedValue >= 0 && parsedValue <= 10; }
            else { valid = valid && parsedValue > 0; }
            if (!valid) {
                std::printf("[viewer-config] invalid %s at line %u\n", name.c_str(), lineNumber);
                return false;
            }
            *setting = parsedValue;
        }
        if (input.bad()) { return false; }
    }
    *this = loaded;
    std::printf("[viewer-config] %s window=%dx%d effects-volume=%d\n",
        path.u8string().c_str(), windowWidth, windowHeight, effectsVolume);
    return true;
}

std::string ViewerWindowTitle(const std::string &view, const std::string &details) {
    std::string title = std::string(Paths::ViewerName) + " - " + view;
    if (!details.empty()) { title += " | " + details; }
    return title;
}

bool OpenViewerWindow(CWindow &window, const std::string &view) {
    const ViewerSettings &settings = GetViewerSettings();
    const std::string title = ViewerWindowTitle(view);
    std::printf("[viewer-window] %s requested=%dx%d\n", title.c_str(), settings.windowWidth, settings.windowHeight);
    return window.Open(title, settings.windowWidth, settings.windowHeight);
}
