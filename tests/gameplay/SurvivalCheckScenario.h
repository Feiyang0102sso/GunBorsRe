#pragma once
/** @file SurvivalCheckScenario.h
 * @brief Binds the permanent checks to a survival session.
 *
 * The session loop knows only ISurvivalScenario. Everything that decides
 * which check runs — and every assertion those checks make — lives on this
 * side of the boundary.
 */
#include "gun_bros_re/gameplay/SurvivalScenario.h"
#include "gun_bros_re/debug/SurvivalDevelopment.h"

/** Routes each session hook to the check the development record selected. */
class SurvivalCheckScenario : public ISurvivalScenario {
public:
    explicit SurvivalCheckScenario(const SurvivalDevelopment &development);

    int OnRewards(SurvivalRewardsFixture fixture) override;
    int OnPowerupInventory(SurvivalPowerupInventoryFixture fixture) override;
    int OnSceneReady(SurvivalSceneFixture fixture) override;
    int OnBoss(SurvivalBossFixture fixture) override;
    int OnFeedback(SurvivalFeedbackFixture fixture) override;
    int OnLevelSounds(SurvivalLevelSoundsFixture fixture) override;
    int OnPropRoutes(SurvivalPropRoutesFixture fixture) override;
    int OnTriggerRoutes(SurvivalTriggerRoutesFixture fixture) override;
    int OnPlacedProps(SurvivalPlacedPropsFixture fixture) override;
    int OnBrotherPose(SurvivalBrotherPoseFixture fixture) override;
    int OnTutorial(SurvivalTutorialFixture fixture) override;
    int OnWaves(SurvivalWavesFixture fixture) override;
    int OnHorde(SurvivalHordeFixture fixture) override;
    int OnCampaign(SurvivalCampaignFixture fixture) override;
    int OnPowerupCapture(SurvivalPowerupCaptureFixture fixture) override;
    int OnLoopStarting(CombatScene &scene, PlayerVitals &vitals) override;

private:
    const SurvivalDevelopment &m_development;
};

/** Attaches a development record and its scenario to one launch.
 * Declare it next to the record; both must outlive the session call. */
struct DevelopmentBinding {
    SurvivalCheckScenario scenario;
    DevelopmentBinding(SurvivalLaunch &launch, const SurvivalDevelopment &development)
        : scenario(development) {
        launch.development = &development;
        launch.scenario = &scenario;
    }
};
