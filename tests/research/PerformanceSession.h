#pragma once
/** Repeatable CPU/crowd experiment; never linked into Game or Viewer. */
#include "gameplay/SurvivalDevelopment.h"
#include "gameplay/SurvivalPilot.h"
#include "gun_bros_re/gameplay/game/CGameRuntime.h"
#include <fstream>
#include <memory>

class PerformanceSession {
  public:
    explicit PerformanceSession(const SurvivalDevelopment &development) : development(development) {}
    ~PerformanceSession();
    int Start(CGame::Session &state);
    int OnFrame(ZGameObserver::FramePhase phase, ZGameObserver::Frame &frame);

  private:
    std::string EvidencePath(const std::string &name) const;
    const SurvivalDevelopment &development;
    CGame::Session *state = nullptr;
    std::unique_ptr<SurvivalPilot> performancePilot;
    std::ofstream performanceReport;
    std::vector<double> performanceCpu;
    unsigned performanceFrame = 0, performanceSteps = 0;
    std::size_t performancePeakAlive = 0;
    bool performancePassed = true;
    using Clock = std::chrono::steady_clock;
    Clock::time_point performanceStart, performanceUpdated, performanceGeometry, performanceWorld, performanceHud;
};
