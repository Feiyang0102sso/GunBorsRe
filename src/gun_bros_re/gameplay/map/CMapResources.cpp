/** CMap resource lifetime, adapted to desktop GL storage; map.cpp :91849. */
#include "gun_bros_re/gameplay/map/CMapResources.h"
#include <cassert>

CMap::~CMap() = default;
CMap::CMap(CMap &&) noexcept = default;
CMap &CMap::operator=(CMap &&) noexcept = default;

CMap::Resources &CMap::GetResources() {
    if (!m_resources) { m_resources = std::make_unique<Resources>(); }
    return *m_resources;
}

const CMap::Resources &CMap::GetResources() const {
    assert(m_resources);
    return *m_resources;
}

#include "engine/resources/CResTOCManager.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace MapDetail {
/**
 * The section bases and SpriteGlu tables of one pack, built on first use.
 *
 * Keyed by pack index rather than assumed, because references cross packs:
 * pack2's maps borrow five props from pack1.
 * Returns null when the pack cannot be addressed at all.
 */
CMap::Resources::Pack *GetPackResources(CResTOCManager &tocManager, CMap &loaded,
                                int packIndex) {
    std::map<int, std::unique_ptr<CMap::Resources::Pack>>::iterator found =
        loaded.GetResources().packs.find(packIndex);
    if (found != loaded.GetResources().packs.end()) {
        return found->second.get();
    }

    CResPackTOC *pack = tocManager.GetPack(packIndex);
    if (pack == nullptr) {
        return nullptr;
    }

    std::unique_ptr<CMap::Resources::Pack> resources(new CMap::Resources::Pack());
    resources->objectPackReady = resources->objectPack.Init(*pack);
    resources->spriteGluReady = resources->spriteGlu.Init(*pack);

    CMap::Resources::Pack *result = resources.get();
    loaded.GetResources().packs[packIndex] = std::move(resources);
    return result;
}

/**
 * Read the resource a (pack hash, section, ordinal) triple names.
 *
 * Every reference in the data is one of these, and the pack hash is part of
 * the address -- CGunBros::GetGameObject picks the pack first and only then
 * adds the section base. Resolving an ordinal against the pack that happened
 * to hold the reference works right up until something points elsewhere, and
 * props already do.
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:78497
 */
bool ReadSectionResource(CResTOCManager &tocManager, CMap &loaded,
                         std::uint32_t packHash, ZGameSection section,
                         std::uint32_t localIndex,
                         std::vector<std::uint8_t> &payload) {
    const int packIndex = tocManager.GetPackIndexFromHash(packHash);
    CMap::Resources::Pack *resources = GetPackResources(tocManager, loaded, packIndex);
    if (resources == nullptr || !resources->objectPackReady) {
        std::printf("[m3] pack %08X has no section table\n", packHash);
        return false;
    }

    const std::uint32_t handle = resources->objectPack.GetHandle(section, localIndex);
    if (handle == 0) {
        std::printf("[m3] pack %08X section %u has no base\n", packHash,
                    static_cast<unsigned>(section));
        return false;
    }
    if (!tocManager.GetPack(packIndex)->GetResource(handle, payload)) {
        std::printf("[m3] handle 0x%08X unreadable\n", handle);
        return false;
    }

    return true;
}

std::uint64_t AssetKey(std::uint32_t packHash, std::uint32_t localIndex) {
    return (static_cast<std::uint64_t>(packHash) << 32) | localIndex;
}

/** Open the archives and pick out one pack, already bound. */
CResPackTOC *OpenPack(CResTOCManager &tocManager, const std::string &bigDirectory,
                      const std::string &packShortName) {
    if (!tocManager.Init(bigDirectory, kArtSetXga)) {
        return nullptr;
    }
    const int packIndex = tocManager.GetPackIndexFromName(packShortName.c_str());
    CResPackTOC *pack = tocManager.GetPack(packIndex);
    if (pack == nullptr || pack->GetShortName() != packShortName) {
        std::printf("[m3] no pack named %s\n", packShortName.c_str());
        return nullptr;
    }
    if (!tocManager.Bind()) {
        return nullptr;
    }
    return pack;
}
}
