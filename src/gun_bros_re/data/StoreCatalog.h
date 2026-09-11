/** @file StoreCatalog.h
 * @brief Resource access for original store entries and progression tables.
 */
#ifndef GUN_BROS_RE_STORECATALOG_H
#define GUN_BROS_RE_STORECATALOG_H
#include "gun_bros_re/data/PackTables.h"
#include "gun_bros_re/data/CStoreItem.h"
#include "gun_bros_re/data/CPlayerProgress.h"
#include "gun_bros_re/data/CRefinementManager.h"

struct StoreEntry {
    GameObjectRef ref;
    std::string owner;
    std::string name;
    CStoreItem data;
};

class CProfileManager;
/** CStoreItemOverride local ownership rule; preserves the original resource. */
int GetStoreDisplayOrder(const CStoreItem &item, const CProfileManager &profile);

std::string ReadGameString(CResTOCManager &toc, const CGameAssetRef &ref);
bool LoadStoreCatalog(CResTOCManager &toc, PackTables &tables, std::vector<StoreEntry> &catalog);
/** Original smallest adequate IAP, or largest available when none suffices. */
int FindCurrencyOffer(const std::vector<StoreEntry> &catalog, unsigned currency, unsigned missing);
bool LoadPlayerProgress(CResTOCManager &toc, PackTables &tables, CPlayerProgress::Template &data);
bool LoadRefinementTemplate(CResTOCManager &toc, PackTables &tables, CRefinementManager::Template &data);
#endif
