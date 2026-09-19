/** @file CRefinementManager.cpp
 * @brief Template :177773, Init :178413, Commit :178471, Collect :178237.
 */
#include "gun_bros_re/data/profile/CRefinementManager.h"
#include <cmath>

namespace {
void ReadValues(CArrayInputStream &stream, std::vector<std::uint32_t> &values) {
    values.resize(stream.ReadUInt16());
    for (std::uint32_t &value : values) { value = stream.ReadUInt32(); }
}
}

bool CRefinementManager::Template::Init(CArrayInputStream &stream) {
    ReadValues(stream, minutes);
    ReadValues(stream, efficiencyPercent);
    ReadValues(stream, commonPrice);
    // Original skips this repeated count and uses the common-price count.
    const unsigned rareCount = stream.ReadUInt16();
    rarePrice.resize(commonPrice.size());
    for (std::uint32_t &value : rarePrice) { value = stream.ReadUInt32(); }
    gate.resize(stream.ReadUInt16());
    for (std::uint8_t &value : gate) { value = static_cast<std::uint8_t>(stream.ReadUInt32()); }
    return !stream.Overran() && minutes.size() == kRefinementSlotCount &&
        efficiencyPercent.size() == kRefinementSlotCount && commonPrice.size() == kRefinementSlotCount &&
        rareCount == kRefinementSlotCount && gate.size() == kRefinementSlotCount;
}

void CRefinementManager::Bind(const Template &data) {
    m_template = &data;
    for (unsigned index = 0; index < slots.size(); ++index) {
        slots[index] = CRefinementSlot();
        if (data.commonPrice[index] == 0 && data.rarePrice[index] == 0 && !IsGated(index)) {
            slots[index].state = 1;
        }
    }
}

bool CRefinementManager::IsGated(unsigned slot) const {
    if (slot >= slots.size()) { return true; }
    return m_template->gate[slot] != 0 && m_template->gate[slot] != slot;
}

bool CRefinementManager::UnlockSlot(unsigned slot, std::uint64_t &coins, std::uint64_t &warbucks) {
    if (slot >= slots.size() || slots[slot].state != 0) { return false; }
    // Free gated intervals are opened by BeginRefinement on their prerequisite.
    if (IsGated(slot)) { return false; }
    if (m_template->commonPrice[slot] != 0) {
        if (coins < m_template->commonPrice[slot]) { return false; }
        coins -= m_template->commonPrice[slot];
    } else {
        if (warbucks < m_template->rarePrice[slot]) { return false; }
        warbucks -= m_template->rarePrice[slot];
    }
    slots[slot].state = 1;
    return true;
}

bool CRefinementManager::BeginRefinement(unsigned slot, unsigned interval, std::uint64_t amount,
    std::uint64_t &xplodium, std::int64_t now) {
    if (slot >= slots.size() || interval >= slots.size() || slots[slot].state != 1 ||
        slots[interval].state == 0 || amount == 0 || amount > xplodium) {
        return false;
    }
    CRefinementSlot &target = slots[slot];
    target.amount = amount;
    // CRefinementManager::GetEfficiency adds percentage points, not a factor.
    target.efficiency = (m_template->efficiencyPercent[interval] + friendEfficiencyBonus) / 100.0f;
    target.finishTime = now + static_cast<std::int64_t>(m_template->minutes[interval]) * 60;
    target.startTimeSeconds = static_cast<std::uint32_t>(now);
    target.totalDurationMs = static_cast<std::int32_t>(m_template->minutes[interval] * 60000);
    target.finishTimeMs = target.finishTime * 1000;
    target.state = 2;
    if (target.finishTime == now) { target.state = 3; }
    xplodium -= amount;
    // Starting the prerequisite unlocks its free dependent intervals (:178550).
    for (unsigned index = 0; index < slots.size(); ++index) {
        if (m_template->gate[index] == slot && index != slot && m_template->gate[index] != 0 &&
            m_template->commonPrice[index] == 0 && slots[index].state == 0) { slots[index].state = 1; }
    }
    return true;
}

void CRefinementManager::UpdateRefinement(std::int64_t now) {
    for (CRefinementSlot &slot : slots) {
        if (slot.state == 2 && now >= slot.finishTime) { slot.state = 3; }
    }
}

std::uint64_t CRefinementManager::GetRefinementSlotYield(unsigned slot) const {
    if (slot >= slots.size()) { return 0; }
    const CRefinementSlot &value = slots[slot];
    return static_cast<std::uint64_t>(std::floor(static_cast<float>(value.amount) * value.efficiency + 0.5f));
}

bool CRefinementManager::CollectResources(unsigned slot, std::uint64_t &coins) {
    if (slot >= slots.size() || slots[slot].state != 3) { return false; }
    coins += GetRefinementSlotYield(slot);
    slots[slot] = CRefinementSlot();
    slots[slot].state = 1;
    return true;
}

#include <cstdio>
bool CRefinementManager::Template::Load(CResTOCManager &toc, CGunBros &tables, CRefinementManager::Template &data) {
    std::vector<std::uint8_t> payload;
    const unsigned hash = toc.GetPack(toc.GetCorePackIndex())->GetPackHash();
    if (!tables.ReadSectionResource(hash, ZGameSection::RefinementManager, 0, payload)) { return false; }
    CArrayInputStream stream(payload);
    return data.Init(stream) && stream.Available() == 0;
}
