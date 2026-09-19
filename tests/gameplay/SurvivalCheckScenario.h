#pragma once
/** @file SurvivalCheckScenario.h
 * @brief Binds the permanent checks to a survival session.
 *
 * The session loop knows only ZGameObserver. Everything that decides
 * which check runs — and every assertion those checks make — lives on this
 * side of the boundary.
 */
#include "gameplay/SurvivalFixtures.h"
#include "gameplay/SurvivalDevelopment.h"
#include "TestOutput.h"
#include "research/PerformanceSession.h"

constexpr int kScenarioContinue = -1;

/** Routes each session hook to the check the development record selected. */
class SurvivalCheckScenario : public ZGameObserver {
public:
    explicit SurvivalCheckScenario(const SurvivalDevelopment &development, ZGameObserver *frameDriver = nullptr);
    void Configure(const CGame::Launch &launch, Options &options) override;
    int OnFrame(FramePhase phase, Frame &frame) override;

    int OnResources(ZGameObserver::Resources resources) override;
    int OnInventory(CResTOCManager &toc, CProfileManager &profile) override;
    int OnStage(ZGameObserver::Stage phase, CGame::Session &state) override;

private:
    unsigned m_failures = 0;
    std::string m_capturePath;
    bool m_finalizeProgress = false;
    CGame::Session *m_state = nullptr;
    ZGameObserver *m_frameDriver = nullptr;
    PerformanceSession m_performance;
    int AfterCapture(CGame::Session &state);
    int OnRewards(SurvivalRewardsFixture fixture);
    int OnPowerupInventory(SurvivalPowerupInventoryFixture fixture);
    int OnSceneReady(SurvivalSceneFixture fixture);
    int OnBoss(SurvivalBossFixture fixture);
    int OnFeedback(SurvivalFeedbackFixture fixture);
    int OnLevelSounds(SurvivalLevelSoundsFixture fixture);
    int OnPropRoutes(SurvivalPropRoutesFixture fixture);
    int OnTriggerRoutes(SurvivalTriggerRoutesFixture fixture);
    int OnPlacedProps(SurvivalPlacedPropsFixture fixture);
    int OnBrotherPose(SurvivalBrotherPoseFixture fixture);
    int OnTutorial(SurvivalTutorialFixture fixture);
    int OnWaves(SurvivalWavesFixture fixture);
    int OnHorde(SurvivalHordeFixture fixture);
    int OnCampaign(SurvivalCampaignFixture fixture);
    int OnPowerupCapture(SurvivalPowerupCaptureFixture fixture);
    int OnLoopStarting(CLevel &scene, CBrother::Vitals &vitals);

private:
    const SurvivalDevelopment &m_development;
};

/** Attaches a development record and its scenario to one launch.
 * Declare it next to the record; both must outlive the session call. */
struct DevelopmentBinding {
    SurvivalCheckScenario scenario;
    DevelopmentBinding(CGame::Launch &launch, SurvivalDevelopment &development, ZGameObserver *frameDriver = nullptr)
        : scenario(development, frameDriver) {
        // Performance CSVs/screenshots use the same explicit case directory.
        if (development.outputDirectory.empty()) { development.outputDirectory = TestOutput::Path(""); }
        launch.observer = &scenario;
    }
};
