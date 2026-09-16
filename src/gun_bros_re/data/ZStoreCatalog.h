/** @file ZStoreCatalog.h
 * @brief Resource access for original store entries and progression tables.
 */
#ifndef GUN_BROS_RE_ZSTORECATALOG_H
#define GUN_BROS_RE_ZSTORECATALOG_H
#include "gun_bros_re/data/ZPackTables.h"
#include "gun_bros_re/data/CStoreItem.h"
#include "gun_bros_re/data/CPlayerProgress.h"
#include "gun_bros_re/data/CRefinementManager.h"

struct ZStoreEntry {
    GameObjectRef ref;
    std::string owner;
    std::string name;
    // CStoreItem::GetIapName: resolved from BIG asset[0], not a host SKU table.
    std::string productId;
    CStoreItem data;
};

class CProfileManager;
/** CStoreItemOverride local ownership rule; preserves the original resource. */
int GetStoreDisplayOrder(const CStoreItem &item, const CProfileManager &profile);

std::string ReadGameString(CResTOCManager &toc, const CGameAssetRef &ref);
bool LoadStoreCatalog(CResTOCManager &toc, ZPackTables &tables, std::vector<ZStoreEntry> &catalog);
/** Original smallest adequate IAP, or largest available when none suffices. */
int FindCurrencyOffer(const std::vector<ZStoreEntry> &catalog, unsigned currency, unsigned missing);
bool LoadPlayerProgress(CResTOCManager &toc, ZPackTables &tables, CPlayerProgress::Template &data);
bool LoadRefinementTemplate(CResTOCManager &toc, ZPackTables &tables, CRefinementManager::Template &data);
#endif
