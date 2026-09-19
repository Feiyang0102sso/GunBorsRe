#pragma once
#include "gun_bros_re/ui/hud/ZHudState.h"

/** Shared desktop caches; selectors and controls use the same decoded resources. */
struct ZHudResources {
    CResTOCManager *m_toc = nullptr;
    mutable ZMovieRenderer m_movies;
    CGunBros *m_tables = nullptr;
    std::vector<CStoreItem::Entry> m_store;
    std::vector<CPowerup::Entry> m_powerups;
    std::map<std::uint32_t, std::unique_ptr<ZMovieRenderer>> m_powerupRenderers;
    std::map<std::uint64_t, std::unique_ptr<ZTexture>> m_icons;
    void Icon(unsigned type, const GameObjectRef &object, const ZMovieRegion &region);
};
