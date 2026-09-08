/**
 * @file PackTables.h
 * @brief Section bases for every pack at once, so any reference resolves.
 *
 * Harness scaffolding, not a port: the engine keeps this state inside
 * CGunBros, which is a class the project has no reason to write yet. Lifted
 * out of M35Mesh.cpp when a second harness needed it.
 *
 * A reference carries its own pack hash and it is not always the pack the
 * template came from, which is the rule M3.1 had to learn the hard way: pick
 * the pack first, then add the section base.
 */

#ifndef GUN_BROS_RE_MILESTONES_PACKTABLES_H
#define GUN_BROS_RE_MILESTONES_PACKTABLES_H

#include "gun_bros/CGameObjectPack.h"
#include "gun_bros/CResTOCManager.h"

#include <cstdint>
#include <string>
#include <vector>

/** Loading screens receive resource boundaries on the render thread. */
class IPackLoadProgress {
public:
    virtual ~IPackLoadProgress() = default;
    virtual void OnResourceRead() = 0;
};

class PackTables {
public:
    explicit PackTables(CResTOCManager &tocManager) : m_tocManager(tocManager) {
        m_objectPacks.resize(tocManager.GetPackCount());
        for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
            m_objectPacks[i].Init(*tocManager.GetPack(static_cast<int>(i)));
        }
    }

    CGameObjectPack &GetObjectPack(int packIndex) { return m_objectPacks[packIndex]; }
    void SetLoadProgress(IPackLoadProgress *progress) { m_loadProgress = progress; }

    /** Which pack a hash lands on. The reference decides, not its owner. */
    const std::string &GetPackName(std::uint32_t packHash) {
        const int packIndex = m_tocManager.GetPackIndexFromHash(packHash);
        return m_tocManager.GetPack(packIndex)->GetShortName();
    }

    bool ReadSectionResource(std::uint32_t packHash, GameSection section,
                             std::uint32_t ordinal,
                             std::vector<std::uint8_t> &payload) {
        if (m_loadProgress != nullptr) { m_loadProgress->OnResourceRead(); }
        const int packIndex = m_tocManager.GetPackIndexFromHash(packHash);
        if (packIndex < 0) { return false; }
        const std::uint32_t handle =
            m_objectPacks[packIndex].GetHandle(section, ordinal);
        if (handle == 0) {
            return false;
        }
        return m_tocManager.GetPack(packIndex)->GetResource(handle, payload);
    }

private:
    CResTOCManager &m_tocManager;
    std::vector<CGameObjectPack> m_objectPacks;
    IPackLoadProgress *m_loadProgress = nullptr;
};

#endif  // GUN_BROS_RE_MILESTONES_PACKTABLES_H
