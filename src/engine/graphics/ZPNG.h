/**
 * @file ZPNG.h
 * @brief PNG decoder, cut down to the shapes this game actually ships.
 *
 * Stands in for platform/shared/graphics/src/CPNG.cpp, which wrapped an
 * embedded libpng. A survey of all 1534 PNGs across the 13 packs found only
 * three variants, all 8-bit and none interlaced:
 *
 *   colour type 6 (RGBA)     1151
 *   colour type 2 (RGB)       379
 *   colour type 3 (palette)     4   (one of them with tRNS)
 *
 * so this handles exactly those and refuses anything else rather than
 * pretending to be a general decoder. Output is always RGBA8, which is what
 * the texture upload wants regardless of the source layout.
 */

#ifndef GUN_BROS_RE_ENGINE_ZPNG_H
#define GUN_BROS_RE_ENGINE_ZPNG_H

#include <cstdint>
#include <string>
#include <vector>

/** A decoded image: tightly packed RGBA8, top row first. */
struct ZPNGImage {
    std::uint32_t width;
    std::uint32_t height;
    std::vector<std::uint8_t> pixels;  // width * height * 4

    ZPNGImage() : width(0), height(0) {}
};

/**
 * Decode a PNG held in memory.
 *
 * @param data  Whole PNG file, signature included.
 * @param out   Receives the RGBA8 image. Untouched on failure.
 * @return false, with a printed reason, when the file is malformed or uses a
 *         feature outside the three variants above.
 */
bool PNGDecode(const std::vector<std::uint8_t> &data, ZPNGImage &out);

#endif  // GUN_BROS_RE_ENGINE_CPNG_H
