/**
 * @file CAggregateResource.h
 * @brief Container resource that packs many small sub-resources into one blob.
 *
 * Port of com::glu::platform::components::CAggregateResource.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:355669 (LoadTOC), :355548 (GetSize),
 *            :355572 (GetOffset), :355603 (GetMimeKey)
 *
 * Most small resources -- all UI text, for one -- do not get their own .big
 * entry. They live inside an aggregate, which is itself an ordinary resource.
 * A handle with bit 29 set addresses one of these; see CBigFileReader.
 */

#ifndef GUN_BROS_RE_ENGINE_CAGGREGATERESOURCE_H
#define GUN_BROS_RE_ENGINE_CAGGREGATERESOURCE_H

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Resource block format
// ---------------------------------------------------------------------------

// Every resource block starts with this byte, top-level and nested alike.
constexpr std::uint8_t kResourceBlockMagic = 0x04;

// Resource block compression flag, stored in block[2].
constexpr std::uint8_t kResourceUncompressed = 0x00;
constexpr std::uint8_t kResourceZlib = 0x80;

// ---------------------------------------------------------------------------
// TOC header flags
// ---------------------------------------------------------------------------

// Sub-resource IDs are consecutive: the header stores one uint16 base ID and
// entry i covers base + i. When clear, every entry carries its own uint16 ID.
constexpr std::uint16_t kAggregateFlagConsecutiveIds = 0x8000;

// Offsets are uint32 rather than uint16.
constexpr std::uint16_t kAggregateFlagWideOffsets = 0x4000;

// A uint32 mime key (a group hash) follows the offset table, one per entry.
constexpr std::uint16_t kAggregateFlagHasMimeKeys = 0x2000;

// Sub-resource IDs are only 15 bits wide; the upper bit of the handle's low
// half is not part of the ID.
constexpr std::uint32_t kAggregateSubIdMask = 0x7FFF;

/**
 * Parsed table of contents for one aggregate resource.
 *
 * The whole aggregate payload is held in memory, because sub-resource offsets
 * are absolute within it and the sub-resources are small by construction.
 */
class CAggregateResource {
public:
    CAggregateResource();

    /**
     * Parse the TOC at the head of an aggregate payload and take ownership of
     * it. Returns false if the payload is truncated or malformed.
     */
    bool LoadTOC(std::vector<std::uint8_t> payload);

    /** Drop the payload and the TOC. */
    void Destroy();

    bool IsLoaded() const { return !m_entries.empty(); }

    std::uint32_t GetSubResourceCount() const {
        return static_cast<std::uint32_t>(m_entries.size());
    }

    /**
     * Fetch one sub-resource, decoded through the ordinary block header so a
     * compressed sub-resource comes back inflated.
     *
     * @param subId Sub-resource ID; only the low 15 bits are used.
     */
    bool GetSubResource(std::uint32_t subId, std::vector<std::uint8_t> &out) const;

    /** Group hash recorded for a sub-resource, or 0 when there is no mime table. */
    std::uint32_t GetMimeKey(std::uint32_t subId) const;

    /** Sub-resource ID at a table position, for enumeration. */
    std::uint32_t GetSubResourceIdAt(std::uint32_t entryIndex) const;

private:
    struct AggregateEntry {
        std::uint32_t subId;
        std::uint32_t offset;   // absolute offset into m_payload
        std::uint32_t mimeKey;  // 0 when the TOC carries no mime table
    };

    /** Table position holding a sub-resource ID, or -1 when absent. */
    int FindEntry(std::uint32_t subId) const;

    /** Byte span of an entry, bounded by the next entry's offset. */
    bool GetEntrySpan(int entryIndex, std::uint32_t &begin, std::uint32_t &end) const;

    std::vector<AggregateEntry> m_entries;
    std::vector<std::uint8_t> m_payload;

    // Offset just past the last sub-resource. The original stores it as one
    // extra slot on the end of the offset array.
    std::uint32_t m_endOffset;
};

/**
 * Decode a resource block: 4-byte header, then either the raw payload or a
 * uint32 pair of sizes followed by a zlib stream.
 *
 * Shared by CBigFileReader and CAggregateResource -- sub-resources carry the
 * exact same block header as top-level ones, so the format nests cleanly.
 */
bool DecodeResourceBlock(const std::uint8_t *block, std::size_t blockSize,
                         std::vector<std::uint8_t> &out);

#endif  // GUN_BROS_RE_ENGINE_CAGGREGATERESOURCE_H
