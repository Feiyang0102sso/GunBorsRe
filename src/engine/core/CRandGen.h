#pragma once
#include <cstdint>
#include <random>
#include <utility>

/** Existing desktop deterministic stream; not a port of iOS Utility::Random. */
// Historical ZRandom description above; its LCG has been replaced by the original algorithm below.
/** Original CRandGen::Seed/Generate :370208/:370253 is MT19937.
 * std::mt19937 has the same 624/397 recurrence, seed multiplier and tempering.
 * Explicit desktop seeds remain available; platform singleton/time seeding does not.
 */
class CRandGen {
public:
    explicit CRandGen(std::uint32_t seed = 1) : m_generator(seed) {}
    void Seed(std::uint32_t seed) { m_generator.seed(seed); }
    std::uint32_t Generate() { return m_generator(); }
    /** GetRandRange :370346 uses inclusive modulo, not uniform_int_distribution. */
    int GetRandRange(int minimum, unsigned maximum) {
        const auto width = static_cast<std::uint32_t>(maximum - minimum) + 1u;
        return minimum + static_cast<int>(Generate() % width);
    }
    /** Utility::Random :105917 orders bounds and does not draw for equal bounds. */
    std::int16_t Integer(std::int16_t minimum, std::int16_t maximum) {
        if (minimum == maximum) { return minimum; }
        if (minimum > maximum) { std::swap(minimum, maximum); }
        return static_cast<std::int16_t>(GetRandRange(minimum, static_cast<unsigned>(maximum)));
    }
private:
    std::mt19937 m_generator;
};
