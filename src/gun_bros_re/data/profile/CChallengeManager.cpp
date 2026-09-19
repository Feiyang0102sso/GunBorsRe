#include "gun_bros_re/data/profile/CChallengeManager.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <cstdio>

namespace {
using LocalCounters = CChallengeManager::Counters;
unsigned CountBits(unsigned value) {
    unsigned count = 0;
    while (value != 0) { count += value & 1; value >>= 1; }
    return count;
}
}

bool CChallengeManager::Template::Init(CArrayInputStream &stream) {
    // entries/challenge_template.bt; Template::Init :239005. Packed LE fields.
    category = stream.ReadUInt8();
    name.Init(stream);
    description.Init(stream);
    for (auto &count : participationRequired) { count = stream.ReadUInt8(); }
    for (auto &prize : prizes) { prize.Init(stream); }
    level.Init(stream);
    flags = stream.ReadUInt32();
    requiredKills = stream.ReadUInt16();
    circumstanceMask = stream.ReadUInt32();
    gunCategoryMask = stream.ReadUInt32();
    weapons.resize(stream.ReadUInt8());
    for (auto &weapon : weapons) {
        weapon.type = stream.ReadUInt8();
        weapon.object.Init(stream);
    }
    enemies.resize(stream.ReadUInt8());
    for (auto &enemy : enemies) { enemy.Init(stream); }
    firstWave = stream.ReadUInt16();
    lastWave = stream.ReadUInt16();
    perfectWaves = stream.ReadUInt16();
    levelIncreases = stream.ReadUInt16();
    friendIncreases = stream.ReadUInt16();
    powerups.resize(stream.ReadUInt8());
    for (auto &powerup : powerups) { powerup.Init(stream); }
    return !stream.Overran() && stream.Available() == 0;
}

bool CChallengeManager::Load(CResTOCManager &toc, CGunBros &tables) {
    templates.clear();
    packTemplates.clear();
    packTemplates.resize(toc.GetPackCount());
    for (unsigned pack = 0; pack < toc.GetPackCount(); ++pack) {
        const unsigned count = tables.GetObjectPack(pack).GetObjectCount(static_cast<ZGameSection>(27));
        for (unsigned index = 0; index < count; ++index) {
            Template entry;
            entry.reference.packHash = toc.GetPack(pack)->GetPackHash();
            entry.reference.localIndex = static_cast<std::uint8_t>(index);
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(entry.reference.packHash, static_cast<ZGameSection>(27), index, bytes)) { return false; }
            CArrayInputStream stream(bytes);
            if (!entry.Init(stream)) {
                std::printf("[challenge] invalid template pack=%08x ordinal=%u offset=%zu size=%zu\n",
                    entry.reference.packHash, index, stream.Position(), bytes.size());
                return false;
            }
            packTemplates[pack].push_back(static_cast<unsigned>(templates.size()));
            templates.push_back(std::move(entry));
        }
    }
    std::printf("[challenge] local templates=%zu packs=%zu\n", templates.size(), packTemplates.size());
    return !templates.empty();
}

std::vector<unsigned> CChallengeManager::GenerateChallengeList(unsigned day) const {
    // GenerateChallengeList :239247. CRandGen uses MT19937 (:370208), with
    // modulo-inclusive GetRandRange (:370346), not std::shuffle/distribution.
    std::mt19937 random(static_cast<unsigned>(templates.size()));
    std::vector<unsigned> packs(packTemplates.size());
    std::iota(packs.begin(), packs.end(), 0);
    for (unsigned index = 0; index < packs.size(); ++index) {
        std::swap(packs[index], packs[random() % packs.size()]);
    }
    std::vector<unsigned> populated;
    for (unsigned pack : packs) {
        if (!packTemplates[pack].empty()) { populated.push_back(pack); }
    }
    std::vector<unsigned> result;
    if (populated.empty()) { return result; }
    for (unsigned category = 1; category <= 5; ++category) {
        std::vector<unsigned> matching;
        for (unsigned step = 0; step < populated.size(); ++step) {
            const unsigned pack = populated[(category - 1 + step) % populated.size()];
            std::vector<unsigned> entries = packTemplates[pack];
            for (unsigned index = 0; index < entries.size(); ++index) {
                std::swap(entries[index], entries[random() % entries.size()]);
            }
            for (unsigned entry : entries) {
                if (templates[entry].category == category) { matching.push_back(entry); }
            }
        }
        if (!matching.empty()) { result.push_back(matching[day % matching.size()]); }
    }
    return result;
}

bool CChallengeManager::Bind(CResTOCManager &toc, CGunBros &tables,
    const CProfileManager &profile, unsigned seconds) {
    if (templates.empty() && !Load(toc, tables)) { return false; }
    std::vector<unsigned> progress, rewards, completedFriends;
    std::vector<LocalCounters> counters;
    cycleDay = 0;
    if (profile.nativeArchive) {
        // DataStore 1017 v5, CChallengeManager::LoadFromDisk. The archive
        // validates the complete record; preserve its cycle-to-slot mapping.
        const auto &record = profile.nativeArchive->records[17];
        if (record.version != 5) { return false; }
        CArrayInputStream stream(record.payload);
        cycleDay = stream.ReadUInt32();
        stream.ReadUInt8(); // New challenge flag.
        stream.ReadUInt8(); // New request flag.
        progress.resize(stream.ReadUInt8());
        for (auto &value : progress) { value = stream.ReadUInt8(); }
        rewards.resize(stream.ReadUInt8());
        for (auto &value : rewards) { value = stream.ReadUInt8(); }
        completedFriends.resize(stream.ReadUInt8());
        for (auto &count : completedFriends) {
            const unsigned friends = stream.ReadUInt8();
            for (unsigned index = 0; index < friends; ++index) {
                stream.ReadInt32(); // Remote client ID is retained in the archive.
                if (stream.ReadUInt8() == 100) { ++count; }
                stream.Skip(3); // Requested, incoming, alignment.
            }
        }
        counters.resize(stream.ReadUInt8());
        for (auto &counter : counters) {
            counter.kills = stream.ReadUInt16();
            stream.Skip(2); // Original serialized struct alignment.
            counter.clearedWaves = stream.ReadUInt32();
            counter.perfectWaves = stream.ReadUInt32();
            counter.initialLevel = stream.ReadUInt16();
            counter.initialFriends = stream.ReadUInt16();
            counter.usedPowerups = stream.ReadUInt32();
        }
        if (stream.Overran()) { return false; }
    }
    const bool savedCycle = cycleDay != 0;
    // InitProgressData :242162 applies UTC + 10 hours before daily division.
    // The Windows local service supplies the clock only; no server records
    // or invented remote participants are required to load these templates.
    if (!savedCycle) { cycleDay = static_cast<unsigned>((std::uint64_t(seconds) + 36000) / 86400); }
    current.clear();
    for (unsigned index : GenerateChallengeList(cycleDay)) {
        const Template &entry = templates[index];
        Challenge challenge;
        challenge.templateIndex = index;
        challenge.name = tables.ReadString(entry.name);
        challenge.description = tables.ReadString(entry.description);
        if (challenge.name.empty() || challenge.description.empty()) { return false; }
        const auto slot = current.size();
        if (savedCycle && slot < progress.size()) { challenge.progress = progress[slot]; }
        if (savedCycle && slot < rewards.size()) { challenge.rewardStatus = rewards[slot]; }
        if (savedCycle && slot < completedFriends.size()) { challenge.completedFriends = completedFriends[slot]; }
        LocalCounters counter;
        if (savedCycle && slot < counters.size()) { counter = counters[slot]; }
        challenge.counters = counter;
        // GetProgressString :240190 chooses the first applicable objective.
        challenge.target = entry.requiredKills;
        challenge.achieved = counter.kills;
        if (entry.requiredKills == 0 && entry.perfectWaves != 0) {
            challenge.progressLabel = 1;
            challenge.target = entry.perfectWaves;
            challenge.achieved = counter.perfectWaves;
            if (entry.lastWave != 0) { challenge.achieved = CountBits(counter.perfectWaves); }
        } else if (entry.requiredKills == 0 && entry.lastWave != 0) {
            challenge.progressLabel = 2;
            challenge.target = entry.lastWave - entry.firstWave + 1;
            challenge.achieved = CountBits(counter.clearedWaves);
        } else if (entry.requiredKills == 0 && entry.levelIncreases != 0) {
            challenge.progressLabel = 3;
            challenge.target = entry.levelIncreases;
            CPlayerProgress playerProgress;
            if (profile.nativeArchive) { playerProgress.Bind(profile.nativeArchive->progression); }
            playerProgress.SetExperience(profile.experience);
            challenge.achieved = 0;
            if (savedCycle && playerProgress.GetLevel() > counter.initialLevel) {
                challenge.achieved = playerProgress.GetLevel() - counter.initialLevel;
            }
        } else if (entry.requiredKills == 0 && entry.friendIncreases != 0) {
            challenge.progressLabel = 4;
            challenge.target = entry.friendIncreases;
            challenge.achieved = 0; // The local service has no remote friend list.
        } else if (entry.requiredKills == 0 && !entry.powerups.empty()) {
            challenge.progressLabel = 5;
            challenge.target = static_cast<unsigned>(entry.powerups.size());
            challenge.achieved = CountBits(counter.usedPowerups);
        }
        for (unsigned tier = 0; tier < 3; ++tier) {
            const auto &ref = entry.prizes[tier];
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(ref.packHash, static_cast<ZGameSection>(19), ref.localIndex, bytes)) { return false; }
            CArrayInputStream stream(bytes);
            ZDailyPrize &prize = challenge.prizes[tier];
            // entries/prize_entry.bt; CPrize::Init :204736, including unused tail.
            prize.coins = stream.ReadUInt32();
            prize.warbucks = stream.ReadUInt32();
            prize.experience = stream.ReadUInt32();
            prize.storeItems.resize(stream.ReadUInt8());
            for (auto &item : prize.storeItems) { item.Init(stream); }
            prize.image.Init(stream);
            prize.name.Init(stream);
            prize.description.Init(stream);
            stream.ReadInt32();
            stream.ReadUInt32();
            if (stream.Overran() || stream.Available() != 0) { return false; }
        }
        std::printf("[challenge] day=%u pack=%08x ordinal=%u name=%s progress=%u\n",
            cycleDay, entry.reference.packHash, entry.reference.localIndex, challenge.name.c_str(), challenge.progress);
        current.push_back(std::move(challenge));
    }
    return !current.empty();
}
