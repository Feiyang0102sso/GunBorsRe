#pragma once
#include <cstdint>

/** BIG format generations, newest first. These are not release numbers.
 * Evidence: ___GAME_TOC_KEYSET + OBJECT_SCRIPT__COUNTS_ in the original
 * archives; big_keyset.bt and CGameObjectPack::InitializeCounts/:129948.
 * All supported layouts append PNG, WAV, MODEL, LEVEL_REFS and COUNTS after
 * their object types. Minor release differences do not get another version.
 */
enum class ZBigVersion : unsigned {
    Unknown = 0,
    V1 = 1,
    V2 = 2,
    V3 = 3,
};

struct ZBigVersionLayout {
    ZBigVersion version;
    std::uint32_t objectTypeCount;
    std::uint32_t sectionCount;
};

// Only format signatures belong here. Resource values stay in the BIG files.
inline constexpr ZBigVersionLayout kBigVersionLayouts[] = {
    {ZBigVersion::V1, 28, 33},
    {ZBigVersion::V2, 27, 32},
    {ZBigVersion::V3, 26, 31},
};

inline const ZBigVersionLayout *FindBigVersionLayout(std::uint32_t sections,
                                                   std::uint32_t objectTypes) {
    for (const ZBigVersionLayout &layout : kBigVersionLayouts) {
        if (layout.sectionCount == sections && layout.objectTypeCount == objectTypes) {
            return &layout;
        }
    }
    return nullptr;
}
