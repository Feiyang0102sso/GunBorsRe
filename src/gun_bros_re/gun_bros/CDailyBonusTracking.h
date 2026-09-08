/** Original daily reward references with a local calendar adapter. */
#ifndef GUN_BROS_RE_CDAILYBONUSTRACKING_H
#define GUN_BROS_RE_CDAILYBONUSTRACKING_H
#include "runtime/StoreCatalog.h"
#include "gun_bros/CProfileManager.h"

struct DailyPrize {
    unsigned coins = 0, warbucks = 0, experience = 0;
    std::vector<GameObjectRef> storeItems;
    CGameAssetRef image, name, description;
};

class CDailyBonusTracking {
public:
    bool Load(CResTOCManager &toc, PackTables &tables);
    bool IsBonusAvailable(const CProfileManager &profile, std::int64_t localDay) const;
    unsigned CalculateBonus(const CProfileManager &profile, std::int64_t localDay) const;
    bool CommitBonus(CProfileManager &profile, std::int64_t localDay, const std::vector<StoreEntry> &store) const;
    std::vector<DailyPrize> prizes;
    unsigned value4 = 0;
};

int RunDailyBonusCheck(const std::string &bigDirectory);
#endif
