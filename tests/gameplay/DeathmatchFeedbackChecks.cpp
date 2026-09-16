/** Reproduce reported DM regressions through real BIG actors and rendering state. */
#define NOMINMAX
#include "gameplay/SurvivalChecks.h"
#include "gun_bros_re/gameplay/CGame.h"
#include "gun_bros_re/gameplay/ZPowerupScene.h"
#include "gun_bros_re/gameplay/CMPMatch.h"
#include "gun_bros_re/gameplay/ZMapWorldInternal.h"
#include "engine/core/CStringToKey.h"
#include <chrono>
#include "gun_bros_re/debug/PerformanceProbe.h"

int CheckDeathmatchFeedback(SurvivalDeathFixture fixture, CMPMatch &match, ZPowerupScene &powerups, CProfileManager &profile) {
    auto &scene = fixture.scene;
    auto &player = fixture.player;
    auto &bot = fixture.brother;
    auto &session = fixture.session;
    // CLevel::OnStart does not spawn either DM actor before the selector.
    if (player.weapon->brother.IsVisible() || fixture.brotherModel.weapon->brother.IsVisible()) {
        std::printf("[dm-entry] actor spawned before equipment confirmation\n");
        return 1;
    }
    const float waitingX = scene.GetPlayer().x, waitingY = scene.GetPlayer().y;
    if (waitingX != fixture.startX || waitingY != fixture.startY) {
        std::printf("[dm-entry-camera] moved to spawn before equipment confirmation\n");
        return 1;
    }
    const float waitingCameraX = fixture.loaded.map.GetCamera().GetX();
    const float waitingCameraY = fixture.loaded.map.GetCamera().GetY();
    ZCombatHit incoming;
    incoming.owner = kBrotherCombatId; incoming.damage = fixture.vitals.maximum * 10;
    for (unsigned elapsed = 0; elapsed < 12000; elapsed += 16) {
        session.Update(16, 1, 1, true);
        if (fixture.loaded.map.GetCamera().GetX() != waitingCameraX || fixture.loaded.map.GetCamera().GetY() != waitingCameraY) {
            std::printf("[dm-entry-camera] camera moved during initial equipment selection\n");
            return 1;
        }
        if (scene.ApplyHit(kPlayerCombatId, incoming) != ZHitResult::Ignored) { return 1; }
    }
    float targetX = 0, targetY = 0;
    if (scene.GetPlayer().x != waitingX || scene.GetPlayer().y != waitingY || fixture.vitals.dead ||
        match.Score(0) != 0 || match.Score(1) != 0 || scene.TouchesPickup(waitingX, waitingY) ||
        scene.GetBrotherTarget(kPlayerCombatId, targetX, targetY) || scene.Suicide()) { return 1; }
    if (!scene.RespawnDeathmatch(1, true)) { return 1; }
    for (unsigned elapsed = 0; elapsed < 4000; elapsed += 16) {
        session.Update(16, 0, 0, false);
        if (fixture.loaded.map.GetCamera().GetX() != waitingCameraX || fixture.loaded.map.GetCamera().GetY() != waitingCameraY) {
            std::printf("[dm-entry-camera] remote spawn moved the waiting camera\n");
            return 1;
        }
    }
    if (fixture.vitals.hits != 0 || !scene.IsMatchSpawnPending(0) || bot.GetTargetCount() != 0) { return 1; }
    const auto selectedGun = scene.MatchGun(0, 1);
    if (!scene.SelectMatchGun(0, 0, selectedGun) || player.weapon->brother.IsVisible()) { return 1; }
    if (!scene.RespawnDeathmatch(0, true) || scene.RespawnDeathmatch(0, true) ||
        !player.weapon->brother.IsVisible() || match.GetLife(0).serial != 0 ||
        scene.ActiveMatchGun(0).packHash != selectedGun.packHash || scene.ActiveMatchGun(0).localIndex != selectedGun.localIndex ||
        *player.weapon->brother.VariableResolver(3) != 3000) { return 1; }
    std::printf("[dm-entry] no-body-no-hit-no-target-before-confirm=1 selected-gun-and-respawn-export=1\n");
    const float entryDistance = std::hypot(scene.GetPlayer().x - waitingX, scene.GetPlayer().y - waitingY);
    const auto *entryPath = fixture.loaded.map.GetPathLayer(session.GetLevel().GetRespawnPathLayer());
    if (entryPath == nullptr) { return 1; }
    for (const auto &node : entryPath->GetNodes()) {
        if (!node.locked && std::hypot(node.x - waitingX, node.y - waitingY) > entryDistance + 0.01f) { return 1; }
    }
    for (unsigned elapsed = 0; elapsed < 1024; elapsed += 16) { session.Update(16, 0, 0, false); }
    const auto &entryCamera = fixture.loaded.map.GetCamera();
    if (entryCamera.GetMode() != 0 || (entryCamera.GetX() == waitingCameraX && entryCamera.GetY() == waitingCameraY)) {
        std::printf("[dm-entry-camera] camera did not follow after confirmation\n");
        return 1;
    }
    std::printf("[dm-entry-camera] held=%.1f,%.1f confirmed=%.1f,%.1f spawn=%.1f,%.1f\n",
        waitingCameraX, waitingCameraY, entryCamera.GetX(), entryCamera.GetY(), scene.GetPlayer().x, scene.GetPlayer().y);
    fixture.vitals.invincible = true;
    bot.vitals.invincible = true;
    unsigned failures = 0;
    // Each selector blocks only its owner's input; the world continues.
    scene.SetMatchShopping(0, true);
    const float shoppingX = scene.GetPlayer().x, shoppingY = scene.GetPlayer().y;
    const float botBeforeX = bot.x, botBeforeY = bot.y;
    for (unsigned elapsed = 0; elapsed < 4000; elapsed += 16) { session.Update(16, 1, 1, true); }
    if (scene.GetPlayer().x != shoppingX || scene.GetPlayer().y != shoppingY || (bot.x == botBeforeX && bot.y == botBeforeY)) { ++failures; }
    scene.SetMatchShopping(0, false);
    scene.SetMatchShopping(1, true);
    const float botShoppingX = bot.x, botShoppingY = bot.y;
    for (unsigned elapsed = 0; elapsed < 1000; elapsed += 16) { session.Update(16, 0, 0, false); }
    if (bot.x != botShoppingX || bot.y != botShoppingY) { ++failures; }
    scene.SetMatchShopping(1, false);
    std::printf("[dm-shop] independent-input-and-world failures=%u\n", failures);
    const float outsideDamage = fixture.vitals.incomingDamage;
    scene.SplashBrothers(scene.GetPlayer().x - 101, scene.GetPlayer().y, 100, 10, 0, 0);
    if (fixture.vitals.incomingDamage != outsideDamage) { ++failures; }
    // Decode an actual active barrel. Native 7 uses CanCollide; only native 10
    // directly visits both brothers, with its own authored damage and radius.
    for (auto &prop : fixture.loaded.props) {
        if (!prop.active || !prop.runtime || prop.sprite->interactiveKind != MapDetail::ZInteractivePropKind::Barrel) { continue; }
        CProp probe;
        probe.SetLevelContext(&session.GetLevel());
        probe.Bind(prop.sprite->data, &prop.sprite->durations);
        probe.Damage(probe.GetHealth(), 0);
        for (unsigned time = 0; time < 1000; time += 16) { probe.Update(16, false); }
        float playerDamage = 0;
        for (const auto &action : probe.TakeActions()) {
            if (action.kind != ZPropAction::Kind::Splash) { continue; }
            std::printf("[dm-barrel] resource=%08x:%u native=%u radius=%d damage=%d owner=%d force=%d\n",
                prop.sprite->resource.packHash, prop.sprite->resource.localIndex, action.playersOnly ? 10 : 7,
                action.radius, action.damage, action.damageOwner, action.force);
            if (action.playersOnly) { playerDamage += action.damage; }
        }
        const float originalX = scene.GetPlayer().x, originalY = scene.GetPlayer().y;
        const float originalBotX = bot.x, originalBotY = bot.y;
        scene.GetPlayer().x = prop.x + 30; scene.GetPlayer().y = prop.y;
        bot.x = prop.x - 30; bot.y = prop.y;
        const float beforePlayer = fixture.vitals.incomingDamage, beforeBot = bot.vitals.incomingDamage;
        scene.SetMatchShopping(0, true); scene.SetMatchShopping(1, true);
        ZCombatHit trigger;
        trigger.owner = kPlayerCombatId; trigger.projectile = 123;
        trigger.damage = prop.runtime->GetHealth(); trigger.applyArmorAttack = false;
        const auto contact = scene.Trace(trigger, prop.x - 150, prop.y, 300, 0, 1, {kBrotherCombatId});
        if (contact.target == 0 || scene.ApplyHit(contact.target, trigger) == ZHitResult::Ignored) { ++failures; }
        for (unsigned time = 0; time < 1000; time += 16) { session.Update(16, 0, 0, false); }
        const float receivedPlayer = fixture.vitals.incomingDamage - beforePlayer;
        const float receivedBot = bot.vitals.incomingDamage - beforeBot;
        const float expectedPlayer = playerDamage * (2 - PlayerArmorMultiplier(player, 0)) / CFriendPowerManager::Multiplier(player.friendCount, 1);
        const float expectedBot = playerDamage * (2 - PlayerArmorMultiplier(fixture.brotherModel, 0)) / CFriendPowerManager::Multiplier(fixture.brotherModel.friendCount, 1);
        if (std::abs(receivedPlayer - expectedPlayer) > 0.01f || std::abs(receivedBot - expectedBot) > 0.01f) { ++failures; }
        std::printf("[dm-barrel] received=%.2f,%.2f expected=%.2f,%.2f failures=%u\n", receivedPlayer, receivedBot, expectedPlayer, expectedBot, failures);
        scene.GetPlayer().x = originalX; scene.GetPlayer().y = originalY;
        bot.x = originalBotX; bot.y = originalBotY;
        scene.SetMatchShopping(0, false); scene.SetMatchShopping(1, false);
        break;
    }
    double maximumStepMs = 0;
    PerformanceProbe::enabled = true;
    for (unsigned slot = 0; slot < 2; ++slot) {
        if (slot == 1 && !scene.RequestMatchWeaponSwap(0)) { ++failures; }
        for (unsigned elapsed = 0; elapsed < 3000; elapsed += 16) {
            const auto start = std::chrono::steady_clock::now();
            PerformanceProbe::counters = {};
            session.Update(16, 0, 0, false);
            const double stepMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            if (stepMs > maximumStepMs && stepMs > 30) {
                const auto &timing = PerformanceProbe::counters;
                std::printf("[dm-timing] slot=%u time=%u total=%.2f bot=%.2f path=%.2f effects=%.2f enemies=%.2f\n", slot, elapsed, stepMs,
                    timing.brotherMs, timing.pathSearchMs, timing.effectsMs, timing.enemyMs);
            }
            maximumStepMs = std::max(maximumStepMs, stepMs);
            if (FindPlayerTorsoPart(player) == nullptr || FindPlayerTorsoPart(fixture.brotherModel) == nullptr) {
                std::printf("[dm-feedback] missing torso slot=%u time=%u\n", slot, elapsed);
                ++failures;
                break;
            }
        }
    }
    // Cross several mesh banks, including every temporary supply weapon.
    // This catches a newly reserved bank preceding the outgoing torso in the map.
    for (unsigned item = 0; item < match.Data().pickups.size(); ++item) {
        if (!scene.CollectMatchWeapon(0, item)) { ++failures; }
        for (unsigned elapsed = 0; elapsed < 400; elapsed += 16) {
            session.Update(16, 0, 0, false);
            if (FindPlayerTorsoPart(player) == nullptr) { ++failures; break; }
        }
    }
    // Successful health use and the grenade release event must each notify the HUD.
    // The command log checks one message per use; rejected retries stay silent.
    GameObjectRef health;
    health.packHash = CStringToKey("pack5"); health.localIndex = 1;
    fixture.vitals.health = fixture.vitals.maximum / 2;
    profile.AddPowerup(health, 2);
    if (!powerups.SelectResource(health) || !powerups.Use()) { ++failures; }
    const auto *healthEntry = powerups.GetSelected();
    if (healthEntry == nullptr || powerups.Cooldowns().at(health.localIndex) != healthEntry->data.field124 * 1000) { ++failures; }
    // Health packs have zero configured cooldown; full health rejects a retry.
    fixture.vitals.health = fixture.vitals.maximum;
    if (powerups.Use()) { ++failures; }
    session.Update(16, 0, 0, false);
    GameObjectRef grenade;
    grenade.packHash = health.packHash; grenade.localIndex = 13;
    profile.AddPowerup(grenade, 2);
    const unsigned grenadeCount = profile.GetPowerupCount(grenade);
    if (!powerups.SelectResource(grenade) || !powerups.Use()) { ++failures; }
    for (unsigned elapsed = 0; elapsed < 1000; elapsed += 16) { session.Update(16, 0, 0, false); }
    if (profile.GetPowerupCount(grenade) != grenadeCount - 1 || powerups.Cooldowns().at(grenade.localIndex) <= 0 || powerups.Use()) { ++failures; }
    std::printf("[dm-presentation] health-grenade-uses=2 retries-rejected=1 failures=%u\n", failures);
    GameObjectRef turret;
    turret.packHash = CStringToKey("pack5"); turret.localIndex = 19;
    profile.AddPowerup(turret, 2);
    if (!powerups.SelectResource(turret) || !powerups.Use()) { ++failures; }
    ZCombatId turretId = 0;
    for (unsigned elapsed = 0; elapsed < 5000; elapsed += 16) {
        session.Update(16, 0, 0, false);
        for (const auto &actor : scene.enemies) {
            const auto &state = actor->model.enemy.combat;
            if (!state.turret || state.removed) { continue; }
            turretId = state.id;
            if (state.targetId == kPlayerCombatId) {
                std::printf("[dm-feedback] turret targets its owner id=%llu\n", state.id);
                ++failures;
                elapsed = 5000;
                break;
            }
        }
    }
    if (turretId == 0) { std::printf("[dm-feedback] turret never spawned\n"); ++failures; }
    fixture.vitals.invincible = false;
    ZCombatHit turretShot;
    turretShot.owner = turretId; turretShot.ownerType = 1; turretShot.damage = 10;
    const float ownerHealth = fixture.vitals.health;
    if (scene.ApplyHit(kPlayerCombatId, turretShot) != ZHitResult::Ignored || fixture.vitals.health != ownerHealth) { ++failures; }
    scene.Suicide();
    for (unsigned elapsed = 0; elapsed < 1500; elapsed += 16) { session.Update(16, 0, 0, false); }
    // Retail PLAYER DM death completes the burst without retaining body meshes.
    if (player.weapon->brother.IsVisible()) {
        std::printf("[dm-feedback] corpse remains torso=%d legs=%d\n", player.weapon->brother.GetTorso().GetMoveIndex(), player.weapon->brother.GetLegs().GetMoveIndex());
        ++failures;
    }
    // Resume uses the same native respawn export before the full timer expires.
    const float opponentX = bot.x, opponentY = bot.y;
    if (!scene.RespawnDeathmatch(0, false, true) || !player.weapon->brother.IsVisible() || fixture.vitals.dead ||
        *player.weapon->brother.VariableResolver(3) != 3000 || match.GetLife(0).serial != 1) { ++failures; }
    const auto *respawnPath = fixture.loaded.map.GetPathLayer(session.GetLevel().GetRespawnPathLayer());
    if (respawnPath == nullptr) { return 1; }
    const float respawnDistance = std::hypot(scene.GetPlayer().x - opponentX, scene.GetPlayer().y - opponentY);
    bool atUnlockedNode = false;
    for (const auto &node : respawnPath->GetNodes()) {
        if (node.locked) { continue; }
        if (node.x == scene.GetPlayer().x && node.y == scene.GetPlayer().y) { atUnlockedNode = true; }
        if (std::hypot(node.x - opponentX, node.y - opponentY) > respawnDistance + 0.01f) { ++failures; }
    }
    if (!atUnlockedNode) { ++failures; }
    std::printf("[dm-spawn] initial-saved-node-and-respawn-farthest-node failures=%u\n", failures);
    bool respawnEffect = false;
    for (const auto &cue : player.weapon->brother.TakeCues()) {
        if (cue.resource.packHash == CStringToKey("pack0_core") && cue.resource.localIndex == 8) { respawnEffect = true; }
    }
    if (!respawnEffect) { std::printf("[dm-feedback] missing respawn effect\n"); ++failures; }
    if (player.weapon->brother.ReceiveDamage(10) != ZHitResult::Ignored) { ++failures; }
    fixture.brotherModel.weapon->brother.StartDeath();
    for (unsigned elapsed = 0; elapsed < 1500; elapsed += 16) { session.Update(16, 0, 0, false); }
    scene.UpdatePeerIndicator(16, bot.x + 1000, bot.y + 1000, 100, 100);
    const auto *indicator = scene.PeerIndicator();
    if (indicator == nullptr || indicator->type != 4 + fixture.brotherModel.brotherIndex) { ++failures; }
    if (!scene.RespawnDeathmatch(1, false, true)) { ++failures; }
    std::printf("[dm-indicator] dead-peer-uses-avatar-not-help failures=%u\n", failures);
    // Match result stops combat immediately, but the host waits for the BIG fade.
    // Exercise the final lethal hit: surrender alone cannot expose a cut-off death export.
    while (match.Score(0) + 1 < match.Data().killLimit) {
        if (!match.Kill(1, 0) || !match.Respawn(1, true)) { return 1; }
    }
    // Keep the final burst in the captured camera, independent of respawn distance.
    scene.GetPlayer().x = bot.x + 80; scene.GetPlayer().y = bot.y;
    fixture.brotherModel.weapon->brother.StartDeath();
    session.Update(16, 0, 0, false);
    if (!session.IsFinished() || session.IsReadyForResults()) { ++failures; }
    const float frozenX = scene.GetPlayer().x, frozenY = scene.GetPlayer().y;
    const auto frozenShots = fixture.effects.GetShotCount();
    unsigned wrapUpMs = 0;
    unsigned burstCompleteMs = 0, fadeStartedMs = 0;
    bool capturedBurst = false;
    while (!session.IsReadyForResults() && wrapUpMs < 10000) {
        session.Update(16, 1, 1, true);
        wrapUpMs += 16;
        const bool burstActive = fixture.effects.HasActorBurst(kBrotherCombatId);
        if (burstActive && wrapUpMs >= 160 && !capturedBurst) {
            bool CaptureRescueEffect(SurvivalDeathFixture &, const char *);
            if (!CaptureRescueEffect(fixture, "deathmatch-final-burst.png")) { return 1; }
            capturedBurst = true;
        }
        if (bot.vitals.deathAnimationComplete && !burstActive && burstCompleteMs == 0) { burstCompleteMs = wrapUpMs; }
        if (session.IsDeathmatchFading() && fadeStartedMs == 0) {
            fadeStartedMs = wrapUpMs;
            // Keep the requested 800 ms hold within one simulation frame in either direction.
            const unsigned holdMs = fadeStartedMs - burstCompleteMs;
            if (burstCompleteMs == 0 || holdMs < 784 || holdMs > 816) { ++failures; }
        }
        if (scene.GetPlayer().x != frozenX || scene.GetPlayer().y != frozenY || fixture.effects.GetShotCount() != frozenShots) { ++failures; break; }
    }
    if (!session.IsReadyForResults() || wrapUpMs <= 16 || powerups.Use()) { ++failures; }
    if (!capturedBurst || fadeStartedMs == 0) { ++failures; }
    std::printf("[dm-final-kill] burst-complete=%u fade-start=%u result=%u\n", burstCompleteMs, fadeStartedMs, wrapUpMs);
    if (!bot.vitals.deathAnimationComplete || fixture.brotherModel.weapon->brother.IsVisible()) {
        std::printf("[dm-final-kill] results cut off death animation complete=%d visible=%d\n",
            bot.vitals.deathAnimationComplete, fixture.brotherModel.weapon->brother.IsVisible());
        ++failures;
    }
    std::printf("[dm-presentation] frozen-wrap-up=%u ms failures=%u\n", wrapUpMs, failures);
    if (maximumStepMs > 100) { std::printf("[dm-feedback] simulation stall\n"); ++failures; }
    std::printf("[dm-feedback] max-step-ms=%.3f failures=%u\n", maximumStepMs, failures);
    PerformanceProbe::enabled = false;
    if (failures != 0) { return 1; }
    return 0;
}
