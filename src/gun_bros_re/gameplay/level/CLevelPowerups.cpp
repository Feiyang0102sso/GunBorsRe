/** CLevel::UsePowerup :116346 and Update :121255 own active execution.
 * Inventory is committed only when the original Flow or projectile reports use.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/ui/CPowerUpSelector.h"
#include "gun_bros_re/gameplay/multiplayer/bot/ZLocalPVPBot.h"
#include <algorithm>
#include <cstdio>

bool CLevel::UsePowerup(CPowerUpSelector &selector, const ZPowerupEntry &entry, bool fromSelector, bool decrement) {
    CPowerup &powerup = selector.GetPowerup();
    const unsigned previousFailures = powerup.failures;
    if (!powerup.Start(entry, fromSelector, selector.GetCount())) {
        selector.failures += powerup.failures - previousFailures;
        return false;
    }
    if (decrement && !CommitPowerupUse(selector, entry.resource, 1)) {
        powerup.Reset();
        return false;
    }
    return true;
}

bool CLevel::CommitPowerupUse(CPowerUpSelector &selector, const GameObjectRef &resource, unsigned count) {
    if (!ZLocalPVPBot::HasUnlimitedInventory(selector) && !selector.m_profile->ConsumePowerup(resource, count)) {
        ++selector.failures;
        return false;
    }
    selector.consumed += count;
    if (selector.m_match != nullptr) {
        ZLocalPVPBot::CommitPowerupBudget(selector, resource);
        for (const auto &entry : selector.m_resources.m_powerups) {
            if (entry.resource.packHash == resource.packHash && entry.resource.localIndex == resource.localIndex) {
                selector.m_cooldowns[resource.localIndex] = entry.data.field124 * 1000;
                if (selector.m_owner == Collision::Player) { selector.m_useMessages.push_back(entry.name); }
                break;
            }
        }
    }
    if (selector.m_owner == Collision::Player) {
        for (unsigned index = 0; index < count; ++index) { RecordChallengePowerup(resource); }
    }
    return true;
}

void CLevel::UpdatePowerup(CPowerup &powerup, int deltaMs) {
    CPowerUpSelector *selector = powerup.m_selector;
    if (selector != nullptr) {
        for (auto &cooldown : selector->m_cooldowns) { cooldown.second = std::max(0, cooldown.second - deltaMs); }
    }
    powerup.Update(deltaMs);
    if (selector == nullptr) { return; }
    GameObjectRef resource;
    const unsigned thrown = powerup.TakeThrownPowerups(resource);
    if (thrown > 0) {
        CommitPowerupUse(*selector, resource, thrown);
        std::printf("[powerup] thrown=%u remaining=%u\n", thrown, selector->m_profile->GetPowerupCount(resource));
    }
}

void CLevel::ResetPowerup(CPowerup &powerup) {
    if (powerup.m_selector != nullptr) {
        powerup.m_selector->m_cooldowns.clear();
        powerup.m_selector->m_useMessages.clear();
    }
    powerup.Reset();
}
