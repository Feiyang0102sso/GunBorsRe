#pragma once
#include "engine/resources/CResTOCManager.h"

/** Loading screens receive resource boundaries on the render thread. */
class IPackLoadProgress {
public:
    virtual ~IPackLoadProgress() = default;
    virtual void OnResourceRead() = 0;
};

/** Cross-package handle reads and load notifications; the upper-layer PackTables resolves game entry categories. */
class ResourcePacks {
public:
    explicit ResourcePacks(CResTOCManager &toc) : m_toc(toc) {}
    void SetLoadProgress(IPackLoadProgress *progress) { m_progress = progress; }
    const std::string &GetPackName(std::uint32_t hash) {
        return m_toc.GetPack(m_toc.GetPackIndexFromHash(hash))->GetShortName();
    }
    bool Read(int packIndex, std::uint32_t handle, std::vector<std::uint8_t> &payload) {
        if (m_progress != nullptr) { m_progress->OnResourceRead(); }
        if (packIndex < 0 || handle == 0) { return false; }
        return m_toc.GetPack(packIndex)->GetResource(handle, payload);
    }
private:
    CResTOCManager &m_toc;
    IPackLoadProgress *m_progress = nullptr;
};

