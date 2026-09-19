// Desktop-only fast-forward through the original survival Flow callbacks.
#include "gun_bros_re/cheats/CheatConfig.h"
#include "gun_bros_re/gameplay/game/CGame.h"
#include <chrono>
#include <cstdio>

bool CGame::StartBossSkip() {
    CBrother::Vitals &player = m_level.GetPlayerVitals();
    if (m_bossSkipActive || m_suspended || m_match != nullptr || m_level.IsRescuePending() ||
        m_archive || m_horde || m_level.GetTutorialStep() >= 0 || player.dead ||
        m_level.IsCleared() || m_level.IsPaused() ||
        m_level.IsPowerupMovieActive()) {
        std::printf("[stboss] unavailable in current mode or presentation\n");
        return false;
    }
    if (m_level.HasLargeEnemyHealthBars()) {
        std::printf("[stboss] boss already active\n");
        return false;
    }
    m_bossSkipStarted = std::chrono::steady_clock::now();
    m_bossSkipIntroSerial = m_level.GetBossIntroSerial();
    m_bossSkipElapsedMs = 0;
    m_bossSkipDefeated = 0;
    m_bossSkipPreviousFlag = *m_level.VariableResolver(5);
    m_bossSkipActive = true;
    // The original LEVEL consumes this flag in its next Boss probability roll.
    // Remaining spawns must still die: resetting the spawner strands kill quotas.
    *m_level.VariableResolver(5) = 1;
    std::printf("[stboss] fast-forward requested\n");
    return true;
}

void CGame::AdvanceBossSkip() {
    if (!m_bossSkipActive) { return; }
    CBrother::Vitals &player = m_level.GetPlayerVitals();
    if (m_suspended || player.dead || m_level.IsRescuePending() || m_level.IsPaused() ||
        m_level.IsPowerupMovieActive()) {
        FinishBossSkip();
        return;
    }
    const auto frameStarted = std::chrono::steady_clock::now();
    const bool playerInvincible = player.invincible;
    CBrother::Vitals *brother = m_level.GetBrotherVitals();
    bool brotherInvincible = false;
    if (brother != nullptr) { brotherInvincible = brother->invincible; brother->invincible = true; }
    player.invincible = true;
    m_level.SetPaused(true);
    // Return to the host event/render loop after a small batch. The wall bound
    // also ends unreachable skips without monopolizing input for ten minutes.
    for (int step = 0; step < GameCheats::BossSkipFrameSteps; ++step) {
        const auto now = std::chrono::steady_clock::now();
        const auto wallMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_bossSkipStarted).count();
        if (m_bossSkipElapsedMs >= GameCheats::BossSkipLimitMs || wallMs >= GameCheats::BossSkipWallLimitMs ||
            m_level.GetBossIntroSerial() != m_bossSkipIntroSerial || m_level.IsCleared()) {
            FinishBossSkip();
            break;
        }
        if (step > 0 && now - frameStarted >= std::chrono::milliseconds(GameCheats::BossSkipFrameBudgetMs)) { break; }
        for (auto &actor : m_level.GetEnemies()) {
            CEnemy &enemy = *actor;
            if (!actor->mapPlaced && enemy.CanReceiveProjectile(0, Collision::Player)) {
                enemy.Damage(enemy.combat.health);
                ++m_bossSkipDefeated;
            }
        }
        // Retire skipped projectiles/audio before each tick. The last tick's
        // real Boss spawn cues survive, so its authored entrance plays normally.
        m_level.Clear();
        Update(GameCheats::BossSkipStepMs, 0, 0, false);
        m_bossSkipElapsedMs += GameCheats::BossSkipStepMs;
    }
    player.invincible = playerInvincible;
    if (brother != nullptr) { brother->invincible = brotherInvincible; }
    m_level.SetPaused(false);
}

void CGame::FinishBossSkip() {
    *m_level.VariableResolver(5) = m_bossSkipPreviousFlag;
    m_bossSkipActive = false;
    const bool success = m_level.GetBossIntroSerial() != m_bossSkipIntroSerial;
    const auto wallMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_bossSkipStarted).count();
    std::printf("[stboss] started=%d defeated=%u simulated-ms=%d wall-ms=%lld wave=%d state=%d\n",
        success, m_bossSkipDefeated, m_bossSkipElapsedMs, static_cast<long long>(wallMs), m_level.GetWave(), m_level.GetStateId());
}

bool CGame::SkipToBoss() {
    // Synchronous driver for permanent research checks; interactive cheats
    // call StartBossSkip and let each host frame call AdvanceBossSkip.
    if (!StartBossSkip()) { return false; }
    while (m_bossSkipActive) { AdvanceBossSkip(); }
    return m_level.GetBossIntroSerial() != m_bossSkipIntroSerial;
}
