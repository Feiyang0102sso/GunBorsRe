/** Host-only configuration; these switches do not pretend to connect to retired servers. */
#ifndef GUN_BROS_RE_ZHOSTSETTINGS_H
#define GUN_BROS_RE_ZHOSTSETTINGS_H
#include <filesystem>
#include "gun_bros_re/ZConfig.h"
#include <cstdint>

struct ZHostSettings {
    bool drawFPS = true;
    // Session-only information toggle; DebugMode remains the master switch.
    bool drawDebugInfo = true;
    // Enables the local NGS/StoreKit/Game Center adapter for reconstructed menus.
    bool isConnected = false;
    bool debugMode = false;
    /** Sound effect loudness on the original's own 0..10 media-player dial;
     *  see CAudioPlayer::SetEffectsGain for where that scale comes from. */
    int effectsVolume = GameConfig::DefaultEffectsVolume;
    // Local deathmatch only: 1=Easy, 2=Normal, 3=Hard; Easy keeps the life budget.
    int dmBotLevel = GameConfig::DefaultDMBotLevel;
    bool Load(const std::filesystem::path &path);
};

ZHostSettings &GameHostSettings();
/** Local calendar day, independent of daylight-saving day length. */
std::int64_t LocalCalendarDay();
#endif
