#pragma once
/** Development-only timings for real combat calls; no gameplay decisions. */
#include <chrono>

namespace PerformanceProbe {
inline bool enabled = false;
inline bool uncachedPaths = false;
inline bool disableFlock = false;
struct Counters {
    double spawnMs = 0;
    double brotherMs = 0;
    double navigationMs = 0;
    double enemyMs = 0;
    double effectsMs = 0;
    double pathSearchMs = 0;
    double flockMs = 0;
    unsigned pathSearches = 0;
    unsigned pathNodes = 0;
    unsigned pathCacheHits = 0;
    unsigned spawns = 0;
};
inline Counters counters;

class Scope {
public:
    explicit Scope(double &elapsed) : m_elapsed(elapsed), m_enabled(enabled) {
        if (m_enabled) { m_start = std::chrono::steady_clock::now(); }
    }
    ~Scope() {
        if (m_enabled) {
            m_elapsed += std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - m_start).count();
        }
    }
private:
    double &m_elapsed;
    bool m_enabled;
    std::chrono::steady_clock::time_point m_start;
};
}
