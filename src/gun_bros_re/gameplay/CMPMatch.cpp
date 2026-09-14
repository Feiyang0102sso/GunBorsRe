#include "gun_bros_re/gameplay/CMPMatch.h"
#include "gun_bros_re/data/CStoreItem.h"
#include <algorithm>
#include <cstdio>

bool CMPMatch::Template::Init(CArrayInputStream &stream) {
    mode = stream.ReadUInt8();
    stores.resize(stream.ReadUInt8());
    for (auto &ref : stores) { ref.Init(stream); }
    pickups.resize(stream.ReadUInt8());
    for (auto &ref : pickups) { ref.Init(stream); }
    const unsigned count = stream.ReadUInt16();
    pickupRules.resize(count);
    for (auto &rule : pickupRules) { rule.weight = stream.ReadInt32(); }
    discardedCounts[0] = stream.ReadUInt16();
    for (auto &rule : pickupRules) { rule.unknown = stream.ReadInt32(); }
    discardedCounts[1] = stream.ReadUInt16();
    for (auto &rule : pickupRules) { rule.seconds = stream.ReadInt32(); }
    health = stream.ReadUInt16();
    killLimit = stream.ReadUInt16();
    seconds = stream.ReadUInt16();
    respawnSeconds = stream.ReadUInt16();
    return !stream.Overran();
}

bool LoadMPMatches(CResTOCManager &toc, PackTables &tables, std::vector<CMPMatch::Entry> &entries) {
    entries.clear();
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        const auto hash = toc.GetPack(packIndex)->GetPackHash();
        const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(GameSection::MPMatch);
        for (unsigned index = 0; index < count; ++index) {
            CMPMatch::Entry entry;
            entry.resource.packHash = hash;
            entry.resource.localIndex = static_cast<std::uint8_t>(index);
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(hash, GameSection::MPMatch, index, bytes)) { return false; }
            CArrayInputStream stream(bytes);
            if (!entry.data.Init(stream) || stream.Available() != 0 || entry.data.stores.size() < 2 ||
                entry.data.health == 0 || entry.data.pickups.size() != entry.data.pickupRules.size()) {
                std::printf("[deathmatch] invalid template pack=%08x index=%u\n", hash, index);
                return false;
            }
            for (const auto &ref : entry.data.stores) {
                if (!tables.ReadSectionResource(ref.packHash, GameSection::StoreItem, ref.localIndex, bytes)) { return false; }
                CArrayInputStream storeStream(bytes);
                CStoreItem store;
                if (!store.Init(storeStream) || storeStream.Available() != 0 || store.objects.empty() || store.objects[0].type != 6) { return false; }
                // GetWeaponLoadOut :396230 resolves the first STORE physical object.
                entry.guns.push_back(store.objects[0].object);
            }
            for (const auto &rule : entry.data.pickupRules) {
                if (rule.weight < 0 || rule.seconds <= 0) {
                    std::printf("[deathmatch] invalid pickup rule pack=%08x index=%u weight=%d seconds=%d\n", hash, index, rule.weight, rule.seconds);
                    return false;
                }
            }
            std::printf("[deathmatch] template pack=%08x index=%u health=%u kills=%u respawn=%u\n",
                hash, index, entry.data.health, entry.data.killLimit, entry.data.respawnSeconds);
            entries.push_back(std::move(entry));
        }
    }
    return !entries.empty();
}

void CMPMatch::Bind(const Template &data, unsigned seed) { m_data = &data; m_random.seed(seed); Restart(); }
void CMPMatch::Restart() {
    m_lives[0] = {}; m_lives[1] = {};
    m_scores[0] = 0; m_scores[1] = 0;
    m_remainingMs = m_data->seconds * 1000;
    m_previousPickup = -1;
    m_result = Result::Playing;
}
void CMPMatch::Update(unsigned deltaMs) {
    if (m_result != Result::Playing) { return; }
    if (m_data->killLimit != 0 && (m_scores[0] >= m_data->killLimit || m_scores[1] >= m_data->killLimit)) {
        m_result = Result::Draw;
        if (m_scores[0] > m_scores[1]) { m_result = Result::PlayerWon; }
        if (m_scores[1] > m_scores[0]) { m_result = Result::BotWon; }
        return;
    }
    for (auto &life : m_lives) {
        if (deltaMs >= life.respawnMs) { life.respawnMs = 0; }
        else { life.respawnMs -= deltaMs; }
    }
    if (m_data->seconds == 0) { return; }
    if (deltaMs < m_remainingMs) { m_remainingMs -= deltaMs; return; }
    m_remainingMs = 0;
    m_result = Result::Draw;
    if (m_scores[0] > m_scores[1]) { m_result = Result::PlayerWon; }
    if (m_scores[1] > m_scores[0]) { m_result = Result::BotWon; }
}
bool CMPMatch::Kill(unsigned victim, int killer) {
    if (victim > 1 || m_result != Result::Playing || m_lives[victim].dead) { return false; }
    auto &life = m_lives[victim];
    life.dead = true;
    life.respawnMs = m_data->respawnSeconds * 1000;
    // Original two-player OnDeathMatchKill counts the opponent's deaths,
    // including environmental deaths; it does not require a bullet owner.
    ++m_scores[1 - victim];
    // Resolve score limits after the simulation tick so simultaneous kills draw.
    std::printf("[deathmatch] death peer=%u killer=%d score=%u:%u\n", victim, killer, m_scores[0], m_scores[1]);
    return true;
}
bool CMPMatch::Respawn(unsigned peer, bool resumeFromShop) {
    if (peer > 1 || m_result != Result::Playing || !m_lives[peer].dead) { return false; }
    if (m_lives[peer].respawnMs != 0 && !resumeFromShop) { return false; }
    const unsigned serial = m_lives[peer].serial + 1;
    m_lives[peer] = {};
    m_lives[peer].serial = serial;
    return true;
}
void CMPMatch::Surrender(unsigned peer) {
    if (m_result != Result::Playing) { return; }
    m_result = Result::PlayerWon;
    if (peer == 0) { m_result = Result::BotWon; }
}
bool CMPMatch::CanShop(unsigned peer) const {
    if (peer > 1 || m_result != Result::Playing || m_lives[peer].dead) { return false; }
    if (peer == 1 && HasUnlimitedBotPowerups()) { return false; }
    return peer == 0 || m_lives[peer].shops < 2;
}
bool CMPMatch::EnterShop(unsigned peer) {
    if (!CanShop(peer)) { return false; }
    ++m_lives[peer].shops;
    return true;
}
bool CMPMatch::CanUse(unsigned peer, bool grenade) const {
    if (peer > 1 || m_result != Result::Playing || m_lives[peer].dead) { return false; }
    if (peer == 0) { return true; }
    if (HasUnlimitedBotPowerups()) { return true; }
    if (grenade) { return m_lives[peer].grenades < 2; }
    return m_lives[peer].healthPacks < 2;
}
void CMPMatch::CommitUse(unsigned peer, bool grenade) {
    if (grenade) { ++m_lives[peer].grenades; }
    else { ++m_lives[peer].healthPacks; }
}
int CMPMatch::ChoosePickup() {
    // Preserve original 0..100 cumulative comparisons, including weight sums
    // below 100. Conditioning on a new valid result avoids the original retry
    // loop hanging on malformed resources. No resource weights are rewritten.
    std::vector<int> acceptedRolls;
    for (int roll = 0; roll <= 100; ++roll) {
        int total = 0;
        for (unsigned index = 0; index < m_data->pickupRules.size(); ++index) {
            total += m_data->pickupRules[index].weight;
            if (roll > total) { continue; }
            if (static_cast<int>(index) != m_previousPickup) { acceptedRolls.push_back(static_cast<int>(index)); }
            break;
        }
    }
    if (acceptedRolls.empty()) { std::printf("[deathmatch] no eligible pickup\n"); return -1; }
    m_previousPickup = acceptedRolls[std::uniform_int_distribution<unsigned>(0, static_cast<unsigned>(acceptedRolls.size() - 1))(m_random)];
    return m_previousPickup;
}
