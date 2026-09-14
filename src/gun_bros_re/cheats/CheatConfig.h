#pragma once
#include <cstdint>

namespace GameCheats {
// Host cheat values and feedback are kept next to their editable commands.
inline constexpr const char *Money = "chm";
inline constexpr const char *Xplodium = "chplo";
inline constexpr const char *AdvanceRefinery = "stref";
inline constexpr const char *ToggleRefineryLocks = "stlockref";
inline constexpr const char *NextDay = "cht";
inline constexpr const char *ToggleDebug = "chd";
inline constexpr const char *ToggleConnection = "chc";
inline constexpr const char *UpdateChallenges = "chupdate";
inline constexpr const char *UnlockWaves = "chw";
inline constexpr const char *LevelUp = "chxp";
inline constexpr const char *MaximumLevel = "chlvmax";
inline constexpr const char *Boss = "stboss";
inline constexpr const char *Suicide = "stsuicide";
inline constexpr const char *BrotherWeapon = "brow";
inline constexpr const char *BrotherKill = "brok";
inline constexpr const char *BrotherRevive = "bror";
inline constexpr const char *BrotherShop = "bros";
inline constexpr const char *BrotherPowerup = "brop";
inline constexpr const char *Commands[] = {Money, NextDay, ToggleDebug, ToggleConnection, UpdateChallenges,
    UnlockWaves, LevelUp, MaximumLevel,
    Boss, Suicide, BrotherWeapon, BrotherKill, BrotherRevive, BrotherShop, BrotherPowerup,
    Xplodium, AdvanceRefinery, ToggleRefineryLocks};
// Host cheat bounds, not resource timings. Normal gameplay never uses these.
inline constexpr int BossSkipStepMs = 16;
inline constexpr int BossSkipLimitMs = 600000;
inline constexpr unsigned Coins = 500000;
inline constexpr unsigned Warbucks = 500;
inline constexpr unsigned XplodiumAmount = 500;
inline constexpr std::int64_t RefinerySkipSeconds = 24 * 60 * 60;
inline constexpr unsigned Days = 1;
inline constexpr unsigned Levels = 1;
inline constexpr std::uint64_t InputTimeoutMs = 2500;
// UI message formats: keep %u placeholders for coins/warbucks and the resulting level.
inline constexpr const char *MoneyMessage = "COINS +%u / WARBUCKS +%u";
inline constexpr const char *LevelMessage = "LEVEL %u";
inline constexpr const char *UnlockMessage = "ALL WAVES UNLOCKED";
}
