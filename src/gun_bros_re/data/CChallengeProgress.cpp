/** Original challenge counters, day rollover and reward transactions. */
#include "gun_bros_re/data/CChallengeManager.h"
#include <algorithm>
#include <cstdio>

namespace {
bool Same(const GameObjectRef &a, const GameObjectRef &b) {
    return a.packHash == b.packHash && a.localIndex == b.localIndex;
}
unsigned Bits(unsigned value) {
    unsigned count = 0;
    while (value) { count += value & 1; value >>= 1; }
    return count;
}
unsigned PlayerLevel(const CProfileManager &profile) {
    CPlayerProgress progress;
    if (profile.nativeArchive) { progress.Bind(profile.nativeArchive->progression); }
    progress.SetExperience(profile.experience);
    return progress.GetLevel();
}
void Put(std::vector<std::uint8_t> &bytes, std::size_t offset, unsigned value, unsigned width) {
    for (unsigned index = 0; index < width; ++index) { bytes[offset + index] = static_cast<std::uint8_t>(value >> (8 * index)); }
}
}

bool CChallengeManager::StoreProgress(CProfileManager &profile) const {
    if (!profile.nativeArchive) { return false; }
    auto &record = profile.nativeArchive->records[17];
    if (record.version != 5) { return false; }
    // Patch only known fields; keep remote participants and struct padding.
    auto bytes = record.payload;
    CArrayInputStream stream(bytes);
    stream.Skip(6);
    const unsigned progressCount = stream.ReadUInt8();
    const auto progressOffset = stream.Position();
    stream.Skip(progressCount);
    const unsigned rewardCount = stream.ReadUInt8();
    const auto rewardOffset = stream.Position();
    stream.Skip(rewardCount);
    const unsigned groups = stream.ReadUInt8();
    for (unsigned group = 0; group < groups; ++group) { const unsigned count = stream.ReadUInt8(); stream.Skip(count * 8); }
    const unsigned counterCount = stream.ReadUInt8();
    const auto counterOffset = stream.Position();
    stream.Skip(counterCount * 20);
    if (stream.Overran() || current.size() > progressCount || current.size() > rewardCount || current.size() > counterCount) { return false; }
    Put(bytes, 0, cycleDay, 4);
    for (unsigned index = 0; index < current.size(); ++index) {
        const auto &challenge = current[index];
        const auto &counter = challenge.counters;
        Put(bytes, progressOffset + index, challenge.progress, 1);
        Put(bytes, rewardOffset + index, challenge.rewardStatus, 1);
        const auto offset = counterOffset + index * 20;
        Put(bytes, offset, counter.kills, 2);
        Put(bytes, offset + 4, counter.clearedWaves, 4);
        Put(bytes, offset + 8, counter.perfectWaves, 4);
        Put(bytes, offset + 12, counter.initialLevel, 2);
        Put(bytes, offset + 14, counter.initialFriends, 2);
        Put(bytes, offset + 16, counter.usedPowerups, 4);
    }
    record.payload = std::move(bytes);
    return true;
}

bool CChallengeManager::InitProgressData(CResTOCManager &toc, PackTables &tables, CProfileManager &profile, unsigned seconds) {
    if (!profile.nativeArchive || !Bind(toc, tables, profile, seconds)) { return false; }
    const unsigned day = static_cast<unsigned>((std::uint64_t(seconds) + 36000) / 86400);
    CArrayInputStream stream(profile.nativeArchive->records[17].payload);
    const unsigned savedDay = stream.ReadUInt32();
    if (savedDay >= day) { return true; }
    // CChallengeProgressData::Reset :297936; eight slots, thirty friend records.
    std::vector<std::uint8_t> bytes(6, 0);
    Put(bytes, 0, day, 4);
    bytes[4] = 1;
    for (unsigned array = 0; array < 2; ++array) { bytes.push_back(8); bytes.insert(bytes.end(), 8, 0); }
    bytes.push_back(8);
    for (unsigned slot = 0; slot < 8; ++slot) {
        bytes.push_back(30);
        for (unsigned friendIndex = 0; friendIndex < 30; ++friendIndex) {
            bytes.insert(bytes.end(), 4, 255);
            bytes.insert(bytes.end(), 4, 0);
        }
    }
    bytes.push_back(8);
    const auto offset = bytes.size();
    bytes.insert(bytes.end(), 8 * 20, 0);
    for (unsigned slot = 0; slot < 8; ++slot) { Put(bytes, offset + slot * 20 + 12, PlayerLevel(profile), 2); }
    profile.nativeArchive->records[17].payload = std::move(bytes);
    std::printf("[challenge] rollover day=%u previous=%u\n", day, savedDay);
    return Bind(toc, tables, profile, seconds);
}

void CChallengeManager::UpdateFromLevelSession(const Session &session, const std::vector<WeaponEntry> &weapons,
    const CProfileManager &profile) {
    // UpdateFromLevelSession :241437: consume this wave's statistics once.
    for (auto &challenge : current) {
        const auto &entry = templates[challenge.templateIndex];
        challenge.applicable = (entry.level.IsNull() || Same(entry.level, session.level)) && (!(entry.flags & 2) || session.gameType == 2);
        if (challenge.progress == 100 || !challenge.applicable) { continue; }
        auto &counter = challenge.counters;
        std::vector<GameObjectRef> bullets;
        const bool filterWeapons = entry.gunCategoryMask != 0 || !entry.weapons.empty();
        if (filterWeapons) {
            for (const auto &gun : session.guns) {
                for (const auto &weapon : weapons) {
                    if (weapon.packHash != gun.packHash || weapon.ordinal != gun.localIndex) { continue; }
                    if (entry.gunCategoryMask && !(entry.gunCategoryMask & (1u << weapon.data.GetCategory()))) { continue; }
                    bool allowed = entry.weapons.empty();
                    for (const auto &required : entry.weapons) { if (required.type == 6 && Same(required.object, gun)) { allowed = true; } }
                    if (allowed) { bullets.push_back(weapon.data.GetBulletRef()); }
                }
            }
            for (const auto &weapon : entry.weapons) {
                if (weapon.type == 3 && bullets.size() < 8) { bullets.push_back(weapon.object); }
            }
        }
        for (const auto &kill : session.kills) {
            // CStatisticEnemy's AI-brother bucket is excluded by the original.
            if (!kill.player) { continue; }
            if ((entry.circumstanceMask & 1) && entry.level.IsNull()) { continue; }
            const unsigned circumstance = entry.circumstanceMask & 3;
            if (circumstance == 1 && kill.group != 1) { continue; }
            if (circumstance == 2 && !kill.critical) { continue; }
            if (circumstance == 3) {
                // Verified ARM 0x178D8C / 0x178FEC: this original branch is
                // asymmetric when BOTH bullet and enemy filters are present.
                if (filterWeapons && !entry.enemies.empty()) {
                    if (kill.group != 1) { continue; }
                } else if (kill.group != 0 || !kill.critical) { continue; }
            }
            bool allowed = !filterWeapons;
            for (const auto &bullet : bullets) { if (Same(bullet, kill.bullet)) { allowed = true; } }
            if (!allowed) { continue; }
            allowed = entry.enemies.empty();
            for (const auto &enemy : entry.enemies) { if (Same(enemy, kill.enemy)) { allowed = true; } }
            if (allowed) { counter.kills = static_cast<std::uint16_t>(counter.kills + kill.count); }
        }
        if (session.waveCleared) {
            if (entry.lastWave && session.wave >= entry.firstWave && session.wave <= entry.lastWave) {
                const unsigned bit = session.wave - entry.firstWave;
                if (bit < 32) {
                    if (entry.perfectWaves != entry.lastWave - entry.firstWave + 1) { counter.clearedWaves |= 1u << bit; }
                    if (entry.perfectWaves && session.perfect) { counter.perfectWaves |= 1u << bit; }
                }
            } else if (!entry.lastWave && entry.perfectWaves && session.perfect) { ++counter.perfectWaves; }
        }
        // The original matches each used item to one outstanding requirement.
        for (const auto &used : session.powerups) {
            for (unsigned index = 0; index < entry.powerups.size() && index < 32; ++index) {
                if (!(counter.usedPowerups & (1u << index)) && Same(entry.powerups[index], used)) {
                    counter.usedPowerups |= 1u << index;
                    break;
                }
            }
        }
    }
    UpdateChallengeStatusData(profile, session.ended);
}

void CChallengeManager::UpdateChallengeStatusData(const CProfileManager &profile, bool ended) {
    const unsigned level = PlayerLevel(profile);
    for (auto &challenge : current) {
        auto &counter = challenge.counters;
        const auto &entry = templates[challenge.templateIndex];
        std::vector<float> ratios;
        const bool reset = ended && (entry.flags & 1) && challenge.progress < 100;
        // Original :241198 resets each incomplete single-session component.
        if (entry.requiredKills) {
            if (reset && counter.kills < entry.requiredKills) { counter.kills = 0; }
            ratios.push_back(float(counter.kills) / entry.requiredKills);
        }
        if (entry.perfectWaves) {
            unsigned count = counter.perfectWaves;
            if (entry.lastWave) { count = Bits(counter.perfectWaves); }
            if (reset && count < entry.perfectWaves) { counter.perfectWaves = 0; count = 0; }
            ratios.push_back(float(count) / entry.perfectWaves);
        }
        if (entry.lastWave && entry.perfectWaves != entry.lastWave - entry.firstWave + 1) {
            const unsigned target = entry.lastWave - entry.firstWave + 1;
            if (reset && Bits(counter.clearedWaves) < target) { counter.clearedWaves = 0; }
            ratios.push_back(float(Bits(counter.clearedWaves)) / target);
        }
        if (!entry.powerups.empty()) {
            if (reset && Bits(counter.usedPowerups) < entry.powerups.size()) { counter.usedPowerups = 0; }
            ratios.push_back(float(Bits(counter.usedPowerups)) / entry.powerups.size());
        }
        const unsigned gainedLevels = level - std::min(level, counter.initialLevel);
        if (entry.levelIncreases) { ratios.push_back(float(gainedLevels) / entry.levelIncreases); }
        if (entry.friendIncreases) { ratios.push_back(0); } // No fabricated remote participants.
        float total = 0;
        for (float ratio : ratios) { total += ratio; }
        if (challenge.progress != 100 && !ratios.empty()) { challenge.progress = std::min(100u, static_cast<unsigned>(total * 100 / ratios.size())); }
        challenge.achieved = counter.kills;
        if (challenge.progressLabel == 1) { challenge.achieved = counter.perfectWaves; if (entry.lastWave) { challenge.achieved = Bits(counter.perfectWaves); } }
        if (challenge.progressLabel == 2) { challenge.achieved = Bits(counter.clearedWaves); }
        if (challenge.progressLabel == 3) { challenge.achieved = gainedLevels; }
        if (challenge.progressLabel == 4) { challenge.achieved = 0; }
        if (challenge.progressLabel == 5) { challenge.achieved = Bits(counter.usedPowerups); }
    }
}

bool CChallengeManager::AwardAvailableRewards(CProfileManager &profile, const std::vector<StoreEntry> &store, unsigned &awarded) {
    awarded = 0;
    for (auto &challenge : current) {
        if (challenge.progress != 100 || challenge.rewardStatus >= 3) { continue; }
        const auto &entry = templates[challenge.templateIndex];
        CProfileManager candidate = profile;
        unsigned status = challenge.rewardStatus;
        for (unsigned tier = status; tier < 3; ++tier) {
            if (challenge.completedFriends < entry.participationRequired[tier]) { break; }
            const auto &prize = challenge.prizes[tier];
            for (const auto &ref : prize.storeItems) {
                const StoreEntry *item = nullptr;
                for (const auto &value : store) { if (Same(value.ref, ref)) { item = &value; break; } }
                if (!item) { return false; }
                const auto result = candidate.AcquireItem(item->data, PlayerLevel(candidate), true);
                if (result != PurchaseResult::Purchased && result != PurchaseResult::Owned && result != PurchaseResult::LevelLocked) { return false; }
            }
            candidate.coins += prize.coins;
            candidate.warbucks += prize.warbucks;
            candidate.experience += prize.experience;
            if (tier == 0) { ++candidate.statistics[29]; }
            ++status;
            ++awarded;
        }
        if (awarded) {
            const unsigned previous = challenge.rewardStatus;
            challenge.rewardStatus = status;
            if (!StoreProgress(candidate)) { challenge.rewardStatus = previous; return false; }
            profile = std::move(candidate);
            std::printf("[challenge] award day=%u name=%s tiers=%u\n", cycleDay, challenge.name.c_str(), awarded);
            return true;
        }
    }
    return StoreProgress(profile);
}
