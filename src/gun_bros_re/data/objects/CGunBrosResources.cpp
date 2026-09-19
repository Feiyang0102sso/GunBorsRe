/** Game-pack lifetime and cross-pack addressing, original gunbros.cpp. */
#include "gun_bros_re/application/CGunBros.h"

CGunBros::CGunBros(CResTOCManager &toc) : m_tocManager(toc) {
    m_objectPacks.resize(toc.GetPackCount());
    for (unsigned index = 0; index < toc.GetPackCount(); ++index) {
        m_objectPacks[index].Init(*toc.GetPack(index));
    }
}

bool CGunBros::HasLatestBigVersion() const {
    if (m_objectPacks.empty()) { return false; }
    for (const CGameObjectPack &pack : m_objectPacks) {
        if (pack.GetBigVersion() != CGameObjectPack::BigVersion::V1) { return false; }
    }
    return true;
}

const std::string &CGunBros::GetPackName(std::uint32_t hash) {
    return m_tocManager.GetPack(m_tocManager.GetPackIndexFromHash(hash))->GetShortName();
}

bool CGunBros::ReadSectionResource(std::uint32_t hash, ZGameSection section,
    std::uint32_t ordinal, std::vector<std::uint8_t> &payload) {
    const int index = m_tocManager.GetPackIndexFromHash(hash);
    if (index < 0) { return false; }
    const unsigned handle = m_objectPacks[index].GetHandle(section, ordinal);
    if (handle == 0) { return false; }
    if (m_progress != nullptr) { m_progress->OnResourceRead(); }
    return m_tocManager.GetPack(index)->GetResource(handle, payload);
}

#include "gun_bros_re/data/objects/CGameObjectPackObjects.h"
/** GetResId :78597 uses the string section of the original game keyset.
 * Resolve the string ordinal in the game keyset (following its section bases).
 * Localized strings use UTF-8 bytes in this archive's English locale.
 */
std::string CGunBros::ReadString(const CGameAssetRef &ref) {
    if (ref.assetId < 0 || ref.IsNull()) { return {}; }
    const int packIndex = m_tocManager.GetPackIndexFromHash(ref.packHash);
    if (packIndex < 0) { return {}; }
    CGameObjectPack &pack = m_objectPacks[packIndex];
    auto &strings = pack.GetObjects().strings;
    const auto found = strings.find(ref.assetId);
    if (found != strings.end()) { return found->second; }
    const unsigned handle = pack.GetStringHandle(ref.assetId);
    if (handle == 0) { return {}; }
    std::vector<std::uint8_t> bytes;
    if (!m_tocManager.GetPack(packIndex)->GetResource(handle, bytes)) { return {}; }
    std::string text;
    for (std::uint8_t byte : bytes) {
        if (byte == 0) { break; }
        text.push_back(static_cast<char>(byte));
    }
    strings.emplace(ref.assetId, text);
    return text;
}
