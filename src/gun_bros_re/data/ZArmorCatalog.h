/** @file ZArmorCatalog.h
 * @brief Archive armour catalogue shared by equipment and its research harness.
 */
#ifndef GUN_BROS_RE_ZARMORCATALOG_H
#define GUN_BROS_RE_ZARMORCATALOG_H

#include "gun_bros_re/data/ZPackTables.h"
#include "gun_bros_re/gameplay/armor/CArmor.h"

struct ZArmorEntry {
    std::uint32_t packHash = 0;
    std::uint32_t ordinal = 0;
    CArmor::Template data;
    std::string owner;
};

bool LoadArmorCatalog(CResTOCManager &toc, ZPackTables &tables,
    std::vector<ZArmorEntry> &armor);

#endif
