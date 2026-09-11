/** Host-only configuration; these switches do not pretend to connect to retired servers. */
#ifndef GUN_BROS_RE_HOSTSETTINGS_H
#define GUN_BROS_RE_HOSTSETTINGS_H
#include <filesystem>
#include "gun_bros_re/Config.h"
#include <cstdint>

struct HostSettings {
#if GB_ENABLE_CHEATS
    bool isConnected = false;
    bool debugMode = false;
#else
    static constexpr bool isConnected = false;
    static constexpr bool debugMode = false;
#endif
    /** Sound effect loudness on the original's own 0..10 media-player dial;
     *  see CAudioPlayer::SetEffectsGain for where that scale comes from. */
    int effectsVolume = GameConfig::DefaultEffectsVolume;
    bool Load(const std::filesystem::path &path);
};

HostSettings &GameHostSettings();
/** Local calendar day, independent of daylight-saving day length. */
std::int64_t LocalCalendarDay();
#endif
