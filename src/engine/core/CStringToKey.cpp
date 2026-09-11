/**
 * @file CStringToKey.cpp
 * @brief The engine-wide string hash.
 */

#include "engine/core/CStringToKey.h"

#include <cstring>

namespace {

/** Rotate left by 4. The original writes this as ROR by 28. */
std::uint32_t RotateLeft4(std::uint32_t value) {
    return (value << 4) | (value >> 28);
}

}  // namespace

std::uint32_t CStringToKey(const char *text, bool caseInsensitive) {
    const std::size_t length = std::strlen(text);

    // The seed is the length itself, which is why the empty string hashes to 0.
    std::uint32_t hash = static_cast<std::uint32_t>(length);

    for (std::size_t i = 0; i < length; ++i) {
        // Signed on purpose: bytes >= 0x80 sign-extend to 0xFFFFFF80..0xFFFFFFFF.
        int character = static_cast<signed char>(text[i]);

        if (caseInsensitive && character >= 'A' && character <= 'Z') {
            character += 32;
        }

        hash = static_cast<std::uint32_t>(character) ^ RotateLeft4(hash);
    }

    return hash;
}
