/** Host-only configuration; these switches do not pretend to connect to retired servers. */
#ifndef GUN_BROS_RE_HOSTSETTINGS_H
#define GUN_BROS_RE_HOSTSETTINGS_H
#include <filesystem>
#include <cstdint>

struct HostSettings {
    bool isConnected = false;
    bool debugMode = false;
    bool Load(const std::filesystem::path &path);
};

HostSettings &GameHostSettings();
/** Local calendar day, independent of daylight-saving day length. */
std::int64_t LocalCalendarDay();
#endif
