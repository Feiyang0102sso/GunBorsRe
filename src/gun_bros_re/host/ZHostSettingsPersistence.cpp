/** Lossless host cfg edits. Section names are presentation labels, not namespaces. */
#include "gun_bros_re/host/ZHostSettings.h"
#define NOMINMAX
#include <Windows.h>
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>

namespace {
struct ZConfigValue {
    const char *section;
    const char *key;
    std::string value;
};

std::string Trim(const std::string &text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) { return {}; }
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

/** Update every occurrence because the loader accepts duplicate global keys. */
void UpdateValue(std::vector<std::string> &lines, const ZConfigValue &setting) {
    bool found = false;
    std::size_t insertAt = lines.size();
    bool inSection = false;
    bool sectionFound = false;
    const std::string section = "[" + std::string(setting.section) + "]";
    for (std::size_t index = 0; index < lines.size(); ++index) {
        std::string &line = lines[index];
        const auto comment = line.find_first_of("#;");
        const std::string text = Trim(line.substr(0, comment));
        if (!text.empty() && text.front() == '[' && text.back() == ']') {
            inSection = text == section;
            if (inSection) { sectionFound = true; insertAt = index + 1; }
        } else if (inSection) {
            insertAt = index + 1;
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos || equals > comment || Trim(line.substr(0, equals)) != setting.key) { continue; }
        std::size_t end = line.size();
        if (comment != std::string::npos) { end = comment; }
        std::size_t start = equals + 1;
        while (start < end && (line[start] == ' ' || line[start] == '\t')) { ++start; }
        while (end > start && (line[end - 1] == ' ' || line[end - 1] == '\t')) { --end; }
        line.replace(start, end - start, setting.value);
        found = true;
    }
    if (found) { return; }
    if (!sectionFound) {
        if (!lines.empty() && !lines.back().empty()) { lines.emplace_back(); }
        lines.push_back(section);
        insertAt = lines.size();
    }
    // Keep the separating blank lines after the group's last setting.
    while (insertAt > 0 && Trim(lines[insertAt - 1]).empty()) { --insertAt; }
    lines.insert(lines.begin() + insertAt, std::string(setting.key) + "=" + setting.value);
}
}

bool ZHostSettings::Save(const std::filesystem::path &path) const {
    if (Trim(title).empty() || title != Trim(title) || title.find_first_of("#;\r\n") != std::string::npos ||
        screenX < 1 || screenX > GameConfig::MaximumScreenSize || screenY < 1 || screenY > GameConfig::MaximumScreenSize ||
        soundVolume < 0 || soundVolume > 10 || effectsVolume < 0 || effectsVolume > 10 ||
        !ZBotSettings::IsValidDifficulty(dmBotLevel) || (control != 1 && control != 2)) {
        std::printf("[config] refusing to save invalid settings\n");
        return false;
    }
    std::string original;
    if (std::filesystem::exists(path)) {
        std::ifstream input(path, std::ios::binary);
        if (!input) { return false; }
        original.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        if (input.bad()) { return false; }
    }
    std::string content = original;
    std::string prefix;
    if (content.compare(0, 3, "\xEF\xBB\xBF") == 0) { prefix = content.substr(0, 3); content.erase(0, 3); }
    std::string newline = "\n";
    if (content.find("\r\n") != std::string::npos) { newline = "\r\n"; }
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < content.size()) {
        auto end = content.find('\n', start);
        if (end == std::string::npos) { end = content.size(); }
        std::string line = content.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') { line.pop_back(); }
        lines.push_back(std::move(line));
        start = end + 1;
    }
    const ZConfigValue settings[] = {
        {"common", GameConfig::Title, title},
        {"common", GameConfig::StartDialog, std::to_string(startDialog)},
        {"common", GameConfig::ScreenX, std::to_string(screenX)},
        {"common", GameConfig::ScreenY, std::to_string(screenY)},
        {"audio", GameConfig::SoundVolume, std::to_string(soundVolume)},
        {"audio", GameConfig::EffectsVolume, std::to_string(effectsVolume)},
        {"game", GameConfig::IsConnected, std::to_string(isConnected)},
        {"game", ZBotSettings::DifficultyKey, std::to_string(dmBotLevel)},
        {"control", GameConfig::Control, std::to_string(control)},
        {"debug", GameConfig::DebugMode, std::to_string(debugMode)},
        {"debug", GameConfig::DrawFPS, std::to_string(drawFPS)}
    };
    for (const auto &setting : settings) { UpdateValue(lines, setting); }
    std::string updated = prefix;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        updated += lines[index];
        if (index + 1 < lines.size() || original.empty() || original.back() == '\n') { updated += newline; }
    }
    if (updated == original) { return true; }
    const std::filesystem::path temporary = path.wstring() + L".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(updated.data(), static_cast<std::streamsize>(updated.size()));
        output.close();
        if (!output) { return false; }
    }
    // Replace only after the entire new file has been successfully written.
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::printf("[config] replace failed: error=%lu\n", GetLastError());
        return false;
    }
    std::printf("[config] saved: %s\n", path.u8string().c_str());
    return true;
}
