/** Per-peer rewards use the same native progression and LEVEL multipliers.
 * CPlayer::AddExperience/AddXplodium and StatisticPacket; evidence in tests.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/CombatScene.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "gun_bros_re/data/CProfileManager.h"
#include <cmath>

void CombatScene::UpdatePeerIndicator(unsigned deltaMs, float left, float top, float width, float height) {
    if (!m_localLive || m_brother == nullptr || m_brotherModel == nullptr) { m_peerIndicatorVisible = false; return; }
    // CRemotePlayer::Update :229709 compares CBrother's native bounds with
    // the camera. The existing CLevelIndicator renders the original sprites.
    const float x = std::trunc(m_brother->x), y = std::trunc(m_brother->y);
    const bool outside = x + 50 < left || y + 50 < top || x - 50 > left + width || y - 50 > top + height;
    if (outside) {
        unsigned type = 4 + m_brotherModel->brotherIndex;
        if (m_brother->vitals.dead) { type = 6; }
        if (!m_peerIndicatorVisible || m_peerIndicator.type != type || m_peerIndicator.fade >= 0) { m_peerIndicator = {}; }
        m_peerIndicator.type = type;
        m_peerIndicator.x = m_brother->x; m_peerIndicator.y = m_brother->y;
        m_peerIndicator.targetKey = kBrotherCombatId;
        m_peerIndicatorVisible = true;
    } else if (m_peerIndicatorVisible) { m_peerIndicator.FadeOut(); }
    if (m_peerIndicatorVisible) {
        m_peerIndicator.Update(deltaMs);
        if (m_peerIndicator.IsDone()) { m_peerIndicatorVisible = false; }
    }
}

bool CombatScene::SetReviveResources(const CScript &script) {
    const auto &resources = script.GetResources();
    if (resources.size() < 4) { return false; }
    for (unsigned index = 0; index < 2; ++index) {
        const auto &resource = resources[index + 2];
        if (resource.resourceId > 255) { return false; }
        m_reviveEffects[index].packHash = resource.packHash;
        m_reviveEffects[index].localIndex = static_cast<std::uint8_t>(resource.resourceId);
    }
    return true;
}

void CombatScene::SetGunConfiguration(unsigned peer, unsigned slot, const GameObjectRef &ref, unsigned masteryLimit) {
    m_gunConfigurations[peer][slot] = ref;
    m_gunMasteryLimits[peer][slot] = masteryLimit;
    if (peer == 1 && m_brotherModel != nullptr && slot == m_brotherWeaponSlot) { m_brotherModel->gunResource = ref; }
}

void CombatScene::CreditAssistMastery(unsigned peer, unsigned slot, unsigned experience) {
    const auto &ref = m_gunConfigurations[peer][slot];
    const auto limit = m_gunMasteryLimits[peer][slot];
    if (ref.IsNull()) { return; }
    // CGun::OnEnemyKilledAssist :127888 changes weapon XP, not player XP.
    if (peer == 1) {
        if (m_peerProfile != nullptr) { m_peerProfile->AddWeaponExperience(ref, experience, limit); }
        return;
    }
    for (auto &entry : m_weaponProgress) {
        if (entry.resource.packHash == ref.packHash && entry.resource.localIndex == ref.localIndex) { entry.experience += experience; return; }
    }
    m_weaponProgress.push_back({ref, experience, limit});
}

bool CombatScene::BrotherTouchesPickup(float x, float y) const {
    if (!m_localLive || m_brother == nullptr || m_brother->vitals.dead) { return false; }
    return std::hypot(x - m_brother->x, y - m_brother->y) <= m_playerRadius + 10;
}

void CombatScene::ClearWaveStatistics() {
    for (auto &stats : m_multiplayer) { stats.wave = {}; }
}

void CombatScene::AddPeerExperience(unsigned amount) {
    if (!m_localLive || m_peerProgress == nullptr || IsTeamDeathComplete()) { return; }
    const auto before = m_peerProgress->GetExperience();
    const float fraction = m_brother->vitals.health / m_brother->vitals.maximum;
    if (m_peerProgress->AddExperience(amount)) {
        m_brother->vitals.maximum = m_peerProgress->GetHealth();
        m_brother->vitals.health = m_brother->vitals.maximum * fraction;
    }
    const auto earned = m_peerProgress->GetExperience() - before;
    m_multiplayer[1].wave.experience += earned;
    m_multiplayer[1].total.experience += earned;
}

void CombatScene::AddPeerXplodium(unsigned amount) {
    if (!m_localLive || IsTeamDeathComplete()) { return; }
    auto &stats = m_multiplayer[1];
    unsigned percent = 100;
    if (m_level != nullptr) { percent = static_cast<unsigned>(std::max(0, m_level->GetXplodiumMultiplierPercent())); }
    const std::uint64_t scaled = static_cast<std::uint64_t>(amount) * percent + stats.xplodiumRemainder;
    stats.wave.xplodium += scaled / 100;
    stats.total.xplodium += scaled / 100;
    stats.xplodiumRemainder = static_cast<unsigned>(scaled % 100);
}
