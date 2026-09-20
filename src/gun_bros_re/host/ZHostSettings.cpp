#include "gun_bros_re/host/ZHostSettings.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstdio>

ZHostSettings &GameHostSettings() {
    static ZHostSettings settings;
    return settings;
}

bool ZHostSettings::Load(const std::filesystem::path &path) {
    if (!std::filesystem::exists(path)) {
        std::ofstream output(path);
        output << "# Section headers are labels; keys are global and case-sensitive.\n";
        output << "[common]\n# Game window title (UTF-8, no quotes).\n";
        output << GameConfig::Title << "=" << title << "\n";
        output << "# Show launch dialog at startup: 0=off, 1=on.\n";
        output << GameConfig::StartDialog << "=" << startDialog << "\n";
        output << "# Window client size; oversized windows fit the desktop automatically.\n";
        output << GameConfig::ScreenX << "=" << screenX << "\n";
        output << GameConfig::ScreenY << "=" << screenY << "\n\n";
        output << "[audio]\n# BGM volume: 0..10; 0=mute, 3=original volume.\n";
        output << GameConfig::SoundVolume << "=" << soundVolume << "\n";
        output << "# Sound effects volume: 0..10; 0=mute.\n";
        output << GameConfig::EffectsVolume << "=" << effectsVolume << "\n\n";
        output << "[game]\n# Local online-menu adapter: 0=off, 1=on; no remote connection.\n";
        output << GameConfig::IsConnected << "=" << isConnected << "\n";
        output << "# Deathmatch bot: 1=Easy, 2=Normal, 3=Hard\n";
        output << ZBotSettings::DifficultyKey << "=" << dmBotLevel << "\n\n";
        output << "[control]\n";
        output << "# Mouse fire: 1=screen aim, 2=right stick drag\n";
        output << GameConfig::Control << "=" << control << "\n\n";
        output << "[debug]\n# Debug information and debug hotkeys: 0=off, 1=on.\n";
        output << GameConfig::DebugMode << "=" << debugMode << "\n";
        output << "# FPS counter: 0=hidden, 1=visible.\n";
        output << GameConfig::DrawFPS << "=" << drawFPS << "\n";
        return output.good();
    }
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        if (line.compare(0, 3, "\xEF\xBB\xBF") == 0) { line.erase(0, 3); }
        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) { line.erase(comment); }
        const auto equals = line.find('=');
        // Section headers are presentation labels, not key namespaces.
        if (equals == std::string::npos) { continue; }
        const std::string keyText = line.substr(0, equals);
        std::istringstream keyField(keyText);
        std::string name;
        keyField >> name;
        if (name == GameConfig::Title) {
            const auto first = line.find_first_not_of(" \t\r", equals + 1);
            if (first == std::string::npos) {
                std::printf("[config] Title must not be empty\n");
                return false;
            }
            const auto last = line.find_last_not_of(" \t\r");
            title = line.substr(first, last - first + 1);
            continue;
        }
        line[equals] = ' ';
        std::istringstream fields(line);
        int value = 0;
        if (!(fields >> name >> value)) {
            std::printf("[config] not a setting: %s\n", line.c_str());
            return false;
        }
        // EffectsVolume and SoundVolume run 0..10; DMBotLevel runs 1..3; control runs 1..2.
        // Remaining settings are flags.
        if (name == GameConfig::ScreenX || name == GameConfig::ScreenY) {
            fields >> std::ws;
            if (!fields.eof() || value < 1 || value > GameConfig::MaximumScreenSize) {
                std::printf("[config] %s must be 1..%d\n", name.c_str(), GameConfig::MaximumScreenSize);
                return false;
            }
            if (name == GameConfig::ScreenX) { screenX = value; }
            else { screenY = value; }
            continue;
        }
        if (name == GameConfig::StartDialog) {
            fields >> std::ws;
            if (!fields.eof() || (value != 0 && value != 1)) {
                std::printf("[config] StartDialog must be 0 or 1\n");
                return false;
            }
            startDialog = value == 1;
            continue;
        }
        if (name == GameConfig::Control) {
            if (value != 1 && value != 2) {
                std::printf("[config] control must be 1 or 2\n");
                return false;
            }
            control = value;
            continue;
        }
        if (name == ZBotSettings::DifficultyKey) {
            if (!ZBotSettings::IsValidDifficulty(value)) {
                std::printf("[config] DMBotLevel must be 1, 2 or 3\n");
                return false;
            }
            dmBotLevel = value;
            continue;
        }
        const bool ranged = name == GameConfig::EffectsVolume || name == GameConfig::SoundVolume;
        if (!ranged && value != 0 && value != 1) {
            std::printf("[config] invalid boolean: %s\n", line.c_str());
            return false;
        }
        if (ranged && (value < 0 || value > 10)) {
            std::printf("[config] %s must be 0..10\n", name.c_str());
            return false;
        }
        if (name == GameConfig::IsConnected) { isConnected = value == 1; }
        else if (name == GameConfig::DebugMode) { debugMode = value == 1; }
        else
        if (name == GameConfig::DrawFPS) { drawFPS = value == 1; }
        else if (name == GameConfig::EffectsVolume) { effectsVolume = value; }
        else if (name == GameConfig::SoundVolume) { soundVolume = value; }
        else { std::printf("[config] unknown setting: %s\n", name.c_str()); }
    }
    std::printf("[config] connected=%d debug=%d draw-fps=%d effects-volume=%d\n", isConnected, debugMode, drawFPS, effectsVolume);
    std::printf("[config] dm-bot-level=%d\n", dmBotLevel);
    std::printf("[config] control=%d\n", control);
    std::printf("[config] title=%s sound-volume=%d\n", title.c_str(), soundVolume);
    std::printf("[config] screen=%dx%d\n", screenX, screenY);
    std::printf("[config] start-dialog=%d\n", startDialog);
    return !input.bad();
}

std::int64_t LocalCalendarDay() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    local.tm_hour = 0;
    local.tm_min = 0;
    local.tm_sec = 0;
    // Interpret the local date at UTC midnight only to number calendar days.
    return _mkgmtime64(&local) / 86400;
}
