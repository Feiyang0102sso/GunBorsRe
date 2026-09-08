/** @file ArmorCatalog.h
 * @brief Archive armour catalogue shared by equipment and its research harness.
 */
#ifndef GUN_BROS_RE_ARMORCATALOG_H
#define GUN_BROS_RE_ARMORCATALOG_H

#include "runtime/PackTables.h"
#include "gun_bros/CArmor.h"

struct ArmorEntry {
    std::uint32_t packHash = 0;
    std::uint32_t ordinal = 0;
    CArmor::Template data;
    std::string owner;
};

bool LoadArmorCatalog(CResTOCManager &toc, PackTables &tables,
    std::vector<ArmorEntry> &armor);
int RunArmorCheck(const std::string &bigDirectory);
int RunArmorRenderCheck(const std::string &bigDirectory);

#endif
