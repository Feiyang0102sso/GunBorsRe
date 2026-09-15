#pragma once
#include "gun_bros_re/gameplay/SurvivalScenario.h"

int CheckSurvivalRewards(SurvivalRewardsFixture fixture);

int CheckSurvivalPowerupInventory(SurvivalPowerupInventoryFixture fixture);

int CheckSurvivalDeath(SurvivalDeathFixture fixture);
int CheckLocalLive(SurvivalDeathFixture fixture, SurvivalHud *hud);
int CheckLivePeerActions(SurvivalDeathFixture fixture, CResTOCManager &toc, PackTables &tables,
    PowerupScene &playerPowerups, PowerupScene &peerPowerups, CProfileManager &peerProfile);

int CheckSurvivalBoss(SurvivalBossFixture fixture);

int CheckSurvivalFeedback(SurvivalFeedbackFixture fixture);

int CheckSurvivalLevelSounds(SurvivalLevelSoundsFixture fixture);

int CheckSurvivalPropRoutes(SurvivalPropRoutesFixture fixture);

int CheckSurvivalTriggerRoutes(SurvivalTriggerRoutesFixture fixture);

int CheckSurvivalPlacedProps(SurvivalPlacedPropsFixture fixture);

int CheckSurvivalBrotherPose(SurvivalBrotherPoseFixture fixture);

int CheckSurvivalTutorial(SurvivalTutorialFixture fixture);

int CheckSurvivalWaves(SurvivalWavesFixture fixture);
int CheckLiveCheatProgress(SurvivalDeathFixture fixture, SurvivalHud &hud);
int CheckLivePolicies(SurvivalDeathFixture fixture, PowerupScene &powerups, CProfileManager &profile);

int CheckSurvivalHorde(SurvivalHordeFixture fixture);

int CheckSurvivalCampaign(SurvivalCampaignFixture fixture);

int CheckSurvivalPowerupCapture(SurvivalPowerupCaptureFixture fixture);
