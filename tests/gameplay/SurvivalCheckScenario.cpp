/** @file SurvivalCheckScenario.cpp
 * @brief Dispatch for the permanent survival checks.
 *
 * This is the half of the old in-loop `#if GB_ENABLE_TESTS` blocks that had to
 * leave src/: the selection logic. The checks themselves are unchanged.
 */
#include "gameplay/SurvivalCheckScenario.h"
#include "gameplay/SurvivalChecks.h"
#include "gameplay/CampaignDoorChecks.h"
#include "gameplay/DebugMapChecks.h"
#include "gameplay/SurvivalStudy.h"

// Declared where they are defined, in the deathmatch check translation units.
int CheckDeathmatchCombat(SurvivalDeathFixture, CMPMatch &, PickupScene &, PowerupScene &,
    CProfileManager &, SurvivalGameContext &);
int CheckDeathmatchFeedback(SurvivalDeathFixture, CMPMatch &, PowerupScene &, CProfileManager &);
int CheckDeathmatchBotDifficulty(SurvivalDeathFixture, CResTOCManager &, PackTables &,
    CMPMatch &, PowerupScene &, CProfileManager &);

SurvivalCheckScenario::SurvivalCheckScenario(const SurvivalDevelopment &development)
    : m_development(development) {}

int SurvivalCheckScenario::OnRewards(SurvivalRewardsFixture fixture) {
    return CheckSurvivalRewards(fixture);
}

int SurvivalCheckScenario::OnPowerupInventory(SurvivalPowerupInventoryFixture fixture) {
    return CheckSurvivalPowerupInventory(fixture);
}

int SurvivalCheckScenario::OnSceneReady(SurvivalSceneFixture scene) {
    // The death fixture is the common view every scene-stage check wants.
    SurvivalDeathFixture fixture{scene.checkFailures, scene.packShortName, false, scene.vitals,
        scene.window, scene.program, scene.batch, scene.loaded, scene.player, scene.effects,
        scene.scene, scene.brother, scene.brotherModel, scene.session,
        scene.startX, scene.startY, scene.startFacing};

    if (m_development.deathmatchCheck) {
        if (m_development.deathmatchFeedbackCheck) {
            const int feedbackResult = CheckDeathmatchFeedback(fixture, scene.match, scene.powerups,
                scene.gameContext->profile);
            if (feedbackResult != 0) { return feedbackResult; }
            return CheckDeathmatchBotDifficulty(fixture, scene.toc, scene.tables, scene.match,
                scene.peerPowerups, *scene.peerProfile);
        }
        return CheckDeathmatchCombat(fixture, scene.match, scene.pickups, scene.peerPowerups,
            *scene.peerProfile, *scene.gameContext);
    }
    if (m_development.debugMapProfileCheck) {
        if (scene.gameContext == nullptr) { return 1; }
        return CheckDebugMapProfile(scene.tables, scene.player, scene.progress, *scene.gameContext,
            scene.scene, scene.session.GetLevel());
    }
    if (m_development.campaignDoorCheck) { return CheckCampaignDoorPassage(scene.loaded, scene.scene, scene.session); }
    if (m_development.campaignTargetCheck) { return CheckCampaignTargets(scene.loaded, scene.scene, scene.session); }
    if (m_development.campaignProgressionCheck) { return CheckCampaignProgression(scene.loaded, scene.scene, scene.session, scene.mapIndex); }
    if (m_development.campaignRescueCheck) { return CheckCampaignRescue(scene.loaded, scene.scene, scene.session); }
    if (m_development.campaignPortalCheck) { return CheckCampaignPortal(scene.loaded, scene.scene, scene.session); }
    if (m_development.campaignCacheCheck) { return CheckCampaignCache(scene.loaded, scene.scene, scene.session, scene.pickups); }
    if (m_development.localLiveCheck) {
        if (scene.localLive && CheckLiveCheatProgress(fixture, scene.survivalHud) != 0) { return 1; }
        if (scene.localLive && CheckLivePolicies(fixture, scene.peerPowerups, *scene.peerProfile) != 0) { return 1; }
        if (CheckLocalLive(fixture, &scene.survivalHud) != 0) { return 1; }
        scene.session.Restart(scene.startX, scene.startY, scene.startFacing);
        return CheckLivePeerActions(fixture, scene.toc, scene.tables, scene.powerups,
            scene.peerPowerups, *scene.peerProfile);
    }

    // The death check reads deathStudy itself and returns -1 when it does not apply.
    SurvivalDeathFixture deathFixture = fixture;
    deathFixture.deathStudy = scene.deathStudy;
    return CheckSurvivalDeath(deathFixture);
}

int SurvivalCheckScenario::OnBoss(SurvivalBossFixture fixture) { return CheckSurvivalBoss(fixture); }
int SurvivalCheckScenario::OnFeedback(SurvivalFeedbackFixture fixture) { return CheckSurvivalFeedback(fixture); }
int SurvivalCheckScenario::OnLevelSounds(SurvivalLevelSoundsFixture fixture) { return CheckSurvivalLevelSounds(fixture); }
int SurvivalCheckScenario::OnPropRoutes(SurvivalPropRoutesFixture fixture) { return CheckSurvivalPropRoutes(fixture); }
int SurvivalCheckScenario::OnTriggerRoutes(SurvivalTriggerRoutesFixture fixture) { return CheckSurvivalTriggerRoutes(fixture); }
int SurvivalCheckScenario::OnPlacedProps(SurvivalPlacedPropsFixture fixture) { return CheckSurvivalPlacedProps(fixture); }
int SurvivalCheckScenario::OnBrotherPose(SurvivalBrotherPoseFixture fixture) { return CheckSurvivalBrotherPose(fixture); }
int SurvivalCheckScenario::OnTutorial(SurvivalTutorialFixture fixture) { return CheckSurvivalTutorial(fixture); }
int SurvivalCheckScenario::OnWaves(SurvivalWavesFixture fixture) { return CheckSurvivalWaves(fixture); }
int SurvivalCheckScenario::OnHorde(SurvivalHordeFixture fixture) { return CheckSurvivalHorde(fixture); }
int SurvivalCheckScenario::OnCampaign(SurvivalCampaignFixture fixture) { return CheckSurvivalCampaign(fixture); }
int SurvivalCheckScenario::OnPowerupCapture(SurvivalPowerupCaptureFixture fixture) { return CheckSurvivalPowerupCapture(fixture); }

int SurvivalCheckScenario::OnLoopStarting(CombatScene &scene, PlayerVitals &vitals) {
    if (!m_development.flockCheck) { return kScenarioContinue; }
    vitals.invincible = true;
    return CheckFlockMovement(scene);
}
