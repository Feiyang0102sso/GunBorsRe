#pragma once

/** User-requested local Bot policy, independent of original CMPMatch resources.
 * Equipment/level definitions are loaded by ZLocalBotRoster from local-bots.cfg.
 */
struct ZBotSettings {
    enum class Difficulty { Easy = 1, Normal = 2, Hard = 3 };
    static constexpr Difficulty DefaultDifficulty = Difficulty::Easy;
    static constexpr const char *DifficultyKey = "DMBotLevel";
    Difficulty difficulty = DefaultDifficulty;
    static constexpr unsigned ShopsPerLife = 2;
    static constexpr unsigned GrenadesPerLife = 2;
    static constexpr unsigned HealthPacksPerLife = 2;
    bool IsHard() const { return difficulty == Difficulty::Hard; }
    bool HasUnlimitedPowerups() const { return difficulty != Difficulty::Easy; }
    static bool IsValidDifficulty(int value) {
        return value >= static_cast<int>(Difficulty::Easy) && value <= static_cast<int>(Difficulty::Hard);
    }
};
