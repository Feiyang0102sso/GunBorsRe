#include "gun_bros_re/data/profile/CDailyBonusTracking.h"
#include <cstdio>

bool CDailyBonusTracking::Load(CResTOCManager &toc, CGunBros &tables) {
    std::vector<std::uint8_t> bytes;
    bool found = false;
    // InitGameObject(type, globalIndex=0) walks packs; it is not core-local.
    for (unsigned index = 0; index < toc.GetPackCount(); ++index) {
        if (tables.GetObjectPack(index).GetObjectCount(static_cast<ZGameSection>(5)) == 0) { continue; }
        const auto *pack = toc.GetPack(index);
        if (!tables.ReadSectionResource(pack->GetPackHash(), static_cast<ZGameSection>(5), 0, bytes)) { return false; }
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
        if (!tables.ReadSectionResource(ref.packHash, static_cast<ZGameSection>(19), ref.localIndex, bytes)) { return false; }
        CArrayInputStream prizeStream(bytes);
        ZDailyPrize prize;
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

void CDailyBonusTracking::RefreshUsageData(CProfileManager &profile, std::uint32_t currentSeconds) const {
    if (!profile.nativeArchive) { return; }
    // OnReActivate :77908 refreshes even when the player does not claim.
    const auto elapsed = currentSeconds - profile.dailyLastLaunchSeconds;
    if (elapsed < 172800) { profile.dailyConsecutiveSeconds += elapsed; }
    else { profile.dailyConsecutiveSeconds = 0; profile.dailyLastCommit = 0; }
    profile.dailyLastLaunchSeconds = currentSeconds;
    profile.dailyConsecutiveDays = profile.dailyConsecutiveSeconds / 86400 + 1;
}

bool CDailyBonusTracking::IsBonusAvailable(const CProfileManager &profile, std::int64_t localDay) const {
    if (profile.nativeArchive) {
        // Native caller supplies seconds from the Windows UTC clock adapter.
        // RefreshUsageData :209316 resets after a gap of at least 172800s.
        const auto elapsed = static_cast<std::uint32_t>(localDay) - profile.dailyLastLaunchSeconds;
        if (elapsed >= 172800) { return !prizes.empty(); }
        const unsigned day = (profile.dailyConsecutiveSeconds + elapsed) / 86400 + 1;
        return !prizes.empty() && profile.dailyLastCommit < day;
    }
    return !prizes.empty() && localDay + profile.dailyDayOffset > profile.dailyLastClaimDay;
}

unsigned CDailyBonusTracking::CalculateBonus(const CProfileManager &profile, std::int64_t localDay) const {
    if (profile.nativeArchive) {
        const auto elapsed = static_cast<std::uint32_t>(localDay) - profile.dailyLastLaunchSeconds;
        if (elapsed >= 172800) { return 0; }
        return ((profile.dailyConsecutiveSeconds + elapsed) / 86400) % static_cast<unsigned>(prizes.size());
    }
    if (localDay + profile.dailyDayOffset != profile.dailyLastClaimDay + 1) { return 0; }
    // CommitBonus :209500 uses (consecutiveDay - 1) modulo reward count.
    return profile.dailyConsecutiveDays % static_cast<unsigned>(prizes.size());
}

bool CDailyBonusTracking::CommitBonus(CProfileManager &profile, std::int64_t localDay, const std::vector<CStoreItem::Entry> &store) const {
    if (!IsBonusAvailable(profile, localDay)) { return false; }
    const unsigned index = CalculateBonus(profile, localDay);
    const ZDailyPrize &prize = prizes[index];
    CProfileManager candidate = profile;
    for (const auto &ref : prize.storeItems) {
        const CStoreItem::Entry *reward = nullptr;
        for (const auto &entry : store) {
            if (entry.ref.packHash == ref.packHash && entry.ref.localIndex == ref.localIndex) { reward = &entry; break; }
        }
        if (reward == nullptr) { return false; }
        if (!profile.nativeArchive) { return false; }
        CPlayerProgress playerProgress;
        playerProgress.Bind(profile.nativeArchive->progression);
        playerProgress.SetExperience(profile.experience);
        const auto result = candidate.AcquireItem(reward->data, playerProgress.GetLevel(), true);
        // AwardPrize ignores an ineligible/already-owned item and still awards
        // the currency/XP. Unsupported resources remain an explicit failure.
        if (result == CProfileManager::PurchaseResult::LevelLocked) { continue; }
        if (result != CProfileManager::PurchaseResult::Purchased && result != CProfileManager::PurchaseResult::Owned) { return false; }
    }
    // CDailyBonusTracking::CommitBonus :209500-209546: coins only.
    candidate.coins += static_cast<std::uint64_t>(prize.coins) *
        (100 + CFriendPowerManager::Bonus(profile.friendCount, 7)) / 100;
    candidate.warbucks += prize.warbucks;
    candidate.experience += prize.experience;
    if (profile.nativeArchive) {
        const auto elapsed = static_cast<std::uint32_t>(localDay) - profile.dailyLastLaunchSeconds;
        candidate.dailyConsecutiveSeconds = 0;
        if (elapsed < 172800) { candidate.dailyConsecutiveSeconds = profile.dailyConsecutiveSeconds + elapsed; }
        candidate.dailyLastLaunchSeconds = static_cast<std::uint32_t>(localDay);
        candidate.dailyLastCommit = candidate.dailyConsecutiveSeconds / 86400 + 1;
        candidate.dailyConsecutiveDays = candidate.dailyLastCommit;
        ++candidate.statistics[32]; // CommitBonus :209575, DAILY_BONUSES.
        profile = std::move(candidate);
        std::printf("[daily] native seconds=%u day=%u reward=%u\n", profile.dailyLastLaunchSeconds, profile.dailyLastCommit, index + 1);
        return true;
    }
    if (localDay + profile.dailyDayOffset != profile.dailyLastClaimDay + 1) { candidate.dailyConsecutiveDays = 0; }
    ++candidate.dailyConsecutiveDays;
    candidate.dailyLastClaimDay = localDay + profile.dailyDayOffset;
    profile = std::move(candidate);
    std::printf("[daily] claimed day=%lld streak=%u reward=%u\n", profile.dailyLastClaimDay, profile.dailyConsecutiveDays, index + 1);
    return true;
}
