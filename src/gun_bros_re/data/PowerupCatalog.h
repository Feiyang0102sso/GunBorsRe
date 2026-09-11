/** @file PowerupCatalog.h
 * @brief Full consumable catalog and permanent iOS-script check.
 */
#ifndef GUN_BROS_RE_POWERUPCATALOG_H
#define GUN_BROS_RE_POWERUPCATALOG_H
#include "gun_bros_re/gameplay/CPowerup.h"
#include "gun_bros_re/data/PackTables.h"
#include <string>
struct PowerupEntry {
    GameObjectRef resource;
    CPowerup::Template data;
    std::string name;
    std::string owner;
};
bool LoadPowerupCatalog(CResTOCManager &toc, PackTables &tables, std::vector<PowerupEntry> &catalog);
/** Shared availability gate for completed desktop consumable implementations. */
bool IsPlayablePowerup(const GameObjectRef &resource);
#endif
