/** Original daily reward references with a local calendar adapter. */
#ifndef GUN_BROS_RE_CDAILYBONUSTRACKING_H
#define GUN_BROS_RE_CDAILYBONUSTRACKING_H
#include "gun_bros_re/data/store/CStoreItem.h"
#include "gun_bros_re/data/profile/CProfileManager.h"

struct ZDailyPrize {
    unsigned coins = 0, warbucks = 0, experience = 0;
    std::vector<GameObjectRef> storeItems;
    CGameAssetRef image, name, description;
};

class CDailyBonusTracking {
public:
    static void ReadProfileData(CProfileManager &candidate, const std::vector<std::uint8_t> &bytes);
    static void WriteProfileData(const CProfileManager &profile, std::vector<std::uint8_t> &bytes);
    bool Load(CResTOCManager &toc, CGunBros &tables);
    /** Original RefreshUsageData, with seconds supplied by the host clock. */
    void RefreshUsageData(CProfileManager &profile, std::uint32_t currentSeconds) const;
    bool IsBonusAvailable(const CProfileManager &profile, std::int64_t localDay) const;
    unsigned CalculateBonus(const CProfileManager &profile, std::int64_t localDay) const;
    bool CommitBonus(CProfileManager &profile, std::int64_t localDay, const std::vector<CStoreItem::Entry> &store) const;
    std::vector<ZDailyPrize> prizes;
    unsigned value4 = 0;
};

#endif
