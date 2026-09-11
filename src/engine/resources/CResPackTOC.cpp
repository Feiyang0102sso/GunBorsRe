/**
 * @file CResPackTOC.cpp
 * @brief One resource pack: its .big archive plus the name -> handle table.
 */

#include "engine/resources/CResPackTOC.h"

#include "engine/core/CStringToKey.h"

#include <cstdio>
#include <cstring>

const char *const kInitDataResourceName = "___INIT_DATA";

namespace {

std::uint32_t ReadU32(const std::uint8_t *bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

/**
 * Sequential reader over a decompressed resource payload.
 * Reads past the end yield zero and latch an error flag.
 */
class PayloadReader {
public:
    explicit PayloadReader(const std::vector<std::uint8_t> &data)
        : m_data(data), m_position(0), m_overran(false) {}

    std::uint32_t ReadU32() {
        if (m_position + 4 > m_data.size()) {
            m_overran = true;
            return 0;
        }
        const std::uint32_t value = ::ReadU32(&m_data[m_position]);
        m_position += 4;
        return value;
    }

    /** Skip forward; used for the parts of ___INIT_DATA we do not consume. */
    void Skip(std::size_t count) {
        if (m_position + count > m_data.size()) {
            m_overran = true;
            m_position = m_data.size();
            return;
        }
        m_position += count;
    }

    bool Overran() const { return m_overran; }
    std::size_t Remaining() const { return m_data.size() - m_position; }

private:
    const std::vector<std::uint8_t> &m_data;
    std::size_t m_position;
    bool m_overran;
};

}  // namespace

CResPackTOC::CResPackTOC(const std::string &fullName, const std::string &shortName)
    : m_fullName(fullName),
      m_shortName(shortName),
      m_packHash(CStringToKey(shortName.c_str())),
      m_bound(false) {}

bool CResPackTOC::Bind(const std::string &bigDirectory, std::uint32_t tocResourceId) {
    m_bound = false;
    m_entries.clear();

    const std::string archivePath = bigDirectory + "/" + m_fullName + ".big";
    if (!m_reader.Open(archivePath)) {
        return false;
    }

    // --- the TOC resource itself: uint32 count, then count pairs ---
    std::vector<std::uint8_t> payload;
    if (!m_reader.GetResourceById(tocResourceId, payload)) {
        std::printf("[pack] %s: TOC resource %u not found\n",
                    m_fullName.c_str(), tocResourceId);
        return false;
    }

    PayloadReader reader(payload);
    const std::uint32_t entryCount = reader.ReadU32();
    if (entryCount * 8 > reader.Remaining()) {
        std::printf("[pack] %s: TOC claims %u entries but holds %zu bytes\n",
                    m_fullName.c_str(), entryCount, reader.Remaining());
        return false;
    }

    m_entries.resize(entryCount);
    for (std::uint32_t i = 0; i < entryCount; ++i) {
        m_entries[i].nameKey = reader.ReadU32();
        m_entries[i].handle = reader.ReadU32();
    }
    if (reader.Overran()) {
        std::printf("[pack] %s: TOC truncated\n", m_fullName.c_str());
        m_entries.clear();
        return false;
    }

    if (!LoadInitData()) {
        return false;
    }

    m_bound = true;
    std::printf("[pack] %s bound: %u entries, hash 0x%08X\n",
                m_fullName.c_str(), entryCount, m_packHash);
    return true;
}

bool CResPackTOC::LoadInitData() {
    // In the original this lives in CResourceManager_v1::Init, which we do not
    // port -- the resource manager is infrastructure we have no callers for
    // yet. Only the two tables it injects into the reader actually matter.
    // Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:330862
    const std::uint32_t handle = GetResValue(kInitDataResourceName);
    if (handle == 0) {
        std::printf("[pack] %s: no %s\n", m_fullName.c_str(), kInitDataResourceName);
        return false;
    }

    std::vector<std::uint8_t> payload;
    if (!m_reader.GetResourceByHandle(handle, payload)) {
        std::printf("[pack] %s: %s unreadable\n",
                    m_fullName.c_str(), kInitDataResourceName);
        return false;
    }

    PayloadReader reader(payload);

    // A leading uint32 array the engine keeps but never reads back.
    const std::uint32_t leadingCount = reader.ReadU32();
    reader.Skip(static_cast<std::size_t>(leadingCount) * 4);

    // --- locale table ---
    const std::uint32_t localeCount = reader.ReadU32();
    const std::uint32_t localeCodeLength = reader.ReadU32();
    const std::uint32_t localeNameLength = reader.ReadU32();

    std::vector<std::uint32_t> localeIds;
    if (localeCount > 0 && localeCodeLength > 0) {
        localeIds.resize(localeCount);
        for (std::uint32_t i = 0; i < localeCount; ++i) {
            localeIds[i] = reader.ReadU32();
        }
        // Then the language codes ("en") and display names ("ENGLISH"). The
        // game ships English only, so nothing needs them yet.
        reader.Skip(static_cast<std::size_t>(localeCodeLength) * localeCount);
        reader.Skip(static_cast<std::size_t>(localeNameLength) * localeCount);
    }

    // --- aggregate table ---
    const std::uint32_t aggregateCount = reader.ReadU32();
    std::vector<std::uint32_t> aggregateIds(aggregateCount);
    for (std::uint32_t i = 0; i < aggregateCount; ++i) {
        aggregateIds[i] = reader.ReadU32();
    }

    if (reader.Overran()) {
        std::printf("[pack] %s: %s truncated\n",
                    m_fullName.c_str(), kInitDataResourceName);
        return false;
    }

    m_reader.SetLocaleIdTable(localeIds);
    m_reader.SetAggregateIdTable(aggregateIds);
    m_reader.SetLocaleIndex(0);
    return true;
}

std::uint32_t CResPackTOC::GetResValue(const char *resourceName) const {
    const std::uint32_t nameKey = CStringToKey(resourceName);

    // Linear scan, as in the original. The tables are sorted by key, so this
    // could be a binary search -- left alone until a profile says it matters.
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].nameKey == nameKey) {
            return m_entries[i].handle;
        }
    }
    return 0;
}

bool CResPackTOC::GetResource(std::uint32_t handle, std::vector<std::uint8_t> &out) {
    return m_reader.GetResourceByHandle(handle, out);
}
