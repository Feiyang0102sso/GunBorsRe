#pragma once
#include <cstdint>

/** BIG format generations, newest first. These are not release numbers.
 * Evidence: ___GAME_TOC_KEYSET + OBJECT_SCRIPT__COUNTS_ in the original
 * archives; big_keyset.bt and CGameObjectPack::InitializeCounts/:129948.
 * All supported layouts append PNG, WAV, MODEL, LEVEL_REFS and COUNTS after
 * their object types. Minor release differences do not get another version.
 */
enum class BigVersion : unsigned {
    Unknown = 0,
    V1 = 1,
    V2 = 2,
    V3 = 3,
};

struct BigVersionLayout {
    BigVersion version;
    std::uint32_t objectTypeCount;
    std::uint32_t sectionCount;
};

// Only format signatures belong here. Resource values stay in the BIG files.
inline constexpr BigVersionLayout kBigVersionLayouts[] = {
    {BigVersion::V1, 28, 33},
    {BigVersion::V2, 27, 32},
    {BigVersion::V3, 26, 31},
};

inline const BigVersionLayout *FindBigVersionLayout(std::uint32_t sections,
                                                   std::uint32_t objectTypes) {
    for (const BigVersionLayout &layout : kBigVersionLayouts) {
        if (layout.sectionCount == sections && layout.objectTypeCount == objectTypes) {
            return &layout;
        }
    }
    return nullptr;
}
