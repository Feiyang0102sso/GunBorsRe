/** @file ZPowerupCatalog.h
 * @brief Full consumable template catalog read from BIG.
 */
#ifndef GUN_BROS_RE_ZPOWERUPCATALOG_H
#define GUN_BROS_RE_ZPOWERUPCATALOG_H
#include "gun_bros_re/gameplay/CPowerup.h"
#include "gun_bros_re/data/ZPackTables.h"
#include <string>
struct ZPowerupEntry {
    GameObjectRef resource;
    CPowerup::Template data;
    std::string name;
    std::string owner;
};
bool LoadPowerupCatalog(CResTOCManager &toc, ZPackTables &tables, std::vector<ZPowerupEntry> &catalog);
#endif
