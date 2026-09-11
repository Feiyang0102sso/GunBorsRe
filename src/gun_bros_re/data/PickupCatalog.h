/** @file PickupCatalog.h
 * @brief Original pickup resources and collection-script verification.
 */
#ifndef GUN_BROS_RE_PICKUPCATALOG_H
#define GUN_BROS_RE_PICKUPCATALOG_H
#include "gun_bros_re/data/PackTables.h"
#include "gun_bros_re/gameplay/CPickup.h"
struct PickupEntry {
    GameObjectRef ref;
    std::string owner;
    std::string name;
    CPickup::Template data;
};
bool LoadPickupCatalog(CResTOCManager &toc, PackTables &tables, std::vector<PickupEntry> &catalog);
#endif
