#include "research/PerformanceSession.h"
#include "research/FlockMetrics.h"
#include "gun_bros_re/debug/PerformanceProbe.h"
#include "gun_bros_re/debug/Capture.h"

std::string PerformanceSession::EvidencePath(const std::string &name) const {
    // Evidence paths come from the development record, not from tests/.
    std::filesystem::path directory(development.outputDirectory);
    std::filesystem::create_directories(directory);
    return (directory / name).string();
}
PerformanceSession::~PerformanceSession() {
    if (state == nullptr) { return; }
    PerformanceProbe::enabled = false;
    PerformanceProbe::uncachedPaths = false;
}
int PerformanceSession::Start(CGame::Session &sessionState) {
    state = &sessionState;
    auto &scene = state->scene;
    auto &brother = state->brother;
    auto &vitals = state->vitals;
    auto &window = state->window;
    auto &loaded = state->loaded;
    // Deterministic 1,200 rendered frames, with real waves, AI and projectiles.
    if (development.performanceSpawnStudy) {
        // User screenshot coordinates are a test input, not map resource data.
        scene.GetPlayer().x = 454.8f;
        scene.GetPlayer().y = 688.7f;
        brother.vitals.invincible = true;
        if (development.performanceFlockStudy) {
            scene.GetPlayer().x = 733.6f;
            scene.GetPlayer().y = 284.1f;
        }
    } else {
        performancePilot = std::make_unique<SurvivalPilot>(scene, loaded.GetVisibleBounds());
        if (!performancePilot) { return 1; }
    }
    PerformanceProbe::enabled = true;
    PerformanceProbe::uncachedPaths = development.performanceUncachedPaths;
    vitals.invincible = true;
    if (!window.SetVSync(false)) { return 1; }
    std::printf("[performance] vsync=0 update-step=16ms realtime=%d uncached-paths=%d target=1200\n",
                development.performanceRealtimeStudy, development.performanceUncachedPaths);
    performanceReport.open(EvidencePath("performance-frames.csv"));
    performanceReport << "frame,update_ms,geometry_ms,world_ms,hud_ms,present_ms,alive,spawned,spawn_ms,brother_ms,"
                         "navigation_ms,enemy_ms,effects_ms,new_spawns,brother_hp,player_x,player_y,path_ms,path_calls,"
                         "path_nodes,update_steps,flock_ms,nearest_mean,minimum_gap,close_pairs\n";
    return -1;
}

int PerformanceSession::OnFrame(ZGameObserver::FramePhase phase, ZGameObserver::Frame &frame) {
    // Begin precedes LoopStarting, which creates the experiment's runtime state.
    if (phase == ZGameObserver::FramePhase::Begin) { return -1; }
    auto &scene = state->scene;
    auto &brother = state->brother;
    auto &session = state->session;
    auto &window = state->window;
    using Phase = ZGameObserver::FramePhase;
    if (phase == Phase::Starting) {
        PerformanceProbe::counters = {};
        performanceStart = Clock::now();
    }
    if (phase == Phase::BeforeSimulation) {
        auto &accumulator = frame.accumulator;
        auto &moveX = frame.moveX;
        auto &moveY = frame.moveY;
        frame.forceFire = true;
        frame.suppressFire = development.performanceSpawnStudy;
        if (!development.performanceRealtimeStudy) { accumulator = 16; }
        if (performancePilot) { performancePilot->Update(16, moveX, moveY); }
        if (development.performanceSpawnStudy) {
            moveX = 0;
            moveY = 0;
            if (development.performanceFlockStudy) {
                // Repeatable square movement through the reported map.
                switch ((performanceFrame / 300) % 4) {
                case 0:
                    moveY = 1;
                    break;
                case 1:
                    moveX = -1;
                    break;
                case 2:
                    moveY = -1;
                    break;
                case 3:
                    moveX = 1;
                    break;
                }
            }
        }
    }
    if (phase == Phase::Updated) { performanceUpdated = Clock::now(); }
    if (phase == Phase::GeometryDrawn) { performanceGeometry = Clock::now(); }
    if (phase == Phase::WorldDrawn) { performanceWorld = Clock::now(); }
    if (phase == Phase::Drawn) {
        performanceHud = Clock::now();
        if (development.performanceFlockStudy && (performanceFrame + 1) % 300 == 0) {
            if (!Capture::SaveFrame(window, EvidencePath("flock-" + std::to_string(performanceFrame + 1) + ".png"))) {
                return 1;
            }
        }
    }
    if (phase == Phase::Presented) {
        const auto performanceEnd = std::chrono::steady_clock::now();
        const double updateMs =
            std::chrono::duration<double, std::milli>(performanceUpdated - performanceStart).count();
        const double geometryMs =
            std::chrono::duration<double, std::milli>(performanceGeometry - performanceUpdated).count();
        const double worldMs =
            std::chrono::duration<double, std::milli>(performanceWorld - performanceGeometry).count();
        const double hudMs = std::chrono::duration<double, std::milli>(performanceHud - performanceWorld).count();
        const double presentMs = std::chrono::duration<double, std::milli>(performanceEnd - performanceHud).count();
        performanceReport << performanceFrame << ',' << updateMs << ',' << geometryMs << ',' << worldMs << ',' << hudMs
                          << ',' << presentMs << ',' << scene.AliveCount() << ',' << scene.GetSpawnCount() << ','
                          << PerformanceProbe::counters.spawnMs << ',' << PerformanceProbe::counters.brotherMs << ','
                          << PerformanceProbe::counters.navigationMs << ',' << PerformanceProbe::counters.enemyMs << ','
                          << PerformanceProbe::counters.effectsMs << ',' << PerformanceProbe::counters.spawns << ','
                          << brother.vitals.health << ',' << scene.GetPlayer().x << ',' << scene.GetPlayer().y << ','
                          << PerformanceProbe::counters.pathSearchMs << ',' << PerformanceProbe::counters.pathSearches
                          << ',' << PerformanceProbe::counters.pathNodes << ',' << frame.updateSteps;
        const auto flockMetrics = MeasureFlock(scene);
        performanceReport << ',' << PerformanceProbe::counters.flockMs << ',' << flockMetrics.nearestMean << ','
                          << flockMetrics.minimum << ',' << flockMetrics.closePairs << '\n';
        performanceCpu.push_back(updateMs + geometryMs + worldMs + hudMs);
        performanceSteps += frame.updateSteps;
        performancePeakAlive = std::max(performancePeakAlive, scene.AliveCount());
        if (++performanceFrame % 300 == 0) {
            std::printf("[performance] frame=%u update=%.2f geometry=%.2f world=%.2f hud=%.2f present=%.2f alive=%zu\n",
                        performanceFrame, updateMs, geometryMs, worldMs, hudMs, presentMs, scene.AliveCount());
        }
        bool finished = performanceFrame >= 1200;
        if (development.performanceRealtimeStudy) { finished = performanceSteps >= 1200; }
        if (finished) {
            std::sort(performanceCpu.begin(), performanceCpu.end());
            const std::size_t count = performanceCpu.size();
            std::printf("[performance] cpu-p50=%.3f cpu-p95=%.3f cpu-p99=%.3f max=%.3f frames=%u kills=%u\n",
                        performanceCpu[count / 2], performanceCpu[count * 95 / 100], performanceCpu[count * 99 / 100],
                        performanceCpu.back(), performanceFrame, scene.GetTotalKills());
            std::printf("[performance] enemy-assets hits=%u misses=%u templates=%zu\n", scene.GetEnemyModelCache().hits,
                        scene.GetEnemyModelCache().misses, scene.GetEnemyModelCache().entries.size());
            if (development.performanceSpawnStudy) {
                // Host acceptance budget: 60 Hz CPU work, with the reported crowd present.
                const unsigned enemyLimit = session.GetLevel().GetEnemyLimit();
                performancePassed =
                    performancePeakAlive >= enemyLimit && performanceCpu[count * 95 / 100] <= 1000.0 / 60;
                std::printf(
                    "[spawn-performance] updates=%u peak-alive=%zu pool-limit=%u cpu-p95-budget=16.667 passed=%d\n",
                    performanceSteps, performancePeakAlive, enemyLimit, performancePassed);
            }
            if (!performancePassed) { return 1; }
            return state->Finish();
        }
    }
    return -1;
}
