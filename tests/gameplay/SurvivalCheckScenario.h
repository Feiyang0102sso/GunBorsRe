#pragma once
/** @file SurvivalCheckScenario.h
 * @brief Binds the permanent checks to a survival session.
 *
 * The session loop knows only ISurvivalScenario. Everything that decides
 * which check runs — and every assertion those checks make — lives on this
 * side of the boundary.
 */
#include "gameplay/SurvivalFixtures.h"
#include "gun_bros_re/debug/SurvivalDevelopment.h"
#include "TestOutput.h"

/** Routes each session hook to the check the development record selected. */
class SurvivalCheckScenario : public ZSurvivalScenario {
public:
    explicit SurvivalCheckScenario(const SurvivalDevelopment &development);

    int OnResources(ZSurvivalResources resources) override;
    int OnInventory(CResTOCManager &toc, CProfileManager &profile) override;
    int OnStage(ZSurvivalPhase phase, ZSurvivalState &state) override;

private:
    unsigned m_failures = 0;
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
    int OnLoopStarting(CLevel &scene, ZPlayerVitals &vitals);

private:
    const SurvivalDevelopment &m_development;
};

/** Attaches a development record and its scenario to one launch.
 * Declare it next to the record; both must outlive the session call. */
struct DevelopmentBinding {
    SurvivalCheckScenario scenario;
    DevelopmentBinding(CGame::Launch &launch, SurvivalDevelopment &development)
        : scenario(development) {
        // Performance CSVs/screenshots use the same explicit case directory.
        if (development.outputDirectory.empty()) { development.outputDirectory = TestOutput::Path(""); }
        launch.development = &development;
        launch.scenario = &scenario;
    }
};
