/**
 * @file CGameObjectPack.cpp
 * @brief Section base table -- turns a (section, ordinal) pair into a handle.
 */

#include "gun_bros_re/data/CGameObjectPack.h"

#include "engine/resources/CArrayInputStream.h"

#include <cstdio>

const char *const kGameTocKeysetName = "___GAME_TOC_KEYSET";
const char *const kObjectScriptCountsName = "OBJECT_SCRIPT__COUNTS_";

CGameObjectPack::CGameObjectPack() = default;

bool CGameObjectPack::Init(CResPackTOC &pack) {
    m_sectionBases.clear();
    m_objectCounts.clear();

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

    // The section bases are the leading handles that are not aggregate
    // references; the aggregate ones after them are the pack's strings.
    for (std::uint16_t i = 0; i < handleCount; ++i) {
        const std::uint32_t handle = stream.ReadUInt32();
        if ((handle & kHandleAggregateFlag) != 0) {
            break;
        }
        m_sectionBases.push_back(handle);
    }

    if (stream.Overran()) {
        std::printf("[objpack] %s: keyset truncated\n", pack.GetShortName().c_str());
        m_sectionBases.clear();
        return false;
    }
    if (m_sectionBases.size() != kSectionCount) {
        std::printf("[objpack] %s: %zu section bases, expected %u\n",
                    pack.GetShortName().c_str(), m_sectionBases.size(), kSectionCount);
        m_sectionBases.clear();
        return false;
    }

    // --- object counts ---
    // The last section's base points at the counts resource itself.
    const std::uint32_t countsHandle = m_sectionBases[kSectionCount - 1];
    if (!pack.GetResource(countsHandle, payload)) {
        std::printf("[objpack] %s: counts resource unreadable\n",
                    pack.GetShortName().c_str());
        return false;
    }

    // Format: uint8 typeCount, then one uint8 per type.
    CArrayInputStream countsStream(payload);
    const std::uint8_t typeCount = countsStream.ReadUInt8();
    for (std::uint8_t i = 0; i < typeCount; ++i) {
        m_objectCounts.push_back(countsStream.ReadUInt8());
    }
    if (countsStream.Overran()) {
        std::printf("[objpack] %s: counts truncated\n", pack.GetShortName().c_str());
        m_objectCounts.clear();
        return false;
    }

    return true;
}

std::uint32_t CGameObjectPack::GetSectionBase(GameSection section) const {
    const std::uint32_t index = static_cast<std::uint32_t>(section) - 1;
    if (index >= m_sectionBases.size()) {
        return 0;
    }
    return m_sectionBases[index];
}

std::uint32_t CGameObjectPack::GetSectionSpan(GameSection section) const {
    const std::uint32_t index = static_cast<std::uint32_t>(section) - 1;
    if (index + 1 >= m_sectionBases.size()) {
        return 0;
    }

    const std::uint32_t base = m_sectionBases[index];
    const std::uint32_t nextBase = m_sectionBases[index + 1];
    if (nextBase <= base) {
        return 0;
    }
    return nextBase - base;
}

std::uint32_t CGameObjectPack::GetHandle(GameSection section,
                                         std::uint32_t localIndex) const {
    const std::uint32_t base = GetSectionBase(section);
    if (base == 0) {
        return 0;
    }
    return base + localIndex;
}

std::uint32_t CGameObjectPack::GetObjectCount(GameSection section) const {
    // Only the first 28 sections are object types; PNG and beyond are not
    // covered by the counts resource.
    const std::uint32_t index = static_cast<std::uint32_t>(section) - 1;
    if (index >= m_objectCounts.size()) {
        return 0;
    }
    return m_objectCounts[index];
}
