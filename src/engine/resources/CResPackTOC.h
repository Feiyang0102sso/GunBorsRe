/**
 * @file CResPackTOC.h
 * @brief One resource pack: its .big archive plus the name -> handle table.
 *
 * Port of CResPackTOC (src/gunbros/resPackTOC.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:132351 (ctor), :132377 (GetResValue),
 *            :132484 (Bind)
 *
 * A pack is addressed by the hash of its short name ("pack1"), and every
 * resource inside it is addressed by the hash of its own name. The table that
 * maps those name hashes to handles is itself a resource in the archive, whose
 * logical ID comes from packTOC_xga.dat.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CRESPACKTOC_H
#define GUN_BROS_RE_GUN_BROS_CRESPACKTOC_H

#include "engine/resources/CBigFileReader.h"

#include <cstdint>
#include <string>
#include <vector>

// Name of the resource holding this pack's locale and aggregate tables. Read
// during Bind, before any aggregate handle can be resolved.
extern const char *const kInitDataResourceName;

/** One entry of a pack TOC: a name hash and what it points at. */
struct PackTOCEntry {
    std::uint32_t nameKey;  // CStringToKey of the resource name
    std::uint32_t handle;   // see the handle layout in CBigFileReader.h
};

/**
 * A single resource pack.
 *
 * Owns the archive reader, so a resource fetched through here is already
 * decompressed and already followed into its aggregate when it lives in one.
 */
class CResPackTOC {
public:
    /**
     * @param fullName  Archive name without extension, e.g. "pack1_xga".
     * @param shortName Art-set-free name used for hashing, e.g. "pack1".
     */
    CResPackTOC(const std::string &fullName, const std::string &shortName);

    CResPackTOC(const CResPackTOC &) = delete;
    CResPackTOC &operator=(const CResPackTOC &) = delete;

    /**
     * Open the archive, read the TOC resource, then read ___INIT_DATA and hand
     * its locale and aggregate tables to the reader.
     *
     * @param bigDirectory   Directory holding the .big files.
     * @param tocResourceId  Logical ID of the TOC resource, from packTOC.dat.
     */
    bool Bind(const std::string &bigDirectory, std::uint32_t tocResourceId);

    bool IsBound() const { return m_bound; }

    const std::string &GetFullName() const { return m_fullName; }
    const std::string &GetShortName() const { return m_shortName; }

    /** Hash of the short name -- this pack's identity in a CGameAssetRef. */
    std::uint32_t GetPackHash() const { return m_packHash; }

    /**
     * Look up a resource handle by name. Returns 0 when the name is absent,
     * which is also what a null reference looks like, so callers that care
     * must check the handle rather than the lookup.
     */
    std::uint32_t GetResValue(const char *resourceName) const;

    /** Fetch a resource by handle, following aggregates. */
    bool GetResource(std::uint32_t handle, std::vector<std::uint8_t> &out);

    const std::vector<PackTOCEntry> &GetEntries() const { return m_entries; }

    CBigFileReader &GetReader() { return m_reader; }

private:
    /** Read ___INIT_DATA and push the locale/aggregate tables into the reader. */
    bool LoadInitData();

    std::string m_fullName;
    std::string m_shortName;
    std::uint32_t m_packHash;

    CBigFileReader m_reader;
    std::vector<PackTOCEntry> m_entries;
    bool m_bound;
};

#endif  // GUN_BROS_RE_GUN_BROS_CRESPACKTOC_H
