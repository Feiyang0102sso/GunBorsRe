#pragma once
#include "gameplay/SurvivalFixtures.h"

int CheckSurvivalRewards(SurvivalRewardsFixture fixture);

int CheckSurvivalPowerupInventory(SurvivalPowerupInventoryFixture fixture);

int CheckSurvivalDeath(SurvivalDeathFixture fixture);
int CheckLocalLive(SurvivalDeathFixture fixture, CInputPad *hud);
int CheckLivePeerActions(SurvivalDeathFixture fixture, CResTOCManager &toc, ZPackTables &tables,
    ZPowerupScene &playerPowerups, ZPowerupScene &peerPowerups, CProfileManager &peerProfile);

int CheckSurvivalBoss(SurvivalBossFixture fixture);

int CheckSurvivalFeedback(SurvivalFeedbackFixture fixture);

int CheckSurvivalLevelSounds(SurvivalLevelSoundsFixture fixture);

int CheckSurvivalPropRoutes(SurvivalPropRoutesFixture fixture);

int CheckSurvivalTriggerRoutes(SurvivalTriggerRoutesFixture fixture);

int CheckSurvivalPlacedProps(SurvivalPlacedPropsFixture fixture);

int CheckSurvivalBrotherPose(SurvivalBrotherPoseFixture fixture);

int CheckSurvivalTutorial(SurvivalTutorialFixture fixture);

int CheckSurvivalWaves(SurvivalWavesFixture fixture);
int CheckLiveCheatProgress(SurvivalDeathFixture fixture, CInputPad &hud);
int CheckLivePolicies(SurvivalDeathFixture fixture, ZPowerupScene &powerups, CProfileManager &profile);

int CheckSurvivalHorde(SurvivalHordeFixture fixture);

int CheckSurvivalCampaign(SurvivalCampaignFixture fixture);

int CheckSurvivalPowerupCapture(SurvivalPowerupCaptureFixture fixture);

unsigned CheckLevelSounds(CLevel &level, ZWeaponEffects &effects);
unsigned CheckTriggerRoutes(CGame &session, CMap &map, CLevel &scene, float startX, float startY, float startFacing);

namespace MapDetail {
// Exercise the real SDL event queue and CWindow recognizer, including held S.
bool PushBossCheckKey(ZWindow &window, char letter, bool repeat = false, bool checkMovement = false);

unsigned CheckPropEntryRoutes(const ZLoadedMap &map, const CLevel &scene);
unsigned CheckPropDamageContracts(const ZLoadedMap &map);
}
