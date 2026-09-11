/**
 * @file CBigFileReader.h
 * @brief Reader for the .big archive container.
 *
 * Port of com::glu::platform::components::CBigFileReader.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:356201 (ctor), :356788 (Open),
 *            :356239 (GetResourceDataStream), :356745 (Close).
 */

#ifndef GUN_BROS_RE_ENGINE_CBIGFILEREADER_H
#define GUN_BROS_RE_ENGINE_CBIGFILEREADER_H

#include "engine/resources/CAggregateResource.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Format constants
// ---------------------------------------------------------------------------

// Header magic, stored as the ASCII bytes 'F' 'G' 'I' 'B'.
constexpr std::uint32_t kBigMagic = 0x42494746u;

// Only version 1 archives (the Gun Bros series) are accepted. The engine
// compares just the low byte and bails out otherwise.
constexpr std::uint8_t kBigVersionGunBros = 1;

// Low byte of the header flags. When the sign bit is set, table1 entries are
// 8 bytes; otherwise they are 4. This is a layout selector, NOT a compression
// flag -- see the note in Open().
constexpr std::uint8_t kBigFlagWideTable1 = 0x80;

// Group hashes, i.e. the resource category stored in each table2 entry.
constexpr std::uint32_t kGroupStringPack = 0x69E4C505u;
constexpr std::uint32_t kGroupKeyset = 0x69E5D35Cu;
constexpr std::uint32_t kGroupPng = 0xB7178678u;
constexpr std::uint32_t kGroupBin = 0xF4E02223u;
constexpr std::uint32_t kGroupManifest = 0xF686AADCu;
constexpr std::uint32_t kGroupWav = 0xFD8A7754u;

// Logical resource ID 1 always points at this archive's string pack, and it is
// always table2[0].
constexpr std::uint32_t kStringPackResourceId = 1;

// ---------------------------------------------------------------------------
// Resource handles
// ---------------------------------------------------------------------------
//
// A handle is what a pack TOC stores and what CGameAssetRef carries. Layout:
//
//   bit 29        set when the target lives inside an aggregate resource
//   bits 16..23   aggregate selector, only meaningful when bit 29 is set
//   bits 0..14    the ID -- a logical resource ID, or a sub-resource ID
//
// The remaining high bits are a type tag that mirrors the group hash
// (0x0400 png, 0x0600 bin, 0x0A00 keyset, 0x1200 wav when read as handle >> 15).
// The engine never reads it, and neither do we.

// Set when the target lives inside an aggregate resource.
constexpr std::uint32_t kHandleAggregateFlag = 0x20000000u;

// Only the low 15 bits of a handle carry the ID.
constexpr std::uint32_t kHandleIdMask = 0x7FFFu;

// Aggregate selector: which aggregate resource to look in. The value 255 means
// "the one for the current locale"; anything else is matched against the
// aggregate ID table.
constexpr std::uint32_t kHandleSelectorShift = 16;
constexpr std::uint32_t kHandleSelectorMask = 0xFFu;
constexpr std::uint32_t kHandleSelectorLocale = 255;

// ---------------------------------------------------------------------------
// Tables
// ---------------------------------------------------------------------------

/**
 * Maps a run of consecutive logical IDs onto a run of table2 indices.
 * Logical IDs are sparse, so they are stored run-length encoded.
 */
struct BigTable1Range {
    std::uint32_t baseResourceId;    // first logical ID in this run
    std::uint16_t rangeLength;       // how many consecutive IDs it covers
    std::uint16_t table2StartIndex;  // index that baseResourceId maps to
};

/**
 * One resource. There is no size field: a resource runs until the offset of
 * the next entry, and the last one ends at the offset recorded in the footer.
 */
struct BigTable2Entry {
    std::uint32_t groupHash;       // resource category, see kGroup* above
    std::uint32_t resourceOffset;  // absolute offset into the archive
};

/**
 * Reads a .big archive and hands back decompressed resource payloads.
 *
 * The archive is kept open and read on demand; nothing is cached, matching the
 * original engine which streams straight off the file handle.
 */
class CBigFileReader {
public:
    CBigFileReader();
    ~CBigFileReader();

    CBigFileReader(const CBigFileReader &) = delete;
    CBigFileReader &operator=(const CBigFileReader &) = delete;

    /** Open an archive. Returns false if the file is missing or malformed. */
    bool Open(const std::string &path);

    /** Release the file handle and all tables. Safe to call when not open. */
    void Close();

    bool IsOpen() const { return m_file.is_open(); }

    std::uint32_t GetResourceCount() const { return m_table2EntryCount; }

    const std::vector<BigTable1Range> &GetTable1() const { return m_table1; }
    const std::vector<BigTable2Entry> &GetTable2() const { return m_table2; }

    /** Group hash of a resource, or 0 when the index is out of range. */
    std::uint32_t GetGroupHash(std::uint32_t table2Index) const;

    /** Byte span a resource block occupies in the archive, header included. */
    std::uint32_t GetBlockSize(std::uint32_t table2Index) const;

    /**
     * Compression flag from a resource's block header: kResourceUncompressed
     * or kResourceZlib. Reads the archive, so it is not free.
     */
    std::uint8_t GetCompressionFlag(std::uint32_t table2Index);

    /**
     * Read a resource by its table2 index, decompressing when needed.
     * An empty payload is legitimate: zero-length zlib streams are used as
     * placeholders for object types with no instances in this pack.
     */
    bool GetResourceByIndex(std::uint32_t table2Index, std::vector<std::uint8_t> &out);

    /** Read a resource by logical ID, resolving it through table1. */
    bool GetResourceById(std::uint32_t resourceId, std::vector<std::uint8_t> &out);

    /**
     * Read whatever a handle points at, following it into an aggregate
     * resource when bit 29 is set. This is the entry point callers want.
     */
    bool GetResourceByHandle(std::uint32_t handle, std::vector<std::uint8_t> &out);

    /**
     * Aggregate resource IDs, one per locale, indexed by SetLocaleIndex.
     * Comes from the ___INIT_DATA resource; see CResPackTOC::Bind.
     */
    void SetLocaleIdTable(std::vector<std::uint32_t> ids) {
        m_localeIds = std::move(ids);
    }

    /** Aggregate resource IDs addressed by an explicit selector. */
    void SetAggregateIdTable(std::vector<std::uint32_t> ids) {
        m_aggregateIds = std::move(ids);
    }

    /** Which entry of the locale table to use. The game only ships English. */
    void SetLocaleIndex(std::uint32_t index) { m_localeIndex = index; }

    /** Resolve a logical ID to a table2 index. Returns false when unmapped. */
    bool ResolveResourceId(std::uint32_t resourceId, std::uint32_t &table2Index) const;

private:
    bool ReadHeaderAndTables();
    bool ReadAt(std::uint32_t offset, void *dest, std::size_t size);

    /**
     * Make sure the aggregate a handle refers to is the one currently parsed,
     * loading it if not. Mirrors SetupAggregateForResourceId.
     */
    bool SetupAggregateForHandle(std::uint32_t handle);

    /** Resolve a handle's selector to the aggregate's own resource ID. */
    bool FindAggregateResourceId(std::uint32_t handle, std::uint32_t &aggregateId) const;

    std::ifstream m_file;
    std::uint32_t m_fileSize;

    std::uint8_t m_flags;
    std::uint32_t m_table1EntryCount;
    std::uint32_t m_table2EntryCount;
    std::uint32_t m_table1Offset;
    std::uint32_t m_table2Offset;

    std::vector<BigTable1Range> m_table1;
    std::vector<BigTable2Entry> m_table2;

    // Offset just past the last resource, taken from the 8-byte footer that
    // follows table2. Gives the last resource its length.
    std::uint32_t m_dataEndOffset;

    // Aggregate addressing, injected from ___INIT_DATA after Open().
    std::vector<std::uint32_t> m_localeIds;
    std::vector<std::uint32_t> m_aggregateIds;
    std::uint32_t m_localeIndex;

    // Exactly one aggregate is held parsed at a time, matching the original.
    // Handles arrive grouped in practice, so this hits nearly always.
    CAggregateResource m_aggregate;
    std::uint32_t m_loadedAggregateId;
};

/** Human-readable name for a group hash, "unknown" when unrecognised. */
const char *BigGroupName(std::uint32_t groupHash);

#endif  // GUN_BROS_RE_ENGINE_CBIGFILEREADER_H
