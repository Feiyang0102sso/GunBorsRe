/** Real Live HUD, death Flow and wave progression must work together. */
#include "gameplay/SurvivalChecks.h"
#include "gun_bros_re/ui/SurvivalHud.h"
#include <chrono>

int CheckLiveCheatProgress(SurvivalDeathFixture fixture, SurvivalHud &hud) {
    auto &session = fixture.session;
    auto &scene = fixture.scene;
    session.SetOriginalHud(&hud);
    unsigned failures = 0;
    for (unsigned downPeer = 0; downPeer < 3; ++downPeer) {
        hud.ResetNotices();
        session.SetScriptRandomSeed(0xB07);
        session.Restart(fixture.startX, fixture.startY, fixture.startFacing);
        fixture.vitals.invincible = true;
        fixture.brother.vitals.invincible = true;
        bool killed = false;
        long long peakUpdateUs = 0;
        for (unsigned elapsed = 0; elapsed < 120000 && scene.GetClearedWaves() < 2; elapsed += 16) {
            if (!killed && elapsed >= 5000) {
                if (downPeer == 0) { killed = scene.Suicide(); }
                else if (downPeer == 1) { killed = scene.KillTestBot(); }
                else { killed = true; }
            }
            const auto updateStarted = std::chrono::steady_clock::now();
            session.Update(16, 0, 0, false);
            const auto updateUs = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - updateStarted).count();
            peakUpdateUs = std::max(peakUpdateUs, static_cast<long long>(updateUs));
            for (auto &actor : scene.enemies) {
                auto &enemy = actor->model.enemy;
                if (!actor->mapPlaced && enemy.CanReceiveProjectile(0, kPlayerCombatId)) {
                    enemy.Damage(enemy.combat.health);
                }
            }
        }
        std::printf("[live-cheat-progress] down-peer=%u triggered=%d waves=%u alive=%zu state=%d scale=%d\n",
            downPeer, killed, scene.GetClearedWaves(), scene.AliveCount(),
            session.GetLevel().GetStateId(), session.GetLevel().TransformWorldElapseMS(16));
        std::printf("[live-cheat-progress] wait=%u transition=%d local-live=%d player-coop=%d peer-coop=%d\n",
            hud.LiveWaveRemaining(), session.IsTransitioning(), scene.IsLocalLive(),
            fixture.player.weapon->brother.IsCooperative(), fixture.brotherModel.weapon->brother.IsCooperative());
        if (!killed || scene.GetClearedWaves() < 2) { ++failures; }
        std::printf("[live-cheat-progress] down-peer=%u peak-update-us=%lld\n", downPeer, peakUpdateUs);
    }
    hud.ResetNotices();
    session.Restart(fixture.startX, fixture.startY, fixture.startFacing);
    session.SetSuspended(true);
    const auto started = std::chrono::steady_clock::now();
    const bool skipped = session.SkipToBoss();
    const auto wallMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    std::printf("[live-cheat-progress] suspended-boss=%d wall-ms=%lld\n", skipped, static_cast<long long>(wallMs));
    if (skipped || wallMs > 250) { ++failures; }
    session.SetSuspended(false);
    if (!session.StartBossSkip() || session.StartBossSkip()) { return 1; }
    const unsigned introSerial = session.GetLevel().GetBossIntroSerial();
    unsigned batches = 0;
    long long peakBatchUs = 0;
    while (session.IsBossSkipActive()) {
        const auto batchStarted = std::chrono::steady_clock::now();
        session.AdvanceBossSkip();
        const auto batchUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - batchStarted).count();
        peakBatchUs = std::max(peakBatchUs, static_cast<long long>(batchUs));
        ++batches;
    }
    std::printf("[live-cheat-progress] boss-batches=%u peak-batch-us=%lld started=%d\n",
        batches, peakBatchUs, session.GetLevel().GetBossIntroSerial() != introSerial);
    if (batches < 2 || peakBatchUs > 250000 || session.GetLevel().GetBossIntroSerial() == introSerial) { ++failures; }
    // Hold the real world under a Live overlay with one accepted death still
    // pending. Its LEVEL callback must survive the next scene.Update clear.
    hud.ResetNotices();
    session.Restart(fixture.startX, fixture.startY, fixture.startFacing);
    fixture.vitals.invincible = true;
    fixture.brother.vitals.invincible = true;
    CombatEnemy *victim = nullptr;
    for (unsigned elapsed = 0; elapsed < 10000 && victim == nullptr; elapsed += 16) {
        session.Update(16, 0, 0, false);
        for (auto &actor : scene.enemies) {
            if (!actor->mapPlaced && actor->model.enemy.CanReceiveProjectile(0, kPlayerCombatId)) { victim = actor.get(); break; }
        }
    }
    if (victim == nullptr) { return 1; }
    hud.BeginLiveWave(scene.GetMultiplayerStatistics(0), scene.GetMultiplayerStatistics(1));
    const unsigned killsBeforeWait = session.GetKills();
    victim->model.enemy.Damage(victim->model.enemy.combat.health);
    session.Update(16, 0, 0, false);
    std::printf("[live-cheat-progress] wait-death-delivered=%d\n", session.GetKills() > killsBeforeWait);
    if (session.GetKills() <= killsBeforeWait) { ++failures; }
    hud.ResetNotices();
    session.SetOriginalHud(nullptr);
    session.Restart(fixture.startX, fixture.startY, fixture.startFacing);
    return failures;
}
