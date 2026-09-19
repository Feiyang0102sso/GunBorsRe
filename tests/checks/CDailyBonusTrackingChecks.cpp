#include "gun_bros_re/data/profile/CRefinementManager.h"
#include "TestOutput.h"
#include "gun_bros_re/data/profile/CDailyBonusTracking.h"
#include <cstdio>
#include "Checks.h"

int RunDailyBonusCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    CGunBros tables(toc);
    CRefinementManager::Template refinement;
    std::vector<CStoreItem::Entry> store;
    CDailyBonusTracking daily;
    if (!daily.Load(toc, tables) || !CRefinementManager::Template::Load(toc, tables, refinement) || !CStoreItem::LoadEntries(toc, tables, store)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    constexpr std::int64_t firstDay = 21000;
    unsigned failures = 0;
    if (daily.prizes.size() != 5) { ++failures; }
    for (unsigned day = 0; day < 7; ++day) {
        if (daily.CalculateBonus(profile, firstDay + day) != day % daily.prizes.size() ||
            !daily.CommitBonus(profile, firstDay + day, store)) { ++failures; }
        const auto coins = profile.coins;
        if (daily.CommitBonus(profile, firstDay + day, store) || profile.coins != coins) { ++failures; }
    }
    if (daily.CalculateBonus(profile, firstDay + 9) != 0 || !daily.CommitBonus(profile, firstDay + 9, store) || profile.dailyConsecutiveDays != 1) { ++failures; }
    ++profile.dailyDayOffset;
    if (!daily.CommitBonus(profile, firstDay + 9, store) || profile.dailyConsecutiveDays != 2) { ++failures; }
    profile.perfectedWaves[0].set(499);
    if (!profile.SaveToDisk(TestOutput::Path("daily-profile-check.dat"))) { ++failures; }
    CProfileManager restored;
    restored.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!restored.LoadFromDisk(TestOutput::Path("daily-profile-check.dat")) || restored.dailyConsecutiveDays != 2 ||
        restored.dailyDayOffset != 1 || daily.IsBonusAvailable(restored, firstDay + 9) || !restored.perfectedWaves[0].test(499)) { ++failures; }
    std::printf("[daily-check] cycle=7 duplicate-claim=7 missed-day=1 cheat-day=1 reload=1 failures=%u\n", failures);
    return failures != 0;
}
