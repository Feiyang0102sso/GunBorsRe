/** @file ArmorCatalog.h
 * @brief Archive armour catalogue shared by equipment and its research harness.
 */
#ifndef GUN_BROS_RE_ARMORCATALOG_H
#define GUN_BROS_RE_ARMORCATALOG_H

#include "gun_bros_re/data/PackTables.h"
#include "gun_bros_re/gameplay/CArmor.h"

struct ArmorEntry {
    std::uint32_t packHash = 0;
    std::uint32_t ordinal = 0;
    CArmor::Template data;
    std::string owner;
};

bool LoadArmorCatalog(CResTOCManager &toc, PackTables &tables,
    std::vector<ArmorEntry> &armor);

#endif
