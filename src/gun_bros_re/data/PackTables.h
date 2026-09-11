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

#include "gun_bros_re/data/CGameObjectPack.h"
#include "engine/resources/CResTOCManager.h"

#include <cstdint>
#include <string>
#include <vector>

#include "engine/resources/ResourcePacks.h"

class PackTables {
public:
    explicit PackTables(CResTOCManager &tocManager) : m_tocManager(tocManager), m_resources(tocManager) {
        m_objectPacks.resize(tocManager.GetPackCount());
        for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
            m_objectPacks[i].Init(*tocManager.GetPack(static_cast<int>(i)));
        }
    }

    CGameObjectPack &GetObjectPack(int packIndex) { return m_objectPacks[packIndex]; }
    /** Game entry points still use the latest rule set; older BIGs are viewer inputs. */
    bool HasLatestBigVersion() const {
        if (m_objectPacks.empty()) { return false; }
        for (const CGameObjectPack &pack : m_objectPacks) {
            if (pack.GetBigVersion() != BigVersion::V1) { return false; }
        }
        return true;
    }
    void SetLoadProgress(IPackLoadProgress *progress) { m_resources.SetLoadProgress(progress); }

    /** Which pack a hash lands on. The reference decides, not its owner. */
    const std::string &GetPackName(std::uint32_t packHash) {
        return m_resources.GetPackName(packHash);
    }

    bool ReadSectionResource(std::uint32_t packHash, GameSection section,
                             std::uint32_t ordinal,
                             std::vector<std::uint8_t> &payload) {
        const int packIndex = m_tocManager.GetPackIndexFromHash(packHash);
        if (packIndex < 0) { return false; }
        const std::uint32_t handle =
            m_objectPacks[packIndex].GetHandle(section, ordinal);
        if (handle == 0) {
            return false;
        }
        return m_resources.Read(packIndex, handle, payload);
    }

private:
    CResTOCManager &m_tocManager;
    std::vector<CGameObjectPack> m_objectPacks;
    ResourcePacks m_resources;
};

#endif  // GUN_BROS_RE_MILESTONES_PACKTABLES_H
