/** CGame session host implementation; original ownership follows game.cpp.
 * SDL/GL submission and borrowed desktop resources are host adaptations.
 */
#include "gun_bros_re/gameplay/game/CGameRuntime.h"
#include "gun_bros_re/host/ZGameKeys.h"
#include "gun_bros_re/debug/SurvivalDebug.h"
using namespace MapDetail;

// Presentation reads a snapshot; its actions re-enter the same keyboard path.
ZInputPadState CGame::Session::BuildHudState() {
    ZInputPadState state;
    state.health = vitals.health;
    state.maximumHealth = vitals.maximum;
    state.brotherHealth = brother.vitals.health;
    state.brotherMaximumHealth = brother.vitals.maximum;
    state.withBrother = withBrother;
    state.wave = std::min(session.GetLevel().GetWave(), session.GetLevel().GetWaveLimit() - 1);
    state.horde = horde;
    state.score = scene.GetScore();
    state.killStreak = scene.GetKillStreak();
    state.stopwatchMs = session.GetLevel().GetStopwatchTime();
    state.bossIntroSerial = session.GetLevel().GetBossIntroSerial();
    state.xplodiumMultiplier =
        static_cast<int>(std::ceil(session.GetLevel().GetXplodiumMultiplierPercent() * player.GetArmorMultiplier(4) *
                                   CFriendPowerManager::Multiplier(player.friendCount, 6)));
    state.level = progress.GetLevel();
    state.experience = progress.GetExperienceInLevel();
    state.experienceDelta = progress.GetExperienceDelta();
    state.xplodium = scene.GetXplodium();
    state.kills = scene.GetTotalKills();
    state.enemies = session.CountEnemies();
    state.weaponSlot = equippedWeaponSlot;
    state.swapKeyDown = window.IsKeyDown(ZGameKeys::SwapWeapon);
    state.weapon = weapons[weaponSlot].name;
    if (gameContext != nullptr) {
        state.guns[0] = gameContext->profile.configuration.guns[0];
        state.guns[1] = gameContext->profile.configuration.guns[1];
    } else {
        state.guns[0].packHash = weapons[weaponSlot].packHash;
        state.guns[0].localIndex = static_cast<std::uint8_t>(weapons[weaponSlot].ordinal);
    }
    state.paused = paused;
    if (launch.deathmatch) {
        state.guns[0] = scene.MatchGun(0, 0);
        state.guns[1] = scene.MatchGun(0, 1);
    }
    state.shopOpen = shopOpen;
    state.localLive = launch.localLive;
    state.deathmatch = launch.deathmatch;
    state.remoteShop = shopOpen && (launch.localLive || launch.deathmatch) && liveShop.Owner() == 1;
    state.afterDeathShop = deathShop;
    state.shopRemainingMs = liveShop.Remaining(window.GetTicksMs());
    if (!liveShop.IsTimed()) { state.shopRemainingMs = 0; }
    state.brotherName = brotherName;
    state.itemChoice = itemChoice;
    state.leftPowerup = leftPowerup;
    state.rightPowerup = rightPowerup;
    state.leftCount = pickupProfile->GetPowerupCount(leftPowerup);
    state.rightCount = pickupProfile->GetPowerupCount(rightPowerup);
    state.powerupCooldowns = powerups.Cooldowns();
    state.inventory = pickupProfile->powerups;
    state.coins = pickupProfile->coins;
    state.warbucks = pickupProfile->warbucks;
    if (state.remoteShop) {
        state.inventory = peerProfile->powerups;
        state.coins = peerProfile->coins;
        state.warbucks = peerProfile->warbucks;
    }
    state.soundEnabled = pickupProfile->soundEnabled;
    state.musicEnabled = pickupProfile->musicEnabled;
    state.dockedSticks = pickupProfile->options.DockedSticks();
    state.powerupStatus.healthPercent = static_cast<int>(std::lround(vitals.health * 100 / vitals.maximum));
    state.powerupStatus.shield = player.IsShield();
    state.powerupStatus.frenzy = player.IsFrenzy();
    state.powerupStatus.autoFire = player.IsAutoFire();
    state.powerupStatus.turret = player.IsTurretActive();
    for (unsigned type = 0; type < 3; ++type) { state.powerupStatus.frenzyTypes[type] = player.IsFrenzyType(type); }
    state.dead = vitals.dead;
    state.inputHidden = vitals.inputHidden;
    if (launch.deathmatch) { state.inputHidden = false; }
    state.cleared = session.GetLevel().IsCleared();
    state.transitioning = session.IsTransitioning();
    state.transitionTime = session.GetTransitionElapsed();
    state.perfectBonus = scene.GetLastWaveBonus();
    state.damageHits = vitals.hits;
    PopulateSurvivalDebugInfo(state, scene, packShortName, mapIndex, showCollisions);
    state.dialog = session.GetDialogText();
    state.tutorialStep = session.GetLevel().GetTutorialStep();
    if (archiveMission != nullptr) { state.mission = archiveMission->title; }
    if (powerups.GetSelected() != nullptr) {
        state.item = powerups.GetSelected()->name;
        state.itemCount = powerups.GetCount();
    }
    PopulateDebugBuffs(state, player);
    return state;
}
