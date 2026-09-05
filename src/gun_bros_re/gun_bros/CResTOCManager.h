/**
 * @file CResTOCManager.h
 * @brief Registry of every resource pack, and the entry point for cross-pack
 *        addressing.
 *
 * Port of CResTOCManager (src/gunbros/resTOCManager.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:132842 (Init),
 *            :132617 (GetPackIndexFromHash)
 *
 * packTOC_<artset>.dat lists every pack and the logical ID of its TOC
 * resource. It is BIG-endian, unlike everything inside a .big.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CRESTOCMANAGER_H
#define GUN_BROS_RE_GUN_BROS_CRESTOCMANAGER_H

#include "gun_bros/CResPackTOC.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// The only art set we support. The original picks it at runtime from the
// device; see Hardware::GetSupportedArtSetString.
extern const char *const kArtSetXga;

// The pack the game falls back to. It is also the pack an unknown hash lands
// on, because the original returns index 0 on a failed lookup.
extern const char *const kCorePackName;

// Key that every packTOC record uses for its value; the format allows others.
extern const char *const kPackTOCKeyTableOfContents;

/**
 * One record of packTOC_<artset>.dat.
 * The file is a flat list of "<pack>_<artset>:<KEY>" strings with a value each.
 */
struct PackTOCRecord {
    std::string fullName;         // "pack1_xga"
    std::string shortName;        // "pack1"
    std::uint32_t tocResourceId;  // logical ID of the pack's TOC resource
};

/**
 * Owns every CResPackTOC and resolves pack hashes to them.
 */
class CResTOCManager {
public:
    CResTOCManager();
    ~CResTOCManager();

    CResTOCManager(const CResTOCManager &) = delete;
    CResTOCManager &operator=(const CResTOCManager &) = delete;

    /** Parse packTOC_<artSet>.dat and create a CResPackTOC per record. */
    bool Init(const std::string &bigDirectory, const std::string &artSet);

    /** Open and bind every pack. Returns false if any one of them fails. */
    bool Bind();

    std::uint32_t GetPackCount() const {
        return static_cast<std::uint32_t>(m_packs.size());
    }

    /**
     * Pack index for a pack hash.
     *
     * Returns 0 -- the core pack -- for an unknown hash, exactly as the
     * original does. That is why CGameAssetRef values like 0x00267580
     * ("pack0", which is registered as "pack0_core") still land somewhere
     * sensible.
     */
    int GetPackIndexFromHash(std::uint32_t packHash) const;

    /** Pack index whose short name is `name`, or -1. */
    int GetPackIndexFromName(const char *shortName) const;

    CResPackTOC *GetPack(int packIndex);

    /** Index of pack0_core, which the game treats as the global pack. */
    int GetCorePackIndex() const { return m_corePackIndex; }

    /**
     * Fetch what a CGameAssetRef points at: pick the pack by hash, then follow
     * the handle inside it.
     */
    bool GetAsset(std::uint32_t packHash, std::uint32_t handle,
                  std::vector<std::uint8_t> &out);

private:
    /** Read and split the packTOC file. Returns false when it is unusable. */
    bool ReadPackTOCFile(const std::string &path, const std::string &artSet,
                         std::vector<PackTOCRecord> &records) const;

    std::string m_bigDirectory;
    std::vector<std::unique_ptr<CResPackTOC>> m_packs;
    std::vector<std::uint32_t> m_tocResourceIds;
    int m_corePackIndex;
};

#endif  // GUN_BROS_RE_GUN_BROS_CRESTOCMANAGER_H
