/** @file CCrc32.h
 * @brief non-reflected CRC, used by CProfileManager disk records.
 */
#ifndef GUN_BROS_RE_CCRC32_H
#define GUN_BROS_RE_CCRC32_H
#include <cstddef>
#include <cstdint>

class CCrc32 {
public:
    static std::uint32_t Crc32(const std::uint8_t *data, std::size_t size) {
        // Original :357828 uses the high byte and a left shift, polynomial
        // 0x04C11DB7, initial/final complement. This differs from zlib CRC32.
        std::uint32_t value = UINT32_MAX;
        for (std::size_t index = 0; index < size; ++index) {
            value ^= static_cast<std::uint32_t>(data[index]) << 24;
            for (unsigned bit = 0; bit < 8; ++bit) {
                const bool high = (value & 0x80000000u) != 0;
                value <<= 1;
                if (high) { 
					value ^= 0x04C11DB7u; 
				}
            }
        }
        return ~value;
    }
};
#endif
