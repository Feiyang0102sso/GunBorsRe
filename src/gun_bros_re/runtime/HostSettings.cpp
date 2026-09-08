#include "runtime/HostSettings.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstdio>

HostSettings &GameHostSettings() {
    static HostSettings settings;
    return settings;
}

bool HostSettings::Load(const std::filesystem::path &path) {
    if (!std::filesystem::exists(path)) { return true; }
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
        if (!(fields >> name >> value) || (value != 0 && value != 1)) {
            std::printf("[config] invalid boolean: %s\n", line.c_str());
            return false;
        }
        if (name == "IsConnected") { isConnected = value == 1; }
        else if (name == "DebugMode") { debugMode = value == 1; }
        else { std::printf("[config] unknown setting: %s\n", name.c_str()); }
    }
    std::printf("[config] connected=%d debug=%d\n", isConnected, debugMode);
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
