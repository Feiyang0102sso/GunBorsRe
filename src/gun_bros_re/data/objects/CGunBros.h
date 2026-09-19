#pragma once
/**
 * @file CGunBros.h
 * @brief Game-pack lifetime and resource access, restored from gunbros.cpp.
 *
 * Historical description of the earlier harness, retained for provenance:
 *
 * Harness scaffolding, not a port: the engine keeps this state inside
 * CGunBros, which is a class the project has no reason to write yet. Lifted
 * out of M35Mesh.cpp when a second harness needed it.
 *
 * A reference carries its own pack hash and it is not always the pack the
 * template came from, which is the rule M3.1 had to learn the hard way: pick
 * the pack first, then add the section base.
 */


// The historical harness note above describes the former ZPackTables scaffolding.
// CGunBros now owns the game-pack lifetime and delegates object storage to it.
// Original InitGameObject/GetGameObject: 78471/78497; no UI or session singleton.
#include "gun_bros_re/data/objects/CGameObjectPack.h"
#include "engine/resources/CResTOCManager.h"
#include "engine/resources/CResourceLoader.h"
#include <vector>

struct CGameAssetRef;
class CGunBros {
public:
    /** Desktop loading notification; nested contract, not an original class. */
    class LoadProgress {
    public:
        virtual ~LoadProgress() = default;
        virtual void OnResourceRead() = 0;
    };
    explicit CGunBros(CResTOCManager &toc);
    CGunBros(const CGunBros &) = delete;
    CGunBros &operator=(const CGunBros &) = delete;
    CGameObjectPack &GetObjectPack(int packIndex) { return m_objectPacks[packIndex]; }
    CResourceLoader &GetResourceLoader() { return m_resourceLoader; }
    /** Game entry points still use the latest rule set; older BIGs are viewer inputs. */
    bool HasLatestBigVersion() const;
    std::string ReadString(const CGameAssetRef &ref);
    void SetLoadProgress(LoadProgress *progress) { m_progress = progress; }
    /** Which pack a hash lands on. The reference decides, not its owner. */
    const std::string &GetPackName(std::uint32_t hash);
    bool ReadSectionResource(std::uint32_t hash, ZGameSection section,
        std::uint32_t ordinal, std::vector<std::uint8_t> &payload);
private:
    CResTOCManager &m_tocManager;
    std::vector<CGameObjectPack> m_objectPacks;
    LoadProgress *m_progress = nullptr;
    CResourceLoader m_resourceLoader;
};
