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
    // PvP balance knobs: milliseconds between successful heals, shared across pack sizes.
    // Each life starts with a fresh budget and no initial health-pack cooldown.
    static constexpr unsigned NormalHealthPackIntervalMs = 5000;
    static constexpr unsigned HardHealthPackIntervalMs = 2000;
    static constexpr unsigned NormalHealthPacksPerLife = 99;
    static constexpr unsigned HardHealthPacksPerLife = 99;
    unsigned GetHealthPackIntervalMs() const {
        if (difficulty == Difficulty::Normal) { return NormalHealthPackIntervalMs; }
        if (difficulty == Difficulty::Hard) { return HardHealthPackIntervalMs; }
        return 0;
    }
    unsigned GetHealthPacksPerLife() const {
        if (difficulty == Difficulty::Normal) { return NormalHealthPacksPerLife; }
        if (difficulty == Difficulty::Hard) { return HardHealthPacksPerLife; }
        return HealthPacksPerLife;
    }
    bool IsHard() const { return difficulty == Difficulty::Hard; }
    bool HasUnlimitedPowerups() const { return difficulty != Difficulty::Easy; }
    static bool IsValidDifficulty(int value) {
        return value >= static_cast<int>(Difficulty::Easy) && value <= static_cast<int>(Difficulty::Hard);
    }
};
