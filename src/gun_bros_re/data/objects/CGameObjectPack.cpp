/**
 * @file CGameObjectPack.cpp
 * @brief Section base table -- turns a (section, ordinal) pair into a handle.
 */

#include "gun_bros_re/data/objects/CGameObjectPackObjects.h"

#include "engine/resources/CArrayInputStream.h"

#include <cstdio>

const char *const kGameTocKeysetName = "___GAME_TOC_KEYSET";
const char *const kObjectScriptCountsName = "OBJECT_SCRIPT__COUNTS_";

// Only format signatures belong here. Resource values stay in the BIG files.
inline constexpr CGameObjectPack::VersionLayout kBigVersionLayouts[] = {
    {CGameObjectPack::BigVersion::V1, 28, 33},
    {CGameObjectPack::BigVersion::V2, 27, 32},
    {CGameObjectPack::BigVersion::V3, 26, 31},
};

inline const CGameObjectPack::VersionLayout *FindBigVersionLayout(std::uint32_t sections,
                                                   std::uint32_t objectTypes) {
    for (const CGameObjectPack::VersionLayout &layout : kBigVersionLayouts) {
        if (layout.sectionCount == sections && layout.objectTypeCount == objectTypes) {
            return &layout;
        }
    }
    return nullptr;
}

CGameObjectPack::CGameObjectPack() = default;
CGameObjectPack::~CGameObjectPack() = default;
CGameObjectPack::CGameObjectPack(CGameObjectPack &&) noexcept = default;
CGameObjectPack &CGameObjectPack::operator=(CGameObjectPack &&) noexcept = default;
CGameObjectPack::Objects &CGameObjectPack::GetObjects() {
    if (!m_objects) { m_objects = std::make_unique<Objects>(); }
    return *m_objects;
}


bool CGameObjectPack::Init(CResPackTOC &pack) {
    m_objects.reset();
    m_version = CGameObjectPack::BigVersion::Unknown;
    m_sectionBases.clear();
    m_objectCounts.clear();
    m_stringHandles.clear();

    // --- section bases, from the keyset ---
    const std::uint32_t keysetHandle = pack.GetResValue(kGameTocKeysetName);
    if (keysetHandle == 0) {
        std::printf("[objpack] %s: no %s\n",
                    pack.GetShortName().c_str(), kGameTocKeysetName);
        return false;
    }

    std::vector<std::uint8_t> payload;
    if (!pack.GetResource(keysetHandle, payload)) {
        std::printf("[objpack] %s: keyset unreadable\n", pack.GetShortName().c_str());
        return false;
    }

    // Keyset format: uint16 count, then that many uint32 handles.
    CArrayInputStream stream(payload);
    const std::uint16_t handleCount = stream.ReadUInt16();
    if (stream.Overran() || stream.Available() != static_cast<std::size_t>(handleCount) * 4) {
        std::printf("[objpack] %s: keyset size mismatch\n", pack.GetShortName().c_str());
        return false;
    }

    // The section bases are the leading handles that are not aggregate
    // references; the aggregate ones after them are the pack's strings.
    for (std::uint16_t i = 0; i < handleCount; ++i) {
        const std::uint32_t handle = stream.ReadUInt32();
        if ((handle & kHandleAggregateFlag) != 0) {
            m_stringHandles.push_back(handle);
        } else {
            if (!m_stringHandles.empty()) {
                std::printf("[objpack] %s: non-string handle after string prefix\n", pack.GetShortName().c_str());
                return false;
            }
            m_sectionBases.push_back(handle);
        }
    }

    if (stream.Overran()) {
        std::printf("[objpack] %s: keyset truncated\n", pack.GetShortName().c_str());
        m_sectionBases.clear();
        return false;
    }
    if (m_sectionBases.empty()) {
        std::printf("[objpack] %s: section prefix missing\n", pack.GetShortName().c_str());
        return false;
    }

    // --- object counts ---
    // The last section's base points at the counts resource itself.
    const std::uint32_t countsHandle = m_sectionBases.back();
    if (countsHandle != pack.GetResValue(kObjectScriptCountsName)) {
        std::printf("[objpack] %s: counts handle does not match the named resource\n", pack.GetShortName().c_str());
        return false;
    }
    if (!pack.GetResource(countsHandle, payload)) {
        std::printf("[objpack] %s: counts resource unreadable\n",
                    pack.GetShortName().c_str());
        return false;
    }

    // Format: uint8 typeCount, then one uint8 per type.
    CArrayInputStream countsStream(payload);
    const std::uint8_t typeCount = countsStream.ReadUInt8();
    const CGameObjectPack::VersionLayout *layout = FindBigVersionLayout(GetSectionCount(), typeCount);
    if (layout == nullptr || countsStream.Overran() || countsStream.Available() != typeCount) {
        std::printf("[objpack] %s: unsupported or invalid layout sections=%zu types=%u bytes=%zu\n",
                    pack.GetShortName().c_str(), m_sectionBases.size(), typeCount, payload.size());
        return false;
    }
    for (std::uint8_t i = 0; i < typeCount; ++i) {
        m_objectCounts.push_back(countsStream.ReadUInt8());
    }
    if (countsStream.Overran()) {
        std::printf("[objpack] %s: counts truncated\n", pack.GetShortName().c_str());
        m_objectCounts.clear();
        return false;
    }

    m_version = layout->version;
    return true;
}

std::size_t CGameObjectPack::GetSectionIndex(ZGameSection section) const {
    if (!IsInitialised()) { return m_sectionBases.size(); }
    const std::uint32_t number = static_cast<std::uint32_t>(section);
    if (number == 0) { return m_sectionBases.size(); }
    if (number <= kObjectTypeCount) {
        // New object types must never alias an older pack's PNG/WAV sections.
        if (number > m_objectCounts.size()) { return m_sectionBases.size(); }
        return number - 1;
    }
    return m_objectCounts.size() + (number - kObjectTypeCount - 1);
}

std::uint32_t CGameObjectPack::GetSectionBase(ZGameSection section) const {
    const std::size_t index = GetSectionIndex(section);
    if (index >= m_sectionBases.size()) {
        return 0;
    }
    return m_sectionBases[index];
}

std::uint32_t CGameObjectPack::GetSectionSpan(ZGameSection section) const {
    const std::size_t index = GetSectionIndex(section);
    if (index + 1 >= m_sectionBases.size()) {
        return 0;
    }

    // Section tags differ for PNG/WAV/BIN; only logical IDs define the span.
    const std::uint32_t base = m_sectionBases[index] & kHandleIdMask;
    const std::uint32_t nextBase = m_sectionBases[index + 1] & kHandleIdMask;
    if (nextBase <= base) {
        return 0;
    }
    return nextBase - base;
}

std::uint32_t CGameObjectPack::GetHandle(ZGameSection section,
                                         std::uint32_t localIndex) const {
    const std::uint32_t base = GetSectionBase(section);
    if (base == 0) {
        return 0;
    }
    return base + localIndex;
}

std::uint32_t CGameObjectPack::GetObjectCount(ZGameSection section) const {
    // Only the first 28 sections are object types; PNG and beyond are not
    // covered by the counts resource.
    const std::size_t index = GetSectionIndex(section);
    if (index >= m_objectCounts.size()) {
        return 0;
    }
    return m_objectCounts[index];
}

std::uint32_t CGameObjectPack::GetStringHandle(std::uint32_t ordinal) const {
    if (!IsInitialised() || ordinal >= m_stringHandles.size()) { return 0; }
    return m_stringHandles[ordinal];
}
