#pragma once
#if GB_ENABLE_CHEATS
#include <string>
#include <vector>
namespace GameCheats {
inline constexpr const char *Money = "chm";
inline constexpr const char *NextDay = "cht";
inline constexpr const char *ToggleDebug = "chd";
inline constexpr const char *ToggleConnection = "chc";
inline constexpr const char *HealthOrGreeting = "chh";
inline constexpr const char *UnlockWaves = "chw";
inline constexpr const char *Invincible = "chi";
inline constexpr const char *Boss = "stboss";
inline constexpr const char *Suicide = "stsuicide";
// Host cheat values and feedback are kept next to their editable commands.
inline constexpr unsigned Coins = 5000, Warbucks = 500, ClearedWaves = 500;
inline constexpr const char *MoneyMessage = "COINS +5000 / WARBUCKS +500";
inline constexpr const char *UnlockMessage = "ALL WAVES UNLOCKED";
/** Input recognition only records commands; menus and combat handle their respective actions. */
bool Consume(std::string &prefix, std::vector<std::string> &commands, char letter, bool repeat);
}
#endif

