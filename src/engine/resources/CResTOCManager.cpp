#include <filesystem>
/**
 * @file CResTOCManager.cpp
 * @brief Registry of every resource pack.
 */

#include "engine/resources/CResTOCManager.h"

#include "engine/core/CStringToKey.h"

#include <cstdio>
#include <fstream>

const char *const kArtSetXga = "xga";
const char *const kCorePackName = "pack0_core";
const char *const kPackTOCKeyTableOfContents = "TABLEOFCONTENTS";

namespace {

/** Read a BIG-endian uint16. packTOC.dat is the one big-endian file we read. */
std::uint16_t ReadBigU16(const std::uint8_t *bytes) {
    return static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);
}

/** Read a BIG-endian uint32. */
std::uint32_t ReadBigU32(const std::uint8_t *bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24) |
           (static_cast<std::uint32_t>(bytes[1]) << 16) |
           (static_cast<std::uint32_t>(bytes[2]) << 8) |
           static_cast<std::uint32_t>(bytes[3]);
}

/** Load a whole file. Returns false when it cannot be opened. */
bool ReadWholeFile(const std::string &path, std::vector<std::uint8_t> &out) {
    // Preserve Unicode installation directories on Windows.
    std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);

    out.resize(static_cast<std::size_t>(size));
    if (size == 0) {
        return true;
    }
    file.read(reinterpret_cast<char *>(out.data()), size);
    return file.gcount() == size;
}

/** Strip a trailing "_<artSet>" from a pack name. */
std::string StripArtSetSuffix(const std::string &fullName, const std::string &artSet) {
    const std::string suffix = "_" + artSet;
    if (fullName.size() <= suffix.size()) {
        return fullName;
    }
    if (fullName.compare(fullName.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return fullName;
    }
    return fullName.substr(0, fullName.size() - suffix.size());
}

}  // namespace

CResTOCManager::CResTOCManager() : m_corePackIndex(0) {}

CResTOCManager::~CResTOCManager() = default;

bool CResTOCManager::ReadPackTOCFile(const std::string &path,
                                     const std::string &artSet,
                                     std::vector<PackTOCRecord> &records) const {
    std::vector<std::uint8_t> data;
    if (!ReadWholeFile(path, data)) {
        std::printf("[toc] cannot open %s\n", path.c_str());
        return false;
    }

    // Records run back to back: uint16 length, that many ASCII bytes, uint32
    // value. All big-endian, no terminator, no count up front.
    std::size_t position = 0;
    while (position + 2 <= data.size()) {
        const std::uint16_t textLength = ReadBigU16(&data[position]);
        position += 2;

        if (position + textLength + 4 > data.size()) {
            std::printf("[toc] record at %zu runs past the file\n", position);
            return false;
        }

        const std::string text(reinterpret_cast<const char *>(&data[position]), textLength);
        position += textLength;

        const std::uint32_t value = ReadBigU32(&data[position]);
        position += 4;

        // "pack1_xga:TABLEOFCONTENTS"
        const std::size_t colon = text.find(':');
        if (colon == std::string::npos) {
            std::printf("[toc] record %s has no ':'\n", text.c_str());
            return false;
        }

        const std::string key = text.substr(colon + 1);
        if (key != kPackTOCKeyTableOfContents) {
            // The format allows other keys; none appear in this build.
            std::printf("[toc] ignoring key %s\n", key.c_str());
            continue;
        }

        PackTOCRecord record;
        record.fullName = text.substr(0, colon);
        record.shortName = StripArtSetSuffix(record.fullName, artSet);
        record.tocResourceId = value;
        records.push_back(record);
    }

    return !records.empty();
}

bool CResTOCManager::Init(const std::string &bigDirectory, const std::string &artSet) {
    m_bigDirectory = bigDirectory;
    m_packs.clear();
    m_tocResourceIds.clear();
    m_corePackIndex = 0;

    // The original assembles this name piecewise in CResTOCManager::Init.
    const std::string tocPath = bigDirectory + "/packTOC_" + artSet + ".dat";

    std::vector<PackTOCRecord> records;
    if (!ReadPackTOCFile(tocPath, artSet, records)) {
        return false;
    }

    for (std::size_t i = 0; i < records.size(); ++i) {
        const PackTOCRecord &record = records[i];

        m_packs.push_back(std::unique_ptr<CResPackTOC>(
            new CResPackTOC(record.fullName, record.shortName)));
        m_tocResourceIds.push_back(record.tocResourceId);

        if (record.shortName == kCorePackName) {
            m_corePackIndex = static_cast<int>(i);
        }
    }

    std::printf("[toc] %s: %zu packs, core is %s\n",
                tocPath.c_str(), m_packs.size(),
                m_packs[m_corePackIndex]->GetShortName().c_str());
    return true;
}

bool CResTOCManager::Bind() {
    bool allBound = true;
    for (std::size_t i = 0; i < m_packs.size(); ++i) {
        if (!m_packs[i]->Bind(m_bigDirectory, m_tocResourceIds[i])) {
            std::printf("[toc] bind failed: %s\n", m_packs[i]->GetFullName().c_str());
            allBound = false;
        }
    }
    return allBound;
}

int CResTOCManager::GetPackIndexFromHash(std::uint32_t packHash) const {
    for (std::size_t i = 0; i < m_packs.size(); ++i) {
        if (m_packs[i]->GetPackHash() == packHash) {
            return static_cast<int>(i);
        }
    }
    // Unknown hashes fall through to pack index 0, as in the original.
    return 0;
}

int CResTOCManager::GetPackIndexFromName(const char *shortName) const {
    return GetPackIndexFromHash(CStringToKey(shortName));
}

CResPackTOC *CResTOCManager::GetPack(int packIndex) {
    if (packIndex < 0 || static_cast<std::size_t>(packIndex) >= m_packs.size()) {
        return nullptr;
    }
    return m_packs[packIndex].get();
}

bool CResTOCManager::GetAsset(std::uint32_t packHash, std::uint32_t handle,
                              std::vector<std::uint8_t> &out) {
    CResPackTOC *pack = GetPack(GetPackIndexFromHash(packHash));
    if (pack == nullptr) {
        return false;
    }
    return pack->GetResource(handle, out);
}
