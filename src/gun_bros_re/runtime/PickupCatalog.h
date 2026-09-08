/** @file PickupCatalog.h
 * @brief Original pickup resources and collection-script verification.
 */
#ifndef GUN_BROS_RE_PICKUPCATALOG_H
#define GUN_BROS_RE_PICKUPCATALOG_H
#include "runtime/PackTables.h"
#include "gun_bros/CPickup.h"
struct PickupEntry {
    GameObjectRef ref;
    std::string owner;
    std::string name;
    CPickup::Template data;
};
bool LoadPickupCatalog(CResTOCManager &toc, PackTables &tables, std::vector<PickupEntry> &catalog);
int RunPickupCheck(const std::string &bigDirectory);
int RunPickupRenderCheck(const std::string &bigDirectory);
#endif
