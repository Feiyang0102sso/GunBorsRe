/**
 * @file ZPNG.cpp
 * @brief PNG decoder, cut down to the shapes this game actually ships.
 */

#include "engine/graphics/ZPNG.h"

#include <zlib.h>

#include <cstdio>
#include <cstring>
#include <fstream>

namespace {

// ---------------------------------------------------------------------------
// Format constants
// ---------------------------------------------------------------------------

const std::uint8_t kPNGSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

// The only bit depth in these packs. Anything else is rejected.
constexpr std::uint8_t kBitDepth8 = 8;

// PNG colour types.
constexpr std::uint8_t kColorTypeGray = 0;
constexpr std::uint8_t kColorTypeRGB = 2;
constexpr std::uint8_t kColorTypePalette = 3;
constexpr std::uint8_t kColorTypeGrayAlpha = 4;
constexpr std::uint8_t kColorTypeRGBA = 6;

// Per-scanline filter types, PNG spec section 9.
constexpr std::uint8_t kFilterNone = 0;
constexpr std::uint8_t kFilterSub = 1;
constexpr std::uint8_t kFilterUp = 2;
constexpr std::uint8_t kFilterAverage = 3;
constexpr std::uint8_t kFilterPaeth = 4;

// A chunk header is length + type; the trailing CRC is 4 more bytes.
constexpr std::size_t kChunkHeaderSize = 8;
constexpr std::size_t kChunkCrcSize = 4;

// Guard against absurd allocations from a corrupt header.
constexpr std::uint32_t kMaxDimension = 8192;

/** Read a big-endian uint32. Every multi-byte field in PNG is big-endian. */
std::uint32_t ReadBigU32(const std::uint8_t *bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) |
           static_cast<std::uint32_t>(bytes[3]);
}

/** Source bytes per pixel, which is also the filter's left-neighbour distance. */
std::uint32_t BytesPerPixel(std::uint8_t colorType) {
    switch (colorType) {
        case kColorTypeRGB:     return 3;
        case kColorTypeRGBA:    return 4;
        case kColorTypePalette: return 1;
        default:                return 0;
    }
}

/**
 * The Paeth predictor from the PNG spec: pick whichever of the three
 * neighbours the linear estimate a + b - c comes closest to.
 */
std::uint8_t PaethPredictor(int left, int above, int upperLeft) {
    const int estimate = left + above - upperLeft;

    int distanceLeft = estimate - left;
    if (distanceLeft < 0) {
        distanceLeft = -distanceLeft;
    }
    int distanceAbove = estimate - above;
    if (distanceAbove < 0) {
        distanceAbove = -distanceAbove;
    }
    int distanceUpperLeft = estimate - upperLeft;
    if (distanceUpperLeft < 0) {
        distanceUpperLeft = -distanceUpperLeft;
    }

    if (distanceLeft <= distanceAbove && distanceLeft <= distanceUpperLeft) {
        return static_cast<std::uint8_t>(left);
    }
    if (distanceAbove <= distanceUpperLeft) {
        return static_cast<std::uint8_t>(above);
    }
    return static_cast<std::uint8_t>(upperLeft);
}

/**
 * Undo the per-scanline filters in place.
 *
 * `raw` holds height rows of (1 filter byte + rowBytes data bytes). On success
 * `unfiltered` holds the same rows with the filter bytes gone.
 */
bool Unfilter(const std::vector<std::uint8_t> &raw, std::uint32_t height,
              std::uint32_t rowBytes, std::uint32_t bytesPerPixel,
              std::vector<std::uint8_t> &unfiltered) {
    const std::size_t expected =
        static_cast<std::size_t>(height) * (static_cast<std::size_t>(rowBytes) + 1);
    if (raw.size() < expected) {
        std::printf("[png] inflated to %zu bytes, expected %zu\n", raw.size(), expected);
        return false;
    }

    unfiltered.assign(static_cast<std::size_t>(height) * rowBytes, 0);

    for (std::uint32_t y = 0; y < height; ++y) {
        const std::size_t rawRowStart = static_cast<std::size_t>(y) * (rowBytes + 1);
        const std::uint8_t filterType = raw[rawRowStart];
        const std::uint8_t *source = &raw[rawRowStart + 1];

        std::uint8_t *current = &unfiltered[static_cast<std::size_t>(y) * rowBytes];
        // Row above; on the first row the spec says treat it as all zeroes.
        const std::uint8_t *previous =
            (y > 0) ? &unfiltered[static_cast<std::size_t>(y - 1) * rowBytes] : nullptr;

        for (std::uint32_t x = 0; x < rowBytes; ++x) {
            const int left = (x >= bytesPerPixel) ? current[x - bytesPerPixel] : 0;
            const int above = (previous != nullptr) ? previous[x] : 0;
            const int upperLeft =
                (previous != nullptr && x >= bytesPerPixel) ? previous[x - bytesPerPixel] : 0;

            int value = source[x];
            switch (filterType) {
                case kFilterNone:
                    break;
                case kFilterSub:
                    value += left;
                    break;
                case kFilterUp:
                    value += above;
                    break;
                case kFilterAverage:
                    value += (left + above) / 2;
                    break;
                case kFilterPaeth:
                    value += PaethPredictor(left, above, upperLeft);
                    break;
                default:
                    std::printf("[png] row %u has unknown filter %u\n", y, filterType);
                    return false;
            }
            current[x] = static_cast<std::uint8_t>(value & 0xFF);
        }
    }

    return true;
}

/** Widen one decoded scanline to RGBA8. */
void ExpandToRGBA(const std::uint8_t *source, std::uint32_t width, std::uint8_t colorType,
                  const std::vector<std::uint8_t> &palette,
                  const std::vector<std::uint8_t> &paletteAlpha, std::uint8_t *destination) {
    for (std::uint32_t x = 0; x < width; ++x) {
        std::uint8_t *pixel = &destination[static_cast<std::size_t>(x) * 4];

        if (colorType == kColorTypeRGBA) {
            std::memcpy(pixel, &source[static_cast<std::size_t>(x) * 4], 4);
            continue;
        }

        if (colorType == kColorTypeRGB) {
            std::memcpy(pixel, &source[static_cast<std::size_t>(x) * 3], 3);
            pixel[3] = 0xFF;
            continue;
        }

        // Palette: the sample is an index into PLTE, with tRNS supplying alpha
        // for the leading entries and everything past it fully opaque.
        const std::uint8_t index = source[x];
        const std::size_t paletteOffset = static_cast<std::size_t>(index) * 3;
        if (paletteOffset + 2 < palette.size()) {
            pixel[0] = palette[paletteOffset + 0];
            pixel[1] = palette[paletteOffset + 1];
            pixel[2] = palette[paletteOffset + 2];
        } else {
            pixel[0] = 0;
            pixel[1] = 0;
            pixel[2] = 0;
        }
        pixel[3] = (index < paletteAlpha.size()) ? paletteAlpha[index] : 0xFF;
    }
}

}  // namespace

bool PNGDecode(const std::vector<std::uint8_t> &data, ZPNGImage &out) {
    if (data.size() < sizeof(kPNGSignature) ||
        std::memcmp(data.data(), kPNGSignature, sizeof(kPNGSignature)) != 0) {
        std::printf("[png] not a PNG\n");
        return false;
    }

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint8_t bitDepth = 0;
    std::uint8_t colorType = 0;
    bool sawHeader = false;

    std::vector<std::uint8_t> palette;
    std::vector<std::uint8_t> paletteAlpha;
    std::vector<std::uint8_t> compressed;

    // --- walk the chunks ---
    std::size_t position = sizeof(kPNGSignature);
    while (position + kChunkHeaderSize <= data.size()) {
        const std::uint32_t chunkLength = ReadBigU32(&data[position]);
        const std::uint8_t *chunkType = &data[position + 4];
        const std::size_t bodyStart = position + kChunkHeaderSize;

        if (bodyStart + chunkLength + kChunkCrcSize > data.size()) {
            std::printf("[png] chunk at %zu runs past the file\n", position);
            return false;
        }
        const std::uint8_t *body = &data[bodyStart];

        if (std::memcmp(chunkType, "IHDR", 4) == 0) {
            if (chunkLength != 13) {
                std::printf("[png] IHDR is %u bytes, expected 13\n", chunkLength);
                return false;
            }
            width = ReadBigU32(&body[0]);
            height = ReadBigU32(&body[4]);
            bitDepth = body[8];
            colorType = body[9];
            const std::uint8_t compressionMethod = body[10];
            const std::uint8_t filterMethod = body[11];
            const std::uint8_t interlaceMethod = body[12];

            if (width == 0 || height == 0 || width > kMaxDimension || height > kMaxDimension) {
                std::printf("[png] implausible size %ux%u\n", width, height);
                return false;
            }
            if (bitDepth != kBitDepth8) {
                std::printf("[png] bit depth %u unsupported (only 8)\n", bitDepth);
                return false;
            }
            if (colorType == kColorTypeGray || colorType == kColorTypeGrayAlpha) {
                std::printf("[png] greyscale unsupported; no pack contains one\n");
                return false;
            }
            if (colorType != kColorTypeRGB && colorType != kColorTypeRGBA &&
                colorType != kColorTypePalette) {
                std::printf("[png] colour type %u unsupported\n", colorType);
                return false;
            }
            if (compressionMethod != 0 || filterMethod != 0) {
                std::printf("[png] non-standard compression/filter method\n");
                return false;
            }
            if (interlaceMethod != 0) {
                std::printf("[png] interlaced images unsupported\n");
                return false;
            }
            sawHeader = true;

        } else if (std::memcmp(chunkType, "PLTE", 4) == 0) {
            palette.assign(body, body + chunkLength);

        } else if (std::memcmp(chunkType, "tRNS", 4) == 0) {
            // For palette images tRNS is one alpha byte per palette entry.
            paletteAlpha.assign(body, body + chunkLength);

        } else if (std::memcmp(chunkType, "IDAT", 4) == 0) {
            // Image data can be split across several IDATs; the zlib stream
            // runs across the concatenation, not per chunk.
            compressed.insert(compressed.end(), body, body + chunkLength);

        } else if (std::memcmp(chunkType, "IEND", 4) == 0) {
            break;
        }

        position = bodyStart + chunkLength + kChunkCrcSize;
    }

    if (!sawHeader) {
        std::printf("[png] no IHDR\n");
        return false;
    }
    if (compressed.empty()) {
        std::printf("[png] no image data\n");
        return false;
    }
    if (colorType == kColorTypePalette && palette.empty()) {
        std::printf("[png] palette image with no PLTE\n");
        return false;
    }

    // --- inflate ---
    const std::uint32_t bytesPerPixel = BytesPerPixel(colorType);
    const std::uint32_t rowBytes = width * bytesPerPixel;
    // Each row carries one leading filter byte.
    const std::size_t rawSize =
        static_cast<std::size_t>(height) * (static_cast<std::size_t>(rowBytes) + 1);

    std::vector<std::uint8_t> raw(rawSize);
    uLongf inflatedSize = static_cast<uLongf>(rawSize);
    const int result = uncompress(raw.data(), &inflatedSize,
                                  compressed.data(),
                                  static_cast<uLong>(compressed.size()));
    if (result != Z_OK) {
        std::printf("[png] inflate failed (%d)\n", result);
        return false;
    }
    raw.resize(inflatedSize);

    // --- unfilter and widen ---
    std::vector<std::uint8_t> unfiltered;
    if (!Unfilter(raw, height, rowBytes, bytesPerPixel, unfiltered)) {
        return false;
    }

    out.width = width;
    out.height = height;
    out.pixels.assign(static_cast<std::size_t>(width) * height * 4, 0);

    for (std::uint32_t y = 0; y < height; ++y) {
        ExpandToRGBA(&unfiltered[static_cast<std::size_t>(y) * rowBytes], width, colorType,
                     palette, paletteAlpha,
                     &out.pixels[static_cast<std::size_t>(y) * width * 4]);
    }

    return true;
}
