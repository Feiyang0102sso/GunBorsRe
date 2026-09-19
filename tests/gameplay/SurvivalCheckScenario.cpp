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
#include "TestOutput.h"

// Declared where they are defined, in the deathmatch check translation units.
int CheckDeathmatchCombat(SurvivalDeathFixture, CMPMatch &, CPowerUpSelector &, CProfileManager &, CGameFlow &);
int CheckDeathmatchFeedback(SurvivalDeathFixture, CMPMatch &, CPowerUpSelector &, CProfileManager &);
int CheckDeathmatchBotDifficulty(SurvivalDeathFixture, CResTOCManager &, ZPackTables &, CMPMatch &, CPowerUpSelector &,
                                 CProfileManager &);

SurvivalCheckScenario::SurvivalCheckScenario(const SurvivalDevelopment &development, ZGameObserver *frameDriver)
    : m_frameDriver(frameDriver), m_performance(development), m_development(development) {}

int SurvivalCheckScenario::OnRewards(SurvivalRewardsFixture fixture) {
    return CheckSurvivalRewards(fixture);
}

int SurvivalCheckScenario::OnPowerupInventory(SurvivalPowerupInventoryFixture fixture) {
    return CheckSurvivalPowerupInventory(fixture);
}

int SurvivalCheckScenario::OnSceneReady(SurvivalSceneFixture scene) {
    // The death fixture is the common view every scene-stage check wants.
    SurvivalDeathFixture fixture{scene.checkFailures, scene.packShortName, false,         scene.vitals,
                                 scene.window,        scene.program,       scene.batch,   scene.loaded,
                                 scene.player,        scene.scene,         scene.brother, scene.brotherModel,
                                 scene.session,       scene.startX,        scene.startY,  scene.startFacing};

    if (m_development.deathmatchCheck) {
        if (m_development.deathmatchFeedbackCheck) {
            const int feedbackResult =
                CheckDeathmatchFeedback(fixture, scene.match, scene.powerups, scene.gameContext->profile);
            if (feedbackResult != 0) { return feedbackResult; }
            return CheckDeathmatchBotDifficulty(fixture, scene.toc, scene.tables, scene.match, scene.peerPowerups,
                                                *scene.peerProfile);
        }
        return CheckDeathmatchCombat(fixture, scene.match, scene.peerPowerups, *scene.peerProfile, *scene.gameContext);
    }
    if (m_development.debugMapProfileCheck) {
        if (scene.gameContext == nullptr) { return 1; }
        return CheckDebugMapProfile(scene.tables, scene.player, scene.progress, *scene.gameContext, scene.scene,
                                    scene.session.GetLevel());
    }
    if (m_development.campaignDoorCheck) { return CheckCampaignDoorPassage(scene.loaded, scene.scene, scene.session); }
    if (m_development.campaignTargetCheck) { return CheckCampaignTargets(scene.loaded, scene.scene, scene.session); }
    if (m_development.campaignProgressionCheck) {
        return CheckCampaignProgression(scene.loaded, scene.scene, scene.session, scene.mapIndex);
    }
    if (m_development.campaignRescueCheck) { return CheckCampaignRescue(scene.loaded, scene.scene, scene.session); }
    if (m_development.campaignPortalCheck) { return CheckCampaignPortal(scene.loaded, scene.scene, scene.session); }
    if (m_development.campaignCacheCheck) { return CheckCampaignCache(scene.loaded, scene.scene, scene.session); }
    if (m_development.localLiveCheck) {
        if (scene.localLive && CheckLiveCheatProgress(fixture, scene.survivalHud) != 0) { return 1; }
        if (scene.localLive && CheckLivePolicies(fixture, scene.peerPowerups, *scene.peerProfile) != 0) { return 1; }
        if (CheckLocalLive(fixture, &scene.survivalHud) != 0) { return 1; }
        scene.session.Restart(scene.startX, scene.startY, scene.startFacing);
        return CheckLivePeerActions(fixture, scene.toc, scene.tables, scene.powerups, scene.peerPowerups,
                                    *scene.peerProfile);
    }

    // The death check reads deathStudy itself and returns -1 when it does not apply.
    SurvivalDeathFixture deathFixture = fixture;
    deathFixture.deathStudy = scene.deathStudy;
    return CheckSurvivalDeath(deathFixture);
}

int SurvivalCheckScenario::OnBoss(SurvivalBossFixture fixture) {
    return CheckSurvivalBoss(fixture);
}
int SurvivalCheckScenario::OnFeedback(SurvivalFeedbackFixture fixture) {
    return CheckSurvivalFeedback(fixture);
}
int SurvivalCheckScenario::OnLevelSounds(SurvivalLevelSoundsFixture fixture) {
    return CheckSurvivalLevelSounds(fixture);
}
int SurvivalCheckScenario::OnPropRoutes(SurvivalPropRoutesFixture fixture) {
    return CheckSurvivalPropRoutes(fixture);
}
int SurvivalCheckScenario::OnTriggerRoutes(SurvivalTriggerRoutesFixture fixture) {
    return CheckSurvivalTriggerRoutes(fixture);
}
int SurvivalCheckScenario::OnPlacedProps(SurvivalPlacedPropsFixture fixture) {
    return CheckSurvivalPlacedProps(fixture);
}
int SurvivalCheckScenario::OnBrotherPose(SurvivalBrotherPoseFixture fixture) {
    return CheckSurvivalBrotherPose(fixture);
}
int SurvivalCheckScenario::OnTutorial(SurvivalTutorialFixture fixture) {
    return CheckSurvivalTutorial(fixture);
}
int SurvivalCheckScenario::OnWaves(SurvivalWavesFixture fixture) {
    return CheckSurvivalWaves(fixture);
}
int SurvivalCheckScenario::OnHorde(SurvivalHordeFixture fixture) {
    return CheckSurvivalHorde(fixture);
}
int SurvivalCheckScenario::OnCampaign(SurvivalCampaignFixture fixture) {
    return CheckSurvivalCampaign(fixture);
}
int SurvivalCheckScenario::OnPowerupCapture(SurvivalPowerupCaptureFixture fixture) {
    return CheckSurvivalPowerupCapture(fixture);
}

int SurvivalCheckScenario::OnLoopStarting(CLevel &scene, ZPlayerVitals &vitals) {
    if (!m_development.flockCheck) { return kScenarioContinue; }
    vitals.invincible = true;
    return CheckFlockMovement(scene);
}

int SurvivalCheckScenario::OnResources(ZGameObserver::Resources resources) {
    m_failures = 0;
    return OnRewards({m_failures, m_development.check, resources.toc, resources.tables, resources.enemies,
                      resources.vitals, resources.progressData, resources.window, resources.survivalHud,
                      resources.program, resources.loaded, resources.player, resources.scene});
}

int SurvivalCheckScenario::OnInventory(CResTOCManager &toc, CProfileManager &profile) {
    return OnPowerupInventory({m_development.powerupStudy, toc, profile});
}

int SurvivalCheckScenario::OnStage(ZGameObserver::Stage phase, CGame::Session &state) {
    m_state = &state;
    if (phase == ZGameObserver::Stage::Bound) {
        // Regression: a local default partner must never inherit premium gear.
        if (m_development.check && state.withBrother) {
            const auto &gun = state.brotherConfiguration.guns[0];
            const unsigned coreHash = state.toc.GetPack(state.toc.GetCorePackIndex())->GetPackHash();
            bool defaultEquipment = gun.packHash == coreHash && gun.localIndex == 0;
            for (unsigned slot = 0; slot < 3; ++slot) {
                if (state.brotherModel.armor[slot] == nullptr || state.brotherModel.GetArmorMultiplier(slot) != 1.0f) {
                    defaultEquipment = false;
                }
            }
            std::printf("[brother-equipment-check] gun=%08x:%u default=%d\n", gun.packHash, gun.localIndex,
                        defaultEquipment);
            if (!defaultEquipment) { return 1; }
            if (!state.scene.SwapBrotherWeapon() || state.scene.GetBrotherWeaponSlot() != 1 ||
                !state.scene.SwapBrotherWeapon() || state.scene.GetBrotherWeaponSlot() != 0) {
                return 1;
            }
            std::printf("[brother-equipment-check] pistol-rifle-pistol=1 player-unchanged=1\n");
        }
        return OnSceneReady({m_failures,
                             state.launch.packShortName,
                             state.launch.mapIndex,
                             state.launch.localLive,
                             m_development.deathStudy,
                             state.toc,
                             state.tables,
                             state.vitals,
                             state.window,
                             state.program,
                             state.batch,
                             state.loaded,
                             state.player,
                             state.scene,
                             state.brother,
                             state.brotherModel,
                             state.session,
                             state.survivalHud,
                             state.progress,
                             state.match,
                             state.powerups,
                             state.peerPowerups,
                             state.peerProfile,
                             state.launch.gameContext,
                             state.startX,
                             state.startY,
                             state.startFacing});
    }
    if (phase == ZGameObserver::Stage::Ready) {
        int result = -1;
        result = OnBoss({m_failures, state.launch.packShortName, m_development.bossStudy, state.toc, state.tables,
                         state.enemies, state.vitals, state.window, state.program, state.loaded, state.player,
                         state.scene, state.session, state.startX, state.startY, state.startFacing});
        if (result >= 0) { return result; }
        result =
            OnFeedback({m_failures, m_capturePath, m_development.feedbackStudy, state.toc, state.tables, state.weapons,
                        state.enemies, state.vitals, state.survivalHud, state.program, state.loaded, state.player,
                        state.scene, state.session, state.props, state.startX, state.startY, state.startFacing});
        if (result >= 0) { return result; }
        result = OnLevelSounds({m_failures, m_development.check, state.session, state.scene});
        if (result >= 0) { return result; }
        result = OnPropRoutes({m_failures, m_development.check, state.loaded, state.scene});
        if (result >= 0) { return result; }
        result = OnTriggerRoutes({m_failures, m_development.check, state.session, state.loaded, state.scene,
                                  state.startX, state.startY, state.startFacing});
        if (result >= 0) { return result; }
        result = OnPlacedProps({m_development.check, state.loaded});
        if (result >= 0) { return result; }
        result = OnBrotherPose(
            {m_failures, m_development.check, state.withBrother, state.scene, state.brother, state.brotherModel});
        if (result >= 0) { return result; }
        result = OnTutorial({m_failures,
                             m_capturePath,
                             m_development.check,
                             state.launch.gameContext,
                             state.tables,
                             state.weapons,
                             state.vitals,
                             state.progress,
                             state.program,
                             state.loaded,
                             state.player,
                             state.weaponSlot,
                             state.equippedWeaponSlot,
                             state.scene,
                             state.brother,
                             state.session,
                             state.powerups,
                             state.pickupProfile,
                             state.tutorial,
                             state.accountedXplodium});
        if (result >= 0) { return result; }
        result = OnWaves({m_failures,
                          m_capturePath,
                          state.launch.packShortName,
                          state.launch.mapIndex,
                          m_development.check,
                          m_development.checkWaves,
                          state.launch.startWave,
                          state.launch.gameContext,
                          state.withBrother,
                          m_development.powerupStudy,
                          state.launch.archiveMission,
                          state.toc,
                          state.tables,
                          state.weapons,
                          state.enemies,
                          state.vitals,
                          state.progress,
                          state.window,
                          state.program,
                          state.loaded,
                          state.player,
                          state.weaponSlot,
                          state.scene,
                          state.brother,
                          state.brotherModel,
                          state.session,
                          state.props,
                          state.tutorial,
                          state.startX,
                          state.startY,
                          state.startFacing,
                          state.packIndex,
                          state.archiveLevel});
        if (result >= 0) { return result; }
        result = OnHorde({m_failures, m_capturePath, m_development.check, state.launch.startWave, state.vitals,
                          state.loaded, state.scene, state.session, state.horde});
        if (result >= 0) { return result; }
        result = OnCampaign({m_failures, m_capturePath, state.launch.packShortName, state.launch.mapIndex,
                             m_development.check, state.launch.archiveMission, state.vitals, state.loaded, state.scene,
                             state.session, state.horde});
        if (result >= 0) { return result; }
        m_finalizeProgress = m_development.check && state.horde;
        for (unsigned elapsed = 0; elapsed < m_development.advanceMs; elapsed += 16) {
            state.session.Update(16, 0, 0, m_development.firePreview);
            state.loaded.UpdateLayers(16);
        }
        result = OnPowerupCapture({m_capturePath, m_development.powerupStudy, state.scene, state.powerups});
        if (result >= 0) { return result; }
        state.options.realtime = m_capturePath.empty();
        state.options.mouseAim = m_capturePath.empty();
        state.options.exitOnCompletion = !m_development.check && m_capturePath.empty();
    }
    if (phase == ZGameObserver::Stage::LoopStarting) {
        const int result = OnLoopStarting(state.scene, state.vitals);
        if (result >= 0) { return result; }
        if (m_development.performanceStudy) { return m_performance.Start(state); }
    }
    if (phase == ZGameObserver::Stage::WorldDrawn) {
        if (m_development.check) {
            GLint sourceBlend = 0, destinationBlend = 0;
            glGetIntegerv(GL_BLEND_SRC, &sourceBlend);
            glGetIntegerv(GL_BLEND_DST, &destinationBlend);
            if (sourceBlend != GL_SRC_ALPHA || destinationBlend != GL_ONE_MINUS_SRC_ALPHA) { ++m_failures; }
            std::printf("[render-check] after-particles blend=%x/%x failures=%u\n", sourceBlend, destinationBlend,
                        m_failures);
        }
    }
    return kScenarioContinue;
}

int SurvivalCheckScenario::AfterCapture(CGame::Session &state) {
    if (m_development.check && state.horde) {
        CRefinementManager::Template refinement;
        if (!LoadRefinementTemplate(state.toc, state.tables, refinement)) { return 1; }
        CProfileManager hordeProfile;
        hordeProfile.Reset(state.toc.GetPack(state.toc.GetCorePackIndex())->GetPackHash(), refinement);
        CGameFlow record{hordeProfile, TestOutput::Path("horde-progress-check.dat")};
        record.hordeStart = static_cast<int>(state.launch.startWave);
        std::uint64_t credited = 0;
        if (!CGame::SaveProgress(&record, state.progress, state.session.GetLevel(), credited) ||
            !CGame::SaveProgress(&record, state.progress, state.session.GetLevel(), credited)) {
            return 1;
        }
        CProfileManager restored = hordeProfile;
        if (!restored.LoadFromDisk(record.savePath) ||
            restored.hordeBestScore[state.launch.startWave] != state.scene.GetScore() ||
            restored.hordeBestKills[state.launch.startWave] != state.scene.GetTotalKills() ||
            restored.clearedWaves[0] != 0 || restored.enemyKills[0] != 0) {
            ++m_failures;
        }
        const unsigned points = state.scene.GetScore();
        ZCombatHit damage;
        damage.ownerType = 1;
        damage.damage = 1;
        state.scene.ApplyHit(kPlayerCombatId, damage);
        if (state.scene.GetKillStreak() != 0 || state.scene.GetScore() != points) { ++m_failures; }
        state.session.Restart(state.startX, state.startY, state.startFacing);
        if (state.scene.GetScore() != 0 || state.scene.GetKillStreak() != 0 || state.session.GetKills() != 0 ||
            state.session.GetLevel().GetStopwatchTime() != 0 || state.session.GetLevel().GetObjectTimeScale() != 1) {
            ++m_failures;
        }
        std::printf("[horde-check] points=%u saved=1 damage-resets-streak=1 restart=1 failures=%u\n", points,
                    m_failures);
    }
    if (m_failures != 0) { return 1; }
    return 0;
}
