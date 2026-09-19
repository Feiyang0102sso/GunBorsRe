/** Test-only launch policy, frame injection and capture orchestration. */
#include "gameplay/SurvivalCheckScenario.h"
#include "gun_bros_re/debug/Capture.h"

void SurvivalCheckScenario::Configure(const CGame::Launch &launch, Options &options) {
    m_capturePath = m_development.screenshotPath;
    options.realtime = m_capturePath.empty();
    options.mouseAim = m_capturePath.empty();
    options.showCollisions = m_development.showCollisions;
    options.exitOnCompletion = !m_development.check && m_capturePath.empty();
    options.seedLevel =
        !m_development.check && !m_development.bossStudy && !m_development.performanceStudy && m_capturePath.empty();
    if (m_development.deathmatchCheck || !m_capturePath.empty()) { options.matchSeed = 42 + launch.matchIndex; }
    // DM checks fix both the Bot stream and the independent LEVEL script stream.
    if (m_development.deathmatchCheck) {
        options.seedLevel = true;
        options.levelSeed = *options.matchSeed;
    }
}

int SurvivalCheckScenario::OnFrame(FramePhase phase, Frame &frame) {
    if (m_frameDriver != nullptr) {
        const int result = m_frameDriver->OnFrame(phase, frame);
        if (result >= 0 || result == -2) { return result; }
    }
    if (phase == FramePhase::BeforeSimulation && m_development.firePreview) { frame.forceFire = true; }
    if (m_development.performanceStudy) {
        const int result = m_performance.OnFrame(phase, frame);
        if (result >= 0) { return result; }
    }
    if (phase == FramePhase::Drawn && !m_capturePath.empty()) {
        auto &state = *m_state;
        if (!CGame::SaveProgress(state.launch.gameContext, state.progress, state.session.GetLevel(),
                                 state.accountedXplodium, m_finalizeProgress)) {
            return 1;
        }
        const unsigned errors = glGetError();
        if (errors != 0 || !Capture::SaveFrame(state.window, m_capturePath)) { return 1; }
        std::printf("[survival] wave=%d alive=%d spawned=%u kills=%u hp=%.1f\n", state.session.GetLevel().GetWave(),
                    state.session.CountEnemies(), state.scene.GetSpawnCount(), state.session.GetKills(),
                    state.vitals.health);
        state.window.Present();
        if (state.runtimeFailures != 0) { return 1; }
        return AfterCapture(state);
    }
    return -1;
}
