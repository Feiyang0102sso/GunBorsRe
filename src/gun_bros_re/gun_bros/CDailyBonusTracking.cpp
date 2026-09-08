#include "gun_bros/CDailyBonusTracking.h"
#include <cstdio>

bool CDailyBonusTracking::Load(CResTOCManager &toc, PackTables &tables) {
    std::vector<std::uint8_t> bytes;
    bool found = false;
    // InitGameObject(type, globalIndex=0) walks packs; it is not core-local.
    for (unsigned index = 0; index < toc.GetPackCount(); ++index) {
        if (tables.GetObjectPack(index).GetObjectCount(static_cast<GameSection>(5)) == 0) { continue; }
        const auto *pack = toc.GetPack(index);
        if (!tables.ReadSectionResource(pack->GetPackHash(), static_cast<GameSection>(5), 0, bytes)) { return false; }
        std::printf("[daily] template=%s bytes=%zu\n", pack->GetShortName().c_str(), bytes.size());
        found = true;
        break;
    }
    if (!found) { return false; }
    CArrayInputStream stream(bytes);
    // CDailyBonusTracking::Template::Init :209180, not a host reward table.
    const unsigned count = stream.ReadUInt8();
    std::vector<GameObjectRef> references(count);
    for (auto &ref : references) { ref.Init(stream); }
    value4 = stream.ReadUInt16();
    if (stream.Overran() || count == 0) { return false; }
    prizes.clear();
    for (const auto &ref : references) {
        if (!tables.ReadSectionResource(ref.packHash, static_cast<GameSection>(19), ref.localIndex, bytes)) { return false; }
        CArrayInputStream prizeStream(bytes);
        DailyPrize prize;
        // CPrize::Init :204736. Remaining chance and flags are still parsed.
        prize.coins = prizeStream.ReadUInt32();
        prize.warbucks = prizeStream.ReadUInt32();
        prize.experience = prizeStream.ReadUInt32();
        prize.storeItems.resize(prizeStream.ReadUInt8());
        for (auto &item : prize.storeItems) { item.Init(prizeStream); }
        prize.image.Init(prizeStream);
        prize.name.Init(prizeStream);
        prize.description.Init(prizeStream);
        prizeStream.ReadInt32();
        prizeStream.ReadUInt32();
        if (prizeStream.Overran()) { return false; }
        prizes.push_back(prize);
        std::printf("[daily] day=%zu coins=%u warbucks=%u xp=%u items=%zu image=%d\n",
            prizes.size(), prize.coins, prize.warbucks, prize.experience, prize.storeItems.size(), prize.image.assetId);
    }
    return true;
}

bool CDailyBonusTracking::IsBonusAvailable(const CProfileManager &profile, std::int64_t localDay) const {
    return !prizes.empty() && localDay + profile.dailyDayOffset > profile.dailyLastClaimDay;
}

unsigned CDailyBonusTracking::CalculateBonus(const CProfileManager &profile, std::int64_t localDay) const {
    if (localDay + profile.dailyDayOffset != profile.dailyLastClaimDay + 1) { return 0; }
    // CommitBonus :209500 uses (consecutiveDay - 1) modulo reward count.
    return profile.dailyConsecutiveDays % static_cast<unsigned>(prizes.size());
}

bool CDailyBonusTracking::CommitBonus(CProfileManager &profile, std::int64_t localDay, const std::vector<StoreEntry> &store) const {
    if (!IsBonusAvailable(profile, localDay)) { return false; }
    const unsigned index = CalculateBonus(profile, localDay);
    const DailyPrize &prize = prizes[index];
    CProfileManager candidate = profile;
    for (const auto &ref : prize.storeItems) {
        const StoreEntry *reward = nullptr;
        for (const auto &entry : store) {
            if (entry.ref.packHash == ref.packHash && entry.ref.localIndex == ref.localIndex) { reward = &entry; break; }
        }
        if (reward == nullptr) { return false; }
        CStoreItem freeItem = reward->data;
        freeItem.commonPrice = 0;
        freeItem.rarePrice = 0;
        freeItem.requiredLevel = 0;
        const auto result = candidate.AcquireItem(freeItem, 200);
        if (result != PurchaseResult::Purchased && result != PurchaseResult::Owned) { return false; }
    }
    candidate.coins += prize.coins;
    candidate.warbucks += prize.warbucks;
    candidate.experience += prize.experience;
    if (localDay + profile.dailyDayOffset != profile.dailyLastClaimDay + 1) { candidate.dailyConsecutiveDays = 0; }
    ++candidate.dailyConsecutiveDays;
    candidate.dailyLastClaimDay = localDay + profile.dailyDayOffset;
    profile = std::move(candidate);
    std::printf("[daily] claimed day=%lld streak=%u reward=%u\n", profile.dailyLastClaimDay, profile.dailyConsecutiveDays, index + 1);
    return true;
}

int RunDailyBonusCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    CDailyBonusTracking daily;
    if (!daily.Load(toc, tables) || !LoadRefinementTemplate(toc, tables, refinement) || !LoadStoreCatalog(toc, tables, store)) { return 1; }
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
    if (!profile.SaveToDisk("out/daily-profile-check.dat")) { ++failures; }
    CProfileManager restored;
    restored.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!restored.LoadFromDisk("out/daily-profile-check.dat") || restored.dailyConsecutiveDays != 2 ||
        restored.dailyDayOffset != 1 || daily.IsBonusAvailable(restored, firstDay + 9) || !restored.perfectedWaves[0].test(499)) { ++failures; }
    std::printf("[daily-check] cycle=7 duplicate-claim=7 missed-day=1 cheat-day=1 reload=1 failures=%u\n", failures);
    return failures != 0;
}
