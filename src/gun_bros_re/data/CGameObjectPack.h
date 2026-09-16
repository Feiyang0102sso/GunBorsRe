/**
 * @file CGameObjectPack.h
 * @brief Section base table -- turns a (section, ordinal) pair into a handle.
 *
 * Port of CGameObjectPack (src/gunbros/gameObjectPack.cpp).
 *
 * Object templates never store resource handles. They store an ordinal within
 * a section, and the engine adds the section's base:
 *
 *     handle = sectionBase[type] + localIndex
 *
 * The 33 bases are the leading non-aggregate entries of the pack's
 * ___GAME_TOC_KEYSET resource; everything after them is the pack's string
 * handles, which belong to a different consumer.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CGAMEOBJECTPACK_H
#define GUN_BROS_RE_GUN_BROS_CGAMEOBJECTPACK_H

#include "engine/resources/CResPackTOC.h"
#include "gun_bros_re/data/ZBigVersions.h"

#include <cstdint>
#include <string>
#include <vector>

// Name of the keyset holding the section bases.
extern const char *const kGameTocKeysetName;

// Name of the resource holding one object count per type.
extern const char *const kObjectScriptCountsName;

/**
 * Section numbers, as named in the reverse-engineering notes. The type id an
 * object template uses is the section number minus one.
 *
 * Only the ones M3 needs are named; the full list is in
 * _Big_tool/sections.md section 6.3.
 */
enum class ZGameSection {
    Armor = 3,
    Bullet = 4,
    Enemy = 6,
    Gun = 7,
    Level = 8,
    Mission = 10,
    MissionObjective = 11,
    ParticleEffect = 12,
    Pickup = 13,
    Planet = 14,
    Player = 16,
    PlayerProgress = 17,
    Powerup = 18,
    Prop = 20,
    RefinementManager = 21,
    SoundEffect = 22,
    StoreItem = 23,
    TileLayer = 24,
    TileSet = 25,
    MPMatch = 28,
    Png = 29,
    Wav = 30,
    Mesh = 31,
};

// How many section bases the 3.6.0 build stores.
constexpr std::uint32_t kSectionCount = kBigVersionLayouts[0].sectionCount;

// Object types covered by the counts resource -- the first 28 sections.
constexpr std::uint32_t kObjectTypeCount = kBigVersionLayouts[0].objectTypeCount;

/**
 * One pack's object addressing: section bases plus the per-type object counts.
 */
class CGameObjectPack {
public:
    CGameObjectPack();

    /** Read the keyset and the counts resource out of an already-bound pack. */
    bool Init(CResPackTOC &pack);

    bool IsInitialised() const { return m_version != ZBigVersion::Unknown; }
    ZBigVersion GetBigVersion() const { return m_version; }
    std::uint32_t GetSectionCount() const { return static_cast<std::uint32_t>(m_sectionBases.size()); }
    std::uint32_t GetTypeCount() const { return static_cast<std::uint32_t>(m_objectCounts.size()); }
    std::uint32_t GetStringCount() const { return static_cast<std::uint32_t>(m_stringHandles.size()); }

    /** Strings follow the actual section prefix, not a fixed release offset. */
    std::uint32_t GetStringHandle(std::uint32_t ordinal) const;

    /**
     * Handle for one object of a section.
     *
     * @param section    Which section, e.g. GameSection::TileSet.
     * @param localIndex Ordinal within it, as stored in a template.
     * @return the handle, or 0 when the section or ordinal is out of range.
     */
    std::uint32_t GetHandle(ZGameSection section, std::uint32_t localIndex) const;

    /**
     * How many objects a section holds.
     *
     * Comes from the counts resource, which is authoritative: a section whose
     * bases are one apart may still hold zero objects, with a single empty
     * resource standing in as a placeholder.
     */
    std::uint32_t GetObjectCount(ZGameSection section) const;

    /** Base handle of a section, for diagnostics. */
    std::uint32_t GetSectionBase(ZGameSection section) const;

    /**
     * How many handles a section spans: the distance to the next base.
     *
     * The counts resource only covers the first 28 sections, so this is the
     * only way to size PNG, WAV and MESH. It counts placeholders too -- a
     * section holding nothing still owns one empty resource -- so a caller
     * has to be ready for an empty payload.
     */
    std::uint32_t GetSectionSpan(ZGameSection section) const;

private:
    /** GameSection uses the latest names; trailing asset sections shift in older BIGs. */
    std::size_t GetSectionIndex(ZGameSection section) const;
    ZBigVersion m_version = ZBigVersion::Unknown;
    std::vector<std::uint32_t> m_sectionBases;
    std::vector<std::uint8_t> m_objectCounts;
    std::vector<std::uint32_t> m_stringHandles;
};

#endif  // GUN_BROS_RE_GUN_BROS_CGAMEOBJECTPACK_H
