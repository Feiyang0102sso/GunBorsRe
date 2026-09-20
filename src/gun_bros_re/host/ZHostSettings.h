/** Host-only configuration; these switches do not pretend to connect to retired servers. */
#ifndef GUN_BROS_RE_ZHOSTSETTINGS_H
#define GUN_BROS_RE_ZHOSTSETTINGS_H
#include <filesystem>
#include "gun_bros_re/host/ZConfig.h"
#include "gun_bros_re/gameplay/multiplayer/bot/ZBotSettings.h"
#include <cstdint>

struct ZHostSettings {
    std::string title = GameConfig::DefaultTitle;
    bool startDialog = GameConfig::DefaultStartDialog;
    int screenX = GameConfig::DefaultScreenX;
    int screenY = GameConfig::DefaultScreenY;
    // Host BGM dial: 3 preserves the original 0.3 gain before scene scaling.
    int soundVolume = GameConfig::DefaultSoundVolume;
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
    int dmBotLevel = static_cast<int>(ZBotSettings::DefaultDifficulty);
    // Desktop controls: 1=screen aiming, 2=right-stick dragging.
    int control = 1;
    bool Load(const std::filesystem::path &path);
    /** Save editable host values without discarding user comments or section labels. */
    bool Save(const std::filesystem::path &path) const;
};

ZHostSettings &GameHostSettings();
/** Local calendar day, independent of daylight-saving day length. */
std::int64_t LocalCalendarDay();
#endif
