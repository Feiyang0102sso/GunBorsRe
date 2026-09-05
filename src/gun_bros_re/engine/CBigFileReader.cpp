/**
 * @file CBigFileReader.cpp
 * @brief Reader for the .big archive container.
 *
 * Port of com::glu::platform::components::CBigFileReader.
 * All integers in the container are little-endian. Note that packTOC_*.dat,
 * handled elsewhere, is big-endian -- do not share code between the two.
 */

#include "engine/CBigFileReader.h"

#include <zlib.h>

#include <cstdio>
#include <cstring>

namespace {

/** Read a little-endian uint32 from a byte buffer. */
std::uint32_t ReadU32(const std::uint8_t *bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

/** Read a little-endian uint16 from a byte buffer. */
std::uint16_t ReadU16(const std::uint8_t *bytes) {
    return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
}

}  // namespace

CBigFileReader::CBigFileReader()
    : m_fileSize(0),
      m_flags(0),
      m_table1EntryCount(0),
      m_table2EntryCount(0),
      m_table1Offset(0),
      m_table2Offset(0),
      m_dataEndOffset(0) {}

CBigFileReader::~CBigFileReader() {
    Close();
}

void CBigFileReader::Close() {
    if (m_file.is_open()) {
        m_file.close();
    }
    m_file.clear();

    m_fileSize = 0;
    m_flags = 0;
    m_table1EntryCount = 0;
    m_table2EntryCount = 0;
    m_table1Offset = 0;
    m_table2Offset = 0;
    m_dataEndOffset = 0;

    m_table1.clear();
    m_table2.clear();
}

bool CBigFileReader::Open(const std::string &path) {
    Close();

    m_file.open(path, std::ios::binary);
    if (!m_file.is_open()) {
        std::printf("[big] open failed: %s\n", path.c_str());
        return false;
    }

    m_file.seekg(0, std::ios::end);
    m_fileSize = static_cast<std::uint32_t>(m_file.tellg());

    if (!ReadHeaderAndTables()) {
        Close();
        return false;
    }

    std::printf("[big] opened %s: %u resources, flags=0x%02X\n",
                path.c_str(), m_table2EntryCount, m_flags);
    return true;
}

bool CBigFileReader::ReadAt(std::uint32_t offset, void *dest, std::size_t size) {
    if (offset + size > m_fileSize) {
        return false;
    }
    m_file.seekg(offset, std::ios::beg);
    m_file.read(static_cast<char *>(dest), static_cast<std::streamsize>(size));
    return static_cast<std::size_t>(m_file.gcount()) == size;
}

bool CBigFileReader::ReadHeaderAndTables() {
    // Fixed 0x20-byte header.
    std::uint8_t header[32];
    if (!ReadAt(0, header, sizeof(header))) {
        std::printf("[big] header read failed\n");
        return false;
    }

    const std::uint32_t magic = ReadU32(&header[0]);
    if (magic != kBigMagic) {
        std::printf("[big] bad magic 0x%08X\n", magic);
        return false;
    }

    // The engine reads version as two separate bytes and only checks the low
    // one. Version 2 is the Contract Killer series and is rejected here.
    const std::uint8_t version = header[4];
    if (version != kBigVersionGunBros) {
        std::printf("[big] unsupported version %u\n", version);
        return false;
    }

    // Only the low byte of the flags is kept, and the engine stores it in a
    // SIGNED char. The `flags >= 0` test in the original code is therefore a
    // test of bit 7, selecting the table1 entry width. This is the real meaning
    // of 0x80 -- it is not a compression flag, despite sharing the value with
    // the per-resource one.
    m_flags = header[6];

    m_table1Offset = ReadU32(&header[8]);
    m_table1EntryCount = ReadU32(&header[12]);
    m_table2Offset = ReadU32(&header[16]);
    m_table2EntryCount = ReadU32(&header[20]);
    // header[24] is dataBlockOffset and header[28] is dataBlockSize. Both are
    // fully derivable from the fields above; the engine reads and discards
    // them, so we do the same.

    if ((m_flags & kBigFlagWideTable1) == 0) {
        // 4-byte table1 entries. No sample in the Gun Bros packs uses this and
        // the layout is unverified, so refuse rather than guess.
        std::printf("[big] narrow table1 (flags=0x%02X) not supported yet\n", m_flags);
        return false;
    }

    // --- table1: 8 bytes per entry ---
    if (m_table1EntryCount > 0) {
        std::vector<std::uint8_t> raw(m_table1EntryCount * 8);
        if (!ReadAt(m_table1Offset, raw.data(), raw.size())) {
            std::printf("[big] table1 read failed\n");
            return false;
        }
        m_table1.resize(m_table1EntryCount);
        for (std::uint32_t i = 0; i < m_table1EntryCount; ++i) {
            const std::uint8_t *entry = &raw[i * 8];
            m_table1[i].baseResourceId = ReadU32(&entry[0]);
            m_table1[i].rangeLength = ReadU16(&entry[4]);
            m_table1[i].table2StartIndex = ReadU16(&entry[6]);
        }
    }

    // --- table2: 8 bytes per entry, plus an 8-byte footer ---
    if (m_table2EntryCount > 0) {
        const std::size_t byteCount = m_table2EntryCount * 8 + 8;
        std::vector<std::uint8_t> raw(byteCount);
        if (!ReadAt(m_table2Offset, raw.data(), raw.size())) {
            std::printf("[big] table2 read failed\n");
            return false;
        }
        m_table2.resize(m_table2EntryCount);
        for (std::uint32_t i = 0; i < m_table2EntryCount; ++i) {
            const std::uint8_t *entry = &raw[i * 8];
            m_table2[i].groupHash = ReadU32(&entry[0]);
            m_table2[i].resourceOffset = ReadU32(&entry[4]);
        }
        // Footer is { uint32 zero, uint32 endOffset }. The second word closes
        // out the last resource, which has no following entry to bound it.
        m_dataEndOffset = ReadU32(&raw[m_table2EntryCount * 8 + 4]);
    }

    return true;
}

std::uint32_t CBigFileReader::GetGroupHash(std::uint32_t table2Index) const {
    if (table2Index >= m_table2.size()) {
        return 0;
    }
    return m_table2[table2Index].groupHash;
}

std::uint32_t CBigFileReader::GetBlockSize(std::uint32_t table2Index) const {
    if (table2Index >= m_table2.size()) {
        return 0;
    }
    const std::uint32_t start = m_table2[table2Index].resourceOffset;
    std::uint32_t end = m_dataEndOffset;
    if (table2Index + 1 < m_table2.size()) {
        end = m_table2[table2Index + 1].resourceOffset;
    }
    if (end < start) {
        return 0;
    }
    return end - start;
}

bool CBigFileReader::ResolveResourceId(std::uint32_t resourceId,
                                       std::uint32_t &table2Index) const {
    // Logical IDs are sparse; table1 stores them as runs.
    for (const BigTable1Range &range : m_table1) {
        const std::uint32_t first = range.baseResourceId;
        const std::uint32_t last = first + range.rangeLength;
        if (resourceId >= first && resourceId < last) {
            table2Index = range.table2StartIndex + (resourceId - first);
            return table2Index < m_table2EntryCount;
        }
    }
    return false;
}

bool CBigFileReader::GetResourceById(std::uint32_t resourceId,
                                     std::vector<std::uint8_t> &out) {
    // TODO: aggregate handles (kHandleAggregateFlag) address entries *inside*
    // a string pack and need SetupAggregateForResourceId. Not needed yet.
    if ((resourceId & kHandleAggregateFlag) != 0) {
        std::printf("[big] aggregate handle 0x%08X not supported yet\n", resourceId);
        return false;
    }

    std::uint32_t table2Index = 0;
    if (!ResolveResourceId(resourceId, table2Index)) {
        return false;
    }
    return GetResourceByIndex(table2Index, out);
}

bool CBigFileReader::GetResourceByIndex(std::uint32_t table2Index,
                                        std::vector<std::uint8_t> &out) {
    out.clear();

    if (table2Index >= m_table2.size()) {
        return false;
    }

    const std::uint32_t blockOffset = m_table2[table2Index].resourceOffset;
    const std::uint32_t blockSize = GetBlockSize(table2Index);
    if (blockSize < 4) {
        return false;
    }

    // Block header: magic, reserved, compressionFlag, padding.
    std::uint8_t blockHeader[4];
    if (!ReadAt(blockOffset, blockHeader, sizeof(blockHeader))) {
        return false;
    }
    if (blockHeader[0] != kResourceBlockMagic) {
        std::printf("[big] resource %u: bad block magic 0x%02X\n",
                    table2Index, blockHeader[0]);
        return false;
    }

    const std::uint8_t compression = blockHeader[2];

    if (compression == kResourceUncompressed) {
        const std::uint32_t payloadSize = blockSize - 4;
        out.resize(payloadSize);
        if (payloadSize == 0) {
            return true;
        }
        return ReadAt(blockOffset + 4, out.data(), payloadSize);
    }

    if (compression != kResourceZlib) {
        std::printf("[big] resource %u: unknown compression 0x%02X\n",
                    table2Index, compression);
        return false;
    }

    // Compressed layout: uint32 originalSize, uint32 compressedSize, then the
    // zlib stream. Block size is always exactly 12 + compressedSize, with no
    // alignment padding.
    if (blockSize < 12) {
        return false;
    }
    std::uint8_t sizes[8];
    if (!ReadAt(blockOffset + 4, sizes, sizeof(sizes))) {
        return false;
    }
    const std::uint32_t originalSize = ReadU32(&sizes[0]);
    const std::uint32_t compressedSize = ReadU32(&sizes[4]);

    if (compressedSize > blockSize - 12) {
        std::printf("[big] resource %u: compressed size %u exceeds block\n",
                    table2Index, compressedSize);
        return false;
    }

    std::vector<std::uint8_t> compressed(compressedSize);
    if (compressedSize > 0 && !ReadAt(blockOffset + 12, compressed.data(), compressedSize)) {
        return false;
    }

    // originalSize == 0 is valid: the payload is a well-formed zlib stream that
    // inflates to nothing. These are placeholders for object types with zero
    // instances in this pack.
    out.resize(originalSize);
    if (originalSize == 0) {
        return true;
    }

    uLongf destLen = originalSize;
    const int result = uncompress(out.data(), &destLen,
                                  compressed.data(), compressedSize);
    if (result != Z_OK) {
        std::printf("[big] resource %u: inflate failed (%d)\n", table2Index, result);
        out.clear();
        return false;
    }
    if (destLen != originalSize) {
        std::printf("[big] resource %u: size mismatch, got %lu want %u\n",
                    table2Index, destLen, originalSize);
        out.resize(destLen);
    }
    return true;
}

const char *BigGroupName(std::uint32_t groupHash) {
    switch (groupHash) {
        case kGroupStringPack: return "string_pack";
        case kGroupKeyset:     return "keyset";
        case kGroupPng:        return "png";
        case kGroupBin:        return "bin";
        case kGroupManifest:   return "manifest";
        case kGroupWav:        return "wav";
        default:               return "unknown";
    }
}
