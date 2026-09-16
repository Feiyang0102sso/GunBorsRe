/** @file ZPickupCatalog.h
 * @brief Original pickup resources and collection-script verification.
 */
#ifndef GUN_BROS_RE_ZPICKUPCATALOG_H
#define GUN_BROS_RE_ZPICKUPCATALOG_H
#include "gun_bros_re/data/ZPackTables.h"
#include "gun_bros_re/gameplay/CPickup.h"
struct ZPickupEntry {
    GameObjectRef ref;
    std::string owner;
    std::string name;
    CPickup::Template data;
};
bool LoadPickupCatalog(CResTOCManager &toc, ZPackTables &tables, std::vector<ZPickupEntry> &catalog);
#endif
