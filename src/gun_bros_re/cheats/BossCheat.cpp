// Desktop-only fast-forward through the original survival Flow callbacks.
#include "gun_bros_re/cheats/CheatConfig.h"
#include "gun_bros_re/gameplay/SurvivalSession.h"
#include <chrono>
#include <cstdio>

bool SurvivalSession::SkipToBoss() {
    PlayerVitals &player = m_scene.GetPlayerVitals();
    if (m_archive || m_horde || m_level.GetTutorialStep() >= 0 || player.dead ||
        m_level.IsCleared() || m_level.IsPaused() ||
        (m_powerups != nullptr && m_powerups->IsMovieActive())) {
        std::printf("[stboss] unavailable in current mode or presentation\n");
        return false;
    }
    if (m_level.HasLargeEnemyHealthBars()) {
        std::printf("[stboss] boss already active\n");
        return false;
    }
    const auto started = std::chrono::steady_clock::now();
    const unsigned introSerial = m_level.GetBossIntroSerial();
    const bool playerInvincible = player.invincible;
    PlayerVitals *brother = m_scene.GetBrotherVitals();
    bool brotherInvincible = false;
    if (brother != nullptr) { brotherInvincible = brother->invincible; brother->invincible = true; }
    player.invincible = true;
    // The original LEVEL consumes this flag in its next Boss probability roll.
    // Remaining spawns must still die: resetting the spawner strands kill quotas.
    *m_level.VariableResolver(5) = 1;
    if (m_effects != nullptr) { m_effects->SetPaused(true); }
    int elapsed = 0;
    unsigned defeated = 0;
    while (elapsed < GameCheats::BossSkipLimitMs && m_level.GetBossIntroSerial() == introSerial && !m_level.IsCleared()) {
        for (auto &actor : m_scene.enemies) {
            CEnemy &enemy = actor->model.enemy;
            if (!actor->mapPlaced && enemy.CanReceiveProjectile(0, kPlayerCombatId)) {
                enemy.Damage(enemy.combat.health);
                ++defeated;
            }
        }
        // Retire skipped projectiles/audio before each tick. The last tick's
        // real Boss spawn cues survive, so its authored entrance plays normally.
        if (m_effects != nullptr) { m_effects->Clear(); }
        Update(GameCheats::BossSkipStepMs, 0, 0, false);
        elapsed += GameCheats::BossSkipStepMs;
    }
    player.invincible = playerInvincible;
    if (brother != nullptr) { brother->invincible = brotherInvincible; }
    if (m_effects != nullptr) { m_effects->SetPaused(false); }
    *m_level.VariableResolver(5) = 0;
    const bool success = m_level.GetBossIntroSerial() != introSerial;
    const auto wallMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    std::printf("[stboss] started=%d defeated=%u simulated-ms=%d wall-ms=%lld wave=%d state=%d\n",
        success, defeated, elapsed, static_cast<long long>(wallMs), m_level.GetWave(), m_level.GetStateId());
    return success;
}
