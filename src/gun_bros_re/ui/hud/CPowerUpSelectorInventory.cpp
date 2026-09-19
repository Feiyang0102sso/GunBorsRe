/** Selector inventory and equipment: SetupPowerUps :186087, Bind :186808.
 * Separate availability, request and the actual grenade inventory commit.
 * POWERUP/store_entry.bt provide resources; CBrother and CLevel execute uses.
 */
#define NOMINMAX
#include "gun_bros_re/ui/hud/CPowerUpSelector.h"
#include "gun_bros_re/gameplay/multiplayer/bot/ZLocalPVPBot.h"
#include "engine/core/CStringToKey.h"
#include <cstdio>

CPowerUpSelector::CPowerUpSelector()
    : m_ownedResources(std::make_unique<ZHudResources>()), m_resources(*m_ownedResources) {}

CPowerUpSelector::CPowerUpSelector(CResTOCManager &toc, CGunBros &tables, CBrother &player,
    CBrother::Vitals &vitals, CLevel &level, CProfileManager &profile, Collision::ObjectId owner)
    : CPowerUpSelector() {
    BindPowerups(toc, tables, player, vitals, level, profile, owner);
}

void CPowerUpSelector::BindPowerups(CResTOCManager &toc, CGunBros &tables, CBrother &player,
    CBrother::Vitals &vitals, CLevel &level, CProfileManager &profile, Collision::ObjectId owner) {
    m_resources.m_toc = &toc;
    m_resources.m_tables = &tables;
    m_player = &player;
    m_vitals = &vitals;
    m_level = &level;
    m_profile = &profile;
    m_owner = owner;
    m_powerup = std::make_unique<CPowerup>(toc, tables, level);
    m_powerup->SetOwner(owner);
    m_powerup->BindActor(player, vitals);
    m_powerup->m_selector = this;
}

bool CPowerUpSelector::InitPowerups() {
    if (m_resources.m_powerups.empty() &&
        !CPowerup::LoadEntries(*m_resources.m_toc, *m_resources.m_tables, m_resources.m_powerups)) { return false; }
    if (m_resources.m_store.empty() &&
        !CStoreItem::LoadEntries(*m_resources.m_toc, *m_resources.m_tables, m_resources.m_store)) { return false; }
    // The retail selector is defined by dedicated STORE records. Validate their
    // POWERUP references instead of maintaining a second host-side ID list.
    for (const auto &store : m_resources.m_store) {
        if (store.data.type < 10 || store.data.type > 13) { continue; }
        for (const auto &reference : store.data.objects) {
            if (reference.type != 17) { continue; }
            bool found = false;
            for (const auto &entry : m_resources.m_powerups) {
                if (entry.resource.packHash == reference.object.packHash &&
                    entry.resource.localIndex == reference.object.localIndex) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::printf("[powerup] unresolved store reference resource=%08x:%u\n",
                    reference.object.packHash, reference.object.localIndex);
                return false;
            }
        }
    }
    return true;
}

const CStoreItem *CPowerUpSelector::FindStoreItem(const CPowerup::Entry &entry) const {
    // Follow the same dedicated STORE -> POWERUP reference as the selector;
    // bundle contents do not define an individual powerup's mode restrictions.
    for (const auto &store : m_resources.m_store) {
        const auto &item = store.data;
        if (item.type < 10 || item.type > 13 || item.objects.empty()) { continue; }
        const auto &reference = item.objects.front();
        if (reference.type == 17 && reference.object.packHash == entry.resource.packHash &&
            reference.object.localIndex == entry.resource.localIndex) { return &item; }
    }
    return nullptr;
}

bool CPowerUpSelector::ModeAllows(const CPowerup::Entry &entry) const {
    const auto *store = FindStoreItem(entry);
    if (store == nullptr) { return false; }
    unsigned gameType = 0;
    if (m_level->IsLocalLive()) { gameType = 1; }
    if (m_match != nullptr || m_level->IsDeathmatch()) { gameType = 2; }
    return !store->IsExcludedFromGameType(gameType);
}

GameObjectRef CPowerUpSelector::GetEquipped(unsigned slot) {
    if (slot >= 2) { return {}; }
    const auto ordinal = m_profile->configuration.powerups[slot];
    bool excluded = false;
    // Bind :187378 searches the selector catalog by its stored ordinal.
    for (const auto &entry : m_resources.m_powerups) {
        if (entry.resource.localIndex != ordinal) { continue; }
        if (IsSupported(entry)) { return entry.resource; }
        // Bind :187421 replaces a mode-excluded saved choice via export 4.
        excluded = !ModeAllows(entry);
        break;
    }
    if (ordinal != 255 && !excluded) {
        std::printf("[powerup] unresolved equipped slot=%u ordinal=%u\n", slot, ordinal);
        return {};
    }
    for (const auto &entry : m_resources.m_powerups) {
        CPowerup query;
        query.SetLevelContext(m_level);
        query.Bind(entry.data);
        if (IsSupported(entry) && query.Query(4, slot)) {
            m_profile->configuration.powerups[slot] = entry.resource.localIndex;
            if (excluded) {
                std::printf("[powerup] mode excluded slot=%u ordinal=%u default=%u\n", slot, ordinal, entry.resource.localIndex);
            }
            return entry.resource;
        }
    }
    return {};
}

bool CPowerUpSelector::Equip(unsigned slot, const GameObjectRef &resource) {
    if (slot >= 2) { return false; }
    for (const auto &entry : m_resources.m_powerups) {
        if (entry.resource.packHash != resource.packHash || entry.resource.localIndex != resource.localIndex) { continue; }
        CPowerup query;
        query.SetLevelContext(m_level);
        query.Bind(entry.data);
        if (!IsSupported(entry) || !query.Query(0)) { return false; }
        m_profile->configuration.powerups[slot] = resource.localIndex;
        std::printf("[powerup] equipped slot=%u resource=%08x:%u\n", slot, resource.packHash, resource.localIndex);
        return true;
    }
    return false;
}

bool CPowerUpSelector::IsSupported(const CPowerup::Entry &entry) const {
    if (!ModeAllows(entry)) { return false; }
    if (!ZLocalPVPBot::AllowsPowerup(*this, entry)) { return false; }
    return true;
}

const CPowerup::Entry *CPowerUpSelector::GetSelected() const {
    if (m_selected >= m_resources.m_powerups.size()) { return nullptr; }
    return &m_resources.m_powerups[m_selected];
}

unsigned CPowerUpSelector::GetCount() const {
    const CPowerup::Entry *entry = GetSelected();
    if (entry == nullptr) { return 0; }
    // A virtual charge avoids granting or persisting fake account inventory.
    if (ZLocalPVPBot::HasUnlimitedInventory(*this) && IsSupported(*entry)) { return 1; }
    return m_profile->GetPowerupCount(entry->resource);
}

unsigned CPowerUpSelector::GetCount(unsigned localIndex) const {
    for (const CPowerup::Entry &entry : m_resources.m_powerups) {
        if (entry.resource.localIndex != localIndex) { continue; }
        if (ZLocalPVPBot::HasUnlimitedInventory(*this) && IsSupported(entry)) { return 1; }
        return m_profile->GetPowerupCount(entry.resource);
    }
    return 0;
}

bool CPowerUpSelector::Select(unsigned index) {
    if (index >= m_resources.m_powerups.size() || !IsSupported(m_resources.m_powerups[index])) { return false; }
    m_selected = index;
    return true;
}

bool CPowerUpSelector::SelectResource(const GameObjectRef &resource) {
    for (unsigned index = 0; index < m_resources.m_powerups.size(); ++index) {
        const GameObjectRef &candidate = m_resources.m_powerups[index].resource;
        if (candidate.packHash == resource.packHash && candidate.localIndex == resource.localIndex) { return Select(index); }
    }
    return false;
}

void CPowerUpSelector::Cycle() {
    for (unsigned step = 1; step <= m_resources.m_powerups.size(); ++step) {
        const unsigned index = (m_selected + step) % m_resources.m_powerups.size();
        if (IsSupported(m_resources.m_powerups[index]) && m_profile->GetPowerupCount(m_resources.m_powerups[index].resource) > 0) {
            Select(index);
            return;
        }
    }
}

bool CPowerUpSelector::HasAfterDeathPowerup() const {
    if (m_match != nullptr) { return false; }
    for (const auto &entry : m_resources.m_powerups) {
        if (entry.data.field112 != 0 && IsSupported(entry) && m_profile->GetPowerupCount(entry.resource) > 0) { return true; }
    }
    return false;
}

bool CPowerUpSelector::UseAfterDeathPowerup() {
    if (m_match != nullptr) { return false; }
    for (unsigned index = 0; index < m_resources.m_powerups.size(); ++index) {
        if (m_resources.m_powerups[index].data.field112 != 0 && Select(index) && UseSelected(true)) { return true; }
    }
    return false;
}

bool CPowerUpSelector::UseSelected(bool fromSelector) {
    if (m_player == nullptr || !m_player->weapon) { return false; }
    return m_player->UsePowerup(*this, fromSelector);
}
