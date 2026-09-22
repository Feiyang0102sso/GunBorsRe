/** @file CLevelEnemyEvents.cpp
 * Original: src/gunbros/level.cpp OnEnemyKilled :119306, OnEnemyTeleport :118252,
 * SpawnEnemy and levelObjectPool.cpp GetEnemy :145509. CLevel implementation.
 */
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/data/profile/CFriendPowerManager.h"
#include "engine/core/CMatrix4d.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// CEffectLayer::AddTextEffect :66884 has twenty fixed text-effect slots.
constexpr unsigned kTextEffectCapacity = 20;

void CLevel::OnEnemyKilled(int objectId, const GameObjectRef &enemy) {
    // Count delivered deaths once, including the event that clears the level.
    ++m_kills;
    if (m_template == nullptr || m_cleared) {
        return;
    }
    ++m_statisticsKills[m_statisticsGroup];
    int resourceIndex = -1;
    const auto &resources = m_template->script.GetResources();
    for (std::size_t index = 0; index < resources.size(); ++index) {
        const ZScriptResourceRef &ref = resources[index];
        if (ref.packHash == enemy.packHash && ref.resourceId == enemy.localIndex && ref.sectionOrType == 5) {
            resourceIndex = static_cast<int>(index);
            break;
        }
    }
    m_interpreter.CallExportFunction(5, static_cast<std::int16_t>(objectId),
        static_cast<std::int16_t>(resourceIndex));
}

void CLevel::RewardEnemy(const CEnemy &actor) {
    if (m_actor.GetProgress() == nullptr) { return; }
    // CLevel::OnEnemyKilled :119609: offset 912 is XP, 876 is Xplodium.
    // Multiplier attribute 2 is Xplodium; attribute 3 is XP. Both round up.
    const GameObjectRef &ref = actor.combat.templateRef;
    const unsigned experience = static_cast<unsigned>(std::ceil(actor.data->experienceReward *
        GetEnemyMultiplier(ref, 3) * m_playerModel->GetArmorMultiplier(3) *
        CFriendPowerManager::Multiplier(m_playerModel->friendCount, 5)));
    const bool playerKill = actor.combat.pendingHit.owner == Collision::Player;
    if (m_localLive) {
        const Collision::ObjectId owner = actor.combat.pendingHit.owner;
        if (owner == Collision::Player || owner == Collision::Brother) {
            unsigned killer = 0;
            if (owner == Collision::Brother) { killer = 1; }
            ZMultiplayerStatistics &statistics = m_multiplayer[killer];
            ++statistics.wave.kills;
            ++statistics.total.kills;
            ++statistics.streak;
            statistics.total.bestStreak = std::max(statistics.total.bestStreak, statistics.streak);
            const unsigned other = 1 - killer;
            unsigned assistExperience = experience;
            if (other == 1 && m_brotherModel != nullptr) {
                assistExperience = static_cast<unsigned>(std::ceil(actor.data->experienceReward *
                    GetEnemyMultiplier(ref, 3) * m_brotherModel->GetArmorMultiplier(3) *
                    CFriendPowerManager::Multiplier(m_brotherModel->friendCount, 5)));
            }
            unsigned assisted = 0;
            for (unsigned slot = 0; slot < 2; ++slot) {
                if ((actor.assistMask[other] & (1u << slot)) == 0) { continue; }
                ++assisted;
                ++m_multiplayer[other].wave.assists;
                ++m_multiplayer[other].total.assists;
                CreditAssistMastery(other, slot, assistExperience);
            }
            if (assisted == 0) {
                unsigned slot = m_playerModel->gunSlot;
                if (other == 1) { slot = m_brotherWeaponSlot; }
                // OnEnemyKilledByBro grants mastery without a numerical assist.
                CreditAssistMastery(other, slot, assistExperience);
            }
            if (killer == 1 && m_brotherModel != nullptr) {
                const unsigned peerExperience = static_cast<unsigned>(std::ceil(actor.data->experienceReward *
                    GetEnemyMultiplier(ref, 3) * m_brotherModel->GetArmorMultiplier(3) *
                    CFriendPowerManager::Multiplier(m_brotherModel->friendCount, 5)));
                const unsigned peerXplodium = static_cast<unsigned>(std::ceil(actor.data->xplodiumReward *
                    GetEnemyMultiplier(ref, 2) * m_brotherModel->GetArmorMultiplier(4) *
                    CFriendPowerManager::Multiplier(m_brotherModel->friendCount, 6)));
                AddPeerExperience(peerExperience);
                AddPeerXplodium(peerXplodium);
                if (!actor.combat.pendingHit.weapon.IsNull()) {
                    CreditAssistMastery(1,
                        actor.combat.pendingHit.weaponSlot, peerExperience);
                }
            }
        }
    }

    bool counted = false;
    for (CEnemyCasualty &entry : m_casualties) {
        if (entry.resource.packHash == ref.packHash && entry.resource.localIndex == ref.localIndex) {
            ++entry.count;
            counted = true;
            break;
        }
    }
    if (!counted) { m_casualties.push_back({ref, 1, actor.data->owner}); }

    const Collision::Hit &hit = actor.combat.pendingHit;
    // Original CLevel::OnEnemyKilled :119912, six-byte statistic key.
    if (playerKill || hit.owner == Collision::Brother) {
        bool recorded = false;
        for (CChallengeManager::Kill &kill : m_challengeKills) {
            if (kill.enemy.packHash == ref.packHash && kill.enemy.localIndex == ref.localIndex &&
                kill.bullet.packHash == hit.bullet.packHash && kill.bullet.localIndex == hit.bullet.localIndex &&
                kill.group == m_statisticsGroup && kill.critical == hit.critical && kill.player == playerKill) {
                ++kill.count;
                recorded = true;
                break;
            }
        }
        if (!recorded) {
            m_challengeKills.push_back({ref, hit.bullet, m_statisticsGroup, 1, hit.critical, playerKill});
        }
    }
    if (playerKill && !hit.weapon.IsNull()) {
        bool credited = false;
        for (CGun::Progress &entry : m_weaponProgress) {
            if (entry.resource.packHash == hit.weapon.packHash &&
                entry.resource.localIndex == hit.weapon.localIndex) {
                entry.experience += experience;
                credited = true;
                break;
            }
        }
        if (!credited) {
            m_weaponProgress.push_back({hit.weapon, experience, hit.weaponMasteryLimit});
        }
    }

    if (m_horde) {
        // CLevel::OnEnemyKilled :119655-119802: bro kills score once, player
        // kills twice at the current streak multiplier and advance that streak.
        std::uint64_t points = static_cast<std::uint64_t>(experience) * (m_killStreak + 1);
        if (playerKill) {
            points *= 2;
            ++m_killStreak;
        }
        m_bestKillStreak = std::max(m_bestKillStreak, m_killStreak);
        m_score = static_cast<unsigned>(std::min<std::uint64_t>(3000000000ULL, m_score + points));
        if (playerKill) { AddExperience(experience); }
    } else if (!m_localLive || playerKill) {
        AddExperience(experience);
    }

    // CLevel::OnEnemyKilled VA0x950AC captures the projected position once.
    if ((!m_localLive || playerKill) && m_experienceTexts.size() < kTextEffectCapacity) {
        const CEnemy::CombatState &enemy = actor.combat;
        ExperienceText text;
        text.amount = experience;
        text.x = static_cast<float>(static_cast<int>((enemy.x - m_textViewX) * m_textScaleX));
        text.y = static_cast<float>(static_cast<int>((enemy.y - m_textViewY) * m_textScaleY));
        m_experienceTexts.push_back(text);
    }
    if (hit.owner == Collision::Player) {
        const unsigned xplodium = static_cast<unsigned>(std::ceil(actor.data->xplodiumReward *
            GetEnemyMultiplier(ref, 2) * m_playerModel->GetArmorMultiplier(4) *
            CFriendPowerManager::Multiplier(m_playerModel->friendCount, 6)));
        AddXplodium(xplodium);
    }
}

void CLevel::QueueEnemyTeleport(int objectId, const GameObjectRef &enemy) {
    m_pendingTeleports.push_back({objectId, enemy});
}

void CLevel::OnEnemyTeleport(int objectId, const GameObjectRef &enemy) {
    if (m_template == nullptr || m_cleared) { return; }
    int resourceIndex = -1;
    const auto &resources = m_template->script.GetResources();
    for (std::size_t index = 0; index < resources.size(); ++index) {
        const auto &ref = resources[index];
        if (ref.packHash == enemy.packHash && ref.resourceId == enemy.localIndex && ref.sectionOrType == 5) {
            resourceIndex = static_cast<int>(index);
            break;
        }
    }
    // CLevel::OnEnemyTeleport :118252 calls export 9, independently of kills.
    RemoveIndicator(objectId);
    m_interpreter.CallExportFunction(9, static_cast<std::int16_t>(objectId), static_cast<std::int16_t>(resourceIndex));
    std::printf("[level] enemy teleported id=%d resource=%d\n", objectId, resourceIndex);
}

float CLevel::GetEnemyMultiplier(int enemy, int attribute) const {
    if (attribute < 0 || attribute >= 5) {
        return 1;
    }
    float multiplier = m_globalEnemyMultipliers[attribute];
    if (enemy >= 0 && enemy < 32) {
        multiplier *= m_enemyMultipliers[enemy][attribute];
    }
    return multiplier;
}

float CLevel::GetEnemyMultiplier(const GameObjectRef &enemy, int attribute) const {
    // Original lookup defaults to resource zero when the type is not listed.
    int resourceIndex = 0;
    if (m_template != nullptr) {
        const auto &resources = m_template->script.GetResources();
        for (unsigned index = 0; index < resources.size(); ++index) {
            const auto &resource = resources[index];
            if (resource.sectionOrType == 5 && resource.packHash == enemy.packHash && resource.resourceId == enemy.localIndex) {
                resourceIndex = static_cast<int>(index);
                break;
            }
        }
    }
    return GetEnemyMultiplier(resourceIndex, attribute);
}
