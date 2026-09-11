#pragma once
#if GB_ENABLE_CHEATS
#include <string>
#include <vector>
namespace ViewerCheats {
inline constexpr const char *Money = "chm";
inline constexpr const char *NextDay = "cht";
inline constexpr const char *ToggleDebug = "chd";
inline constexpr const char *ToggleConnection = "chc";
inline constexpr const char *HealthOrGreeting = "chh";
inline constexpr const char *UnlockWaves = "chw";
inline constexpr const char *Invincible = "chi";
inline constexpr const char *Boss = "stboss";
inline constexpr const char *Suicide = "stsuicide";
/** Input recognition only records commands; menus and combat handle their respective actions. */
bool Consume(std::string &prefix, std::vector<std::string> &commands, char letter, bool repeat);
}
#endif

