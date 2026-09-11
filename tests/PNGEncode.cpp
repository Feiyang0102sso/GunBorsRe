#include "PNGEncode.h"
#include <zlib.h>
#include <fstream>
#include <filesystem>
#include <cstdio>
namespace {
const std::uint8_t kPNGSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
constexpr std::uint8_t kBitDepth8 = 8;
constexpr std::uint8_t kColorTypeRGBA = 6;
constexpr std::uint8_t kFilterNone = 0;
/** Append a big-endian uint32. */
void AppendBigU32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

/**
 * Append a complete chunk: length, type, body, CRC.
 * The CRC covers the type and the body but not the length.
 */
void AppendChunk(std::vector<std::uint8_t> &out, const char type[4],
                 const std::vector<std::uint8_t> &body) {
    AppendBigU32(out, static_cast<std::uint32_t>(body.size()));

    const std::size_t crcStart = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), body.begin(), body.end());

    const uLong crc = crc32(crc32(0, Z_NULL, 0), &out[crcStart],
                            static_cast<uInt>(out.size() - crcStart));
    AppendBigU32(out, static_cast<std::uint32_t>(crc));
}

}
bool PNGEncode(const PNGImage &image, const std::string &path) {
    const std::size_t expectedPixels =
        static_cast<std::size_t>(image.width) * image.height * 4;
    if (image.width == 0 || image.height == 0 || image.pixels.size() < expectedPixels) {
        std::printf("[png] nothing to encode\n");
        return false;
    }

    // --- raw scanlines: one zero filter byte per row, then the RGBA data ---
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(image.height) * (image.width * 4 + 1));
    for (std::uint32_t y = 0; y < image.height; ++y) {
        raw.push_back(kFilterNone);
        const std::uint8_t *row = &image.pixels[static_cast<std::size_t>(y) * image.width * 4];
        raw.insert(raw.end(), row, row + static_cast<std::size_t>(image.width) * 4);
    }

    // --- deflate ---
    uLongf compressedSize = compressBound(static_cast<uLong>(raw.size()));
    std::vector<std::uint8_t> compressed(compressedSize);
    const int result = compress2(compressed.data(), &compressedSize,
                                 raw.data(), static_cast<uLong>(raw.size()),
                                 Z_DEFAULT_COMPRESSION);
    if (result != Z_OK) {
        std::printf("[png] deflate failed (%d)\n", result);
        return false;
    }
    compressed.resize(compressedSize);

    // --- assemble ---
    std::vector<std::uint8_t> file(kPNGSignature, kPNGSignature + sizeof(kPNGSignature));

    std::vector<std::uint8_t> header;
    AppendBigU32(header, image.width);
    AppendBigU32(header, image.height);
    header.push_back(kBitDepth8);
    header.push_back(kColorTypeRGBA);
    header.push_back(0);  // compression method: deflate
    header.push_back(0);  // filter method: adaptive
    header.push_back(0);  // interlace method: none
    AppendChunk(file, "IHDR", header);

    AppendChunk(file, "IDAT", compressed);
    AppendChunk(file, "IEND", std::vector<std::uint8_t>());

    std::ofstream output(std::filesystem::u8path(path), std::ios::binary);
    if (!output.is_open()) {
        std::printf("[png] cannot write %s\n", path.c_str());
        return false;
    }
    output.write(reinterpret_cast<const char *>(file.data()),
                 static_cast<std::streamsize>(file.size()));

    std::printf("[png] wrote %s (%ux%u, %zu bytes)\n",
                path.c_str(), image.width, image.height, file.size());
    return true;
}
