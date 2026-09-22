/**
 * @file CAggregateResource.cpp
 * @brief Container resource that packs many small sub-resources into one blob.
 * 		  eg read the whole string pack and return each single string bytes (do not interpter it)
 */

#include "engine/resources/CAggregateResource.h"
#include "engine/resources/CArrayInputStream.h"

#include <zlib.h>

#include <cstdio>
#include <cstring>

bool DecodeResourceBlock(const std::uint8_t *block, std::size_t blockSize,
                         std::vector<std::uint8_t> &out) {
    out.clear();

    if (blockSize < 4) {
        std::printf("[agg] block too small (%zu bytes)\n", blockSize);
        return false;
    }
    if (block[0] != kResourceBlockMagic) {
        std::printf("[agg] bad block magic 0x%02X\n", block[0]);
        return false;
    }

    const std::uint8_t compression = block[2];

    if (compression == kResourceUncompressed) {
        out.assign(block + 4, block + blockSize);
        return true;
    }

    if (compression != kResourceZlib) {
        std::printf("[agg] unknown compression 0x%02X\n", compression);
        return false;
    }

    // Compressed layout: uint32 originalSize, uint32 compressedSize, zlib stream.
    if (blockSize < 12) {
        return false;
    }
    CArrayInputStream header(block + 4, blockSize - 4);
    const std::uint32_t originalSize = header.ReadUInt32();
    const std::uint32_t compressedSize = header.ReadUInt32();
    if (compressedSize > blockSize - 12) {
        std::printf("[agg] compressed size %u exceeds block\n", compressedSize);
        return false;
    }

    out.resize(originalSize);
    if (originalSize == 0) {
        return true;
    }

    uLongf destLen = originalSize;
    const int result = uncompress(out.data(), &destLen, block + 12, compressedSize);
    if (result != Z_OK) {
        std::printf("[agg] inflate failed (%d)\n", result);
        out.clear();
        return false;
    }
    out.resize(destLen);
    return true;
}

CAggregateResource::CAggregateResource() : m_endOffset(0) {}

void CAggregateResource::Destroy() {
    m_entries.clear();
    m_payload.clear();
    m_endOffset = 0;
}

bool CAggregateResource::LoadTOC(std::vector<std::uint8_t> payload) {
    Destroy();

    if (payload.size() < 4) {
        std::printf("[agg] payload too small for a TOC\n");
        return false;
    }

    CArrayInputStream cursor(payload);
    const std::uint16_t flags = cursor.ReadUInt16();
    const std::uint16_t entryCount = cursor.ReadUInt16();

    if (entryCount == 0) {
        // Legal but useless to us: the whole payload is one unindexed data area.
        std::printf("[agg] TOC has no entries (flags=0x%04X)\n", flags);
        return false;
    }

    const bool consecutiveIds = (flags & kAggregateFlagConsecutiveIds) != 0;
    const bool wideOffsets = (flags & kAggregateFlagWideOffsets) != 0;
    const bool hasMimeKeys = (flags & kAggregateFlagHasMimeKeys) != 0;

    m_entries.resize(entryCount);

    // --- ids and offsets ---
    std::uint32_t baseId = 0;
    if (consecutiveIds) {
        baseId = cursor.ReadUInt16();
    }
    for (std::uint16_t i = 0; i < entryCount; ++i) {
        if (consecutiveIds) {
            m_entries[i].subId = baseId + i;
        } else {
            m_entries[i].subId = cursor.ReadUInt16();
        }

        if (wideOffsets) {
            m_entries[i].offset = cursor.ReadUInt32();
        } else {
            m_entries[i].offset = cursor.ReadUInt16();
        }

        m_entries[i].mimeKey = 0;
    }

    // One extra offset closes out the last entry. It is always a uint32, even
    // when the entry offsets themselves are 16-bit.
    m_endOffset = cursor.ReadUInt32();

    // --- mime keys ---
    if (hasMimeKeys) {
        for (std::uint16_t i = 0; i < entryCount; ++i) {
            m_entries[i].mimeKey = cursor.ReadUInt32();
        }
    }

    if (cursor.Overran()) {
        std::printf("[agg] TOC truncated (flags=0x%04X, %u entries)\n", flags, entryCount);
        Destroy();
        return false;
    }

    // Offsets are absolute within the payload, so the data area begins exactly
    // where the TOC ends and the closing offset is the payload length.
    if (m_endOffset > payload.size()) {
        std::printf("[agg] end offset %u past payload (%zu bytes)\n",
                    m_endOffset, payload.size());
        Destroy();
        return false;
    }

    m_payload = std::move(payload);
    return true;
}

int CAggregateResource::FindEntry(std::uint32_t subId) const {
    const std::uint32_t wanted = subId & kAggregateSubIdMask;
    // Linear, exactly as the original. The tables top out in the hundreds.
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].subId == wanted) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool CAggregateResource::GetEntrySpan(int entryIndex, std::uint32_t &begin,
                                      std::uint32_t &end) const {
    begin = m_entries[entryIndex].offset;

    const std::size_t next = static_cast<std::size_t>(entryIndex) + 1;
    if (next < m_entries.size()) {
        end = m_entries[next].offset;
    } else {
        end = m_endOffset;
    }

    if (end < begin || end > m_payload.size()) {
        return false;
    }
    return true;
}

bool CAggregateResource::GetSubResource(std::uint32_t subId,
                                        std::vector<std::uint8_t> &out) const {
    out.clear();

    const int entryIndex = FindEntry(subId);
    if (entryIndex < 0) {
        return false;
    }

    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    if (!GetEntrySpan(entryIndex, begin, end)) {
        std::printf("[agg] sub-resource %u has a bad span\n", subId);
        return false;
    }

    return DecodeResourceBlock(m_payload.data() + begin, end - begin, out);
}

std::uint32_t CAggregateResource::GetMimeKey(std::uint32_t subId) const {
    const int entryIndex = FindEntry(subId);
    if (entryIndex < 0) {
        return 0;
    }
    return m_entries[entryIndex].mimeKey;
}

std::uint32_t CAggregateResource::GetSubResourceIdAt(std::uint32_t entryIndex) const {
    if (entryIndex >= m_entries.size()) {
        return 0;
    }
    return m_entries[entryIndex].subId;
}
