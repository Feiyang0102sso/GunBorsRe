#include "gun_bros_re/HostSettings.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstdio>

HostSettings &GameHostSettings() {
    static HostSettings settings;
    return settings;
}

bool HostSettings::Load(const std::filesystem::path &path) {
    if (!std::filesystem::exists(path)) {
        std::ofstream output(path);
        output << GameConfig::EffectsVolume << "=" << effectsVolume << "\n";
        output << GameConfig::DrawFPS << "=1\n";
        output << "# Deathmatch bot: 1=Easy, 2=Normal, 3=Hard\n";
        output << GameConfig::DMBotLevel << "=" << dmBotLevel << "\n";
#if GB_ENABLE_CHEATS
        output << GameConfig::DebugMode << "=0\n" << GameConfig::IsConnected << "=0\n";
#endif
        return output.good();
    }
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) { line.erase(comment); }
        const auto equals = line.find('=');
        if (equals == std::string::npos) { continue; }
        line[equals] = ' ';
        std::istringstream fields(line);
        std::string name;
        int value = 0;
        if (!(fields >> name >> value)) {
            std::printf("[config] not a setting: %s\n", line.c_str());
            return false;
        }
        // EffectsVolume runs 0..10; DMBotLevel runs 1..3; other settings are flags.
        if (name == GameConfig::DMBotLevel) {
            if (value < 1 || value > 3) {
                std::printf("[config] DMBotLevel must be 1, 2 or 3\n");
                return false;
            }
            dmBotLevel = value;
            continue;
        }
        const bool ranged = name == GameConfig::EffectsVolume;
        if (!ranged && value != 0 && value != 1) {
            std::printf("[config] invalid boolean: %s\n", line.c_str());
            return false;
        }
        if (ranged && (value < 0 || value > 10)) {
            std::printf("[config] %s must be 0..10\n", name.c_str());
            return false;
        }
#if GB_ENABLE_CHEATS
        if (name == GameConfig::IsConnected) { isConnected = value == 1; }
        else if (name == GameConfig::DebugMode) { debugMode = value == 1; }
        else
#endif
        if (name == GameConfig::DrawFPS) { drawFPS = value == 1; }
        else if (name == GameConfig::EffectsVolume) { effectsVolume = value; }
        else { std::printf("[config] unknown setting: %s\n", name.c_str()); }
    }
    std::printf("[config] connected=%d debug=%d draw-fps=%d effects-volume=%d\n", isConnected, debugMode, drawFPS, effectsVolume);
    std::printf("[config] dm-bot-level=%d\n", dmBotLevel);
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
