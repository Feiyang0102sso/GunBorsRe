#pragma once
#include <cstdint>

/** Existing desktop deterministic stream; not a port of iOS Utility::Random. */
class ZRandom {
public:
    void Seed(std::uint32_t seed) { m_state = seed; }
    std::int16_t Integer(std::int16_t minimum, std::int16_t maximum) {
        int first = minimum;
        int last = maximum;
        if (first > last) {
            first = maximum;
            last = minimum;
        }
        m_state = m_state * 1664525u + 1013904223u;
        return static_cast<std::int16_t>(first + (m_state >> 8) % (last - first + 1));
    }
private:
    std::uint32_t m_state = 1;
};
