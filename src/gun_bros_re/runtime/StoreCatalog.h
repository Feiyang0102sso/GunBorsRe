/** @file StoreCatalog.h
 * @brief Resource access for original store entries and progression tables.
 */
#ifndef GUN_BROS_RE_STORECATALOG_H
#define GUN_BROS_RE_STORECATALOG_H
#include "runtime/PackTables.h"
#include "gun_bros/CStoreItem.h"
#include "gun_bros/CPlayerProgress.h"
#include "gun_bros/CRefinementManager.h"

struct StoreEntry {
    GameObjectRef ref;
    std::string owner;
    std::string name;
    CStoreItem data;
};

std::string ReadGameString(CResTOCManager &toc, const CGameAssetRef &ref);
bool LoadStoreCatalog(CResTOCManager &toc, PackTables &tables, std::vector<StoreEntry> &catalog);
/** Original smallest adequate IAP, or largest available when none suffices. */
int FindCurrencyOffer(const std::vector<StoreEntry> &catalog, unsigned currency, unsigned missing);
bool LoadPlayerProgress(CResTOCManager &toc, PackTables &tables, CPlayerProgress::Template &data);
bool LoadRefinementTemplate(CResTOCManager &toc, PackTables &tables, CRefinementManager::Template &data);
int RunProgressCheck(const std::string &bigDirectory);
#endif
