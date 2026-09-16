/** StatisticPacket fields, without copying its padded native memory layout.
 * Original serialization :122468; local transport shares values directly.
 */
#pragma once
#include <cstdint>
struct ZMultiplayerStats {
    unsigned kills = 0, assists = 0, perfectWaves = 0, deaths = 0, bestStreak = 0;
    std::uint64_t xplodium = 0, experience = 0;
};
struct ZMultiplayerStatistics {
    ZMultiplayerStats wave, total;
    unsigned streak = 0;
    unsigned previousHits = 0;
    unsigned streakHits = 0;
    unsigned xplodiumRemainder = 0;
};
