#include "gun_bros_re/debug/Capture.h"
#include "gameplay/SurvivalChecks.h"
#include "TestOutput.h"
#include "gun_bros_re/gameplay/CMPMatch.h"
using namespace MapDetail;

/** Exercise mode changes with real BIG items and isolated inventory. */
static unsigned CheckPowerupModes(CResTOCManager &toc, ZPackTables &tables, ZPowerupScene &powerups,
    CProfileManager &profile, CLevel &scene) {
    std::vector<ZStoreEntry> stores;
    std::vector<ZPowerupEntry> catalog;
    if (!LoadStoreCatalog(toc, tables, stores) || !LoadPowerupCatalog(toc, tables, catalog)) { return 1; }
    CMPMatch match;
    unsigned failures = 0;
    const bool wasLocalLive = scene.IsLocalLive();
    for (unsigned mode = 0; mode < 3; ++mode) {
        unsigned allowedCount = 0, blockedCount = 0;
        for (const auto &store : stores) {
            const auto &item = store.data;
            if (item.type < 10 || item.type > 13 || item.objects.empty() || item.objects.front().type != 17) { continue; }
            const auto &reference = item.objects.front().object;
            const ZPowerupEntry *entry = nullptr;
            for (const auto &candidate : catalog) {
                if (candidate.resource.packHash == reference.packHash && candidate.resource.localIndex == reference.localIndex) { entry = &candidate; break; }
            }
            if (entry == nullptr) { ++failures; continue; }
            profile.AddPowerup(reference, 2);
            const auto stock = profile.GetPowerupCount(reference);
            const bool allowed = (item.excludedGameModes & (1u << mode)) == 0;
            if (!allowed) {
                // Select while allowed, then change mode: Use must recheck too.
                for (unsigned previousMode = 0; previousMode < 3; ++previousMode) {
                    if ((item.excludedGameModes & (1u << previousMode)) != 0) { continue; }
                    scene.SetLocalLive(previousMode == 1);
                    powerups.SetDeathmatch(nullptr);
                    if (previousMode == 2) { powerups.SetDeathmatch(&match); }
                    if (!powerups.SelectResource(reference)) { ++failures; }
                    break;
                }
            }
            scene.SetLocalLive(mode == 1);
            powerups.SetDeathmatch(nullptr);
            if (mode == 2) { powerups.SetDeathmatch(&match); }
            if (powerups.SelectResource(reference) != allowed) { ++failures; }
            CPowerup query;
            query.SetDeathmatch(mode == 2);
            query.Bind(entry->data);
            const bool equipable = allowed && query.Query(0);
            if (powerups.Equip(0, reference) != equipable) { ++failures; }
            if (allowed) { ++allowedCount; continue; }
            ++blockedCount;
            const unsigned consumed = powerups.consumed;
            if (powerups.Use() || powerups.Use(true) || powerups.consumed != consumed ||
                profile.GetPowerupCount(reference) != stock || powerups.IsMovieActive()) { ++failures; }
            // A saved forbidden ordinal must be replaced by the original default.
            for (unsigned slot = 0; slot < 2; ++slot) {
                profile.configuration.powerups[slot] = reference.localIndex;
                const auto replacement = powerups.GetEquipped(slot);
                const ZPowerupEntry *defaultEntry = nullptr;
                for (const auto &candidate : catalog) {
                    if (candidate.resource.packHash == replacement.packHash && candidate.resource.localIndex == replacement.localIndex) { defaultEntry = &candidate; break; }
                }
                if (defaultEntry == nullptr || !powerups.SelectResource(replacement)) { ++failures; continue; }
                query.Bind(defaultEntry->data);
                if (!query.Query(4, slot) || profile.configuration.powerups[slot] != replacement.localIndex) { ++failures; }
            }
        }
        // Cycling an owned catalogue must skip every mode-excluded entry.
        for (unsigned index = 0; index < catalog.size(); ++index) {
            powerups.Cycle();
            const auto *selected = powerups.GetSelected();
            if (selected == nullptr) { ++failures; continue; }
            for (const auto &store : stores) {
                const auto &item = store.data;
                if (item.type < 10 || item.type > 13 || item.objects.empty()) { continue; }
                const auto &reference = item.objects.front().object;
                if (reference.packHash == selected->resource.packHash && reference.localIndex == selected->resource.localIndex &&
                    (item.excludedGameModes & (1u << mode)) != 0) { ++failures; }
            }
        }
        std::printf("[powerup-mode-host] mode=%u allowed=%u blocked=%u stock-preserved=1 default-restored=1 failures=%u\n",
            mode, allowedCount, blockedCount, failures);
    }
    scene.SetLocalLive(wasLocalLive);
    powerups.SetDeathmatch(nullptr);
    return failures;
}

int CheckSurvivalWaves(SurvivalWavesFixture fixture) {
    auto archiveLevel = fixture.archiveLevel;
    auto & checkFailures = fixture.checkFailures;
    auto & capturePath = fixture.capturePath;
    auto & packShortName = fixture.packShortName;
    auto & mapIndex = fixture.mapIndex;
    auto & check = fixture.check;
    auto & checkWaves = fixture.checkWaves;
    auto & startWave = fixture.startWave;
    auto & gameContext = fixture.gameContext;
    auto & withBrother = fixture.withBrother;
    auto & powerupStudy = fixture.powerupStudy;
    auto & archiveMission = fixture.archiveMission;
    auto & toc = fixture.toc;
    auto & tables = fixture.tables;
    auto & weapons = fixture.weapons;
    auto & enemies = fixture.enemies;
    auto & vitals = fixture.vitals;
    auto & progress = fixture.progress;
    auto & window = fixture.window;
    auto & program = fixture.program;
    auto & loaded = fixture.loaded;
    auto & player = fixture.player;
    auto & weaponSlot = fixture.weaponSlot;
    auto & effects = fixture.effects;
    auto & scene = fixture.scene;
    auto & brother = fixture.brother;
    auto & brotherModel = fixture.brotherModel;
    auto & session = fixture.session;
    auto & pickups = fixture.pickups;
    auto & props = fixture.props;
    auto & tutorial = fixture.tutorial;
    auto & startX = fixture.startX;
    auto & startY = fixture.startY;
    auto & startFacing = fixture.startFacing;
    auto & packIndex = fixture.packIndex;

    if (check && archiveMission == nullptr && !tutorial) {
        checkFailures += CheckPropDamageContracts(loaded);
        // Original character animation, real bullet scripts and actual stock.
        // This account is isolated even when the full-menu check owns a profile.
        CProfileManager consumableProbe;
        CRefinementManager::Template consumableRefinement;
        if (!LoadRefinementTemplate(toc, tables, consumableRefinement)) { return 1; }
        consumableProbe.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), consumableRefinement);
        ZPowerupScene powerupProbe(toc, tables, player, vitals, scene, effects, consumableProbe);
        if (!powerupProbe.Init()) { return 1; }
        if (powerupStudy) {
            CProfileManager modeProfile;
            ZPowerupScene modeProbe(toc, tables, player, vitals, scene, effects, modeProfile);
            if (!modeProbe.Init()) { return 1; }
            checkFailures += CheckPowerupModes(toc, tables, modeProbe, modeProfile, scene);
        }
        GameObjectRef consumable;
        consumable.packHash = toc.GetPack(toc.GetPackIndexFromName("pack5"))->GetPackHash();
        for (unsigned index = 13; index <= 15; ++index) {
            session.Restart(startX, startY, startFacing);
            consumable.localIndex = static_cast<std::uint8_t>(index);
            consumableProbe.AddPowerup(consumable, 2);
            powerupProbe.Select(index);
            const std::size_t before = effects.GetShotCount();
            if (!powerupProbe.Use() || powerupProbe.Use() || consumableProbe.GetPowerupCount(consumable) != 2) { ++checkFailures; }
            for (int elapsed = 0; elapsed < 5000; elapsed += 16) {
                scene.Update(16, 0, 0, false);
                powerupProbe.Update(16);
            }
            if (effects.GetShotCount() != before + 1 || consumableProbe.GetPowerupCount(consumable) != 1) { ++checkFailures; }
            std::printf("[powerup-play-check] item=%u shots=%zu stock=%u state=%d failures=%u\n", index,
                effects.GetShotCount() - before, consumableProbe.GetPowerupCount(consumable), player.weapon->brother.GetStateId(), checkFailures);
            if (!powerupProbe.Use()) { ++checkFailures; }
            ZCombatHit cancel;
            cancel.ownerType = 1;
            cancel.damage = 10000;
            scene.ApplyHit(kPlayerCombatId, cancel);
            for (int elapsed = 0; elapsed < 1000; elapsed += 16) {
                scene.Update(16, 0, 0, false);
                powerupProbe.Update(16);
            }
            if (consumableProbe.GetPowerupCount(consumable) != 1 || effects.GetShotCount() != before + 1) { ++checkFailures; }
        }
        session.Restart(startX, startY, startFacing);
        consumable.localIndex = 1;
        consumableProbe.AddPowerup(consumable, 1);
        powerupProbe.Select(1);
        if (powerupProbe.Use()) { ++checkFailures; }
        vitals.health = 1;
        if (!powerupProbe.Use() || vitals.health != std::min(vitals.maximum, 9.0f) || powerupProbe.GetCount() != 0) { ++checkFailures; }
        checkFailures += powerupProbe.failures;
        std::printf("[powerup-play-check] healing/cancel/repeat consumed=%u failures=%u\n", powerupProbe.consumed, checkFailures);
        unsigned airstrikeCase = 0;
        for (unsigned airstrikeIndex : {0u, 10u, 11u, 0u, 10u, 11u}) {
            const bool fromSelector = airstrikeCase++ >= 3;
            session.Restart(startX, startY, startFacing);
            ZWeaponEffects airstrikeEffects(toc, tables, program);
            CLevel airstrikeScene(tables, program, enemies, player, vitals, airstrikeEffects, loaded.playerTemplate->gameScale);
            airstrikeScene.Reset();
            // Exercise the same session update as gameplay: movie-only tests
            // cannot detect actors continuing to move during an air strike.
            CGame airstrikeSession(airstrikeScene, loaded.map, enemies);
            if (!airstrikeSession.Load(toc, tables, toc.GetPack(packIndex)->GetPackHash(), mapIndex, archiveLevel, archiveMission != nullptr)) { return 1; }
            airstrikeSession.Restart(startX, startY, startFacing);
            ZCombatEnemy *target = airstrikeScene.Spawn(0, 700, 650);
            ZCombatEnemy *outside = airstrikeScene.Spawn(0, 4600, 650);
            if (target == nullptr || outside == nullptr) { return 1; }
            for (int elapsed = 0; elapsed < 1000; elapsed += 16) {
                target->model.enemy.Update(16);
                outside->model.enemy.Update(16);
            }
            target->model.enemy.TakeActions();
            outside->model.enemy.TakeActions();
            // Arena clamps initial spawns; explicitly place the radius probe
            // outside the blast only after its authored spawn state has matured.
            outside->model.enemy.combat.x = 600;
            outside->model.enemy.combat.y = 650;
            target->model.enemy.combat.health = 100000;
            target->model.enemy.combat.maxHealth = 100000;
            // Put the blast at a camera center far from the player. The target
            // must be hit there; the player's vicinity is outside every radius.
            airstrikeScene.SetViewCenter(5000, 650);
            target->model.enemy.combat.x = 5100;
            target->model.enemy.combat.y = 650;
            consumable.localIndex = static_cast<std::uint8_t>(airstrikeIndex);
            consumableProbe.AddPowerup(consumable, 2);
            ZPowerupScene airstrike(toc, tables, player, vitals, airstrikeScene, airstrikeEffects, consumableProbe);
            airstrikeSession.SetPowerups(&airstrike);
            if (!airstrike.Init() || !airstrike.Select(airstrikeIndex) || !airstrike.Use(fromSelector) || airstrike.Use() || airstrike.GetCount() != 1) { ++checkFailures; }
            if (target->model.enemy.combat.hitCount != 0) { ++checkFailures; }
            // Paused presentation has no elapsed time and must not finish a movie.
            for (unsigned repeat = 0; repeat < 10; ++repeat) { airstrike.Update(0); }
            if (airstrike.GetMoviePlayer().GetElapsed() != 0) { ++checkFailures; }
            unsigned splashTime = 0;
            unsigned movingFrames = 0;
            unsigned warningFrames = 0;
            unsigned closingFrames = 0;
            const float frozenX = airstrikeScene.GetPlayer().x;
            const float frozenY = airstrikeScene.GetPlayer().y;
            const float frozenEnemyX = target->model.enemy.combat.x;
            const float frozenEnemyY = target->model.enemy.combat.y;
            const float frozenHealth = vitals.health;
            // Haven's LEVEL also creates two map turrets. Freeze preserves
            // the starting count; it does not imply only our two probes exist.
            const std::size_t frozenEnemyCount = airstrikeScene.GetEnemies().size();
            for (int elapsed = 0; elapsed < 12000 && airstrike.IsMovieActive(); elapsed += 16) {
                airstrikeSession.Update(16, 1, 0, true);
                if (airstrike.GetMoviePlayer().IsForegroundMovie()) { ++warningFrames; }
                if (airstrike.GetMoviePlayer().IsSelectorFrameClosing()) {
                    ++closingFrames;
                    if (closingFrames == 4 && airstrikeIndex == 0) {
                        glClearColor(0.04f, 0.05f, 0.07f, 1);
                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                        if (!airstrike.DrawMovies() || !Capture::SaveFrame(window, TestOutput::Path("airstrike-frame-closing.png"))) { ++checkFailures; }
                    }
                }
                if (airstrikeScene.GetPlayer().x != frozenX || airstrikeScene.GetPlayer().y != frozenY || airstrikeEffects.GetShotCount() != 0) { ++movingFrames; }
                if (target->model.enemy.combat.x != frozenEnemyX || target->model.enemy.combat.y != frozenEnemyY ||
                    vitals.health != frozenHealth || airstrikeScene.GetEnemies().size() != frozenEnemyCount) { ++movingFrames; }
                if (airstrike.GetMoviePlayer().splashCount > 0 && splashTime == 0) { splashTime = elapsed + 16; }
                if (elapsed == 992) {
                    glClearColor(0.04f, 0.05f, 0.07f, 1);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    std::string suffix = "-equipped";
                    if (fromSelector) { suffix = "-selector"; }
                    if (!airstrike.DrawMovies() || !Capture::SaveFrame(window, TestOutput::Path("airstrike-check-") + std::to_string(airstrikeIndex) + suffix + ".png")) { ++checkFailures; }
                    if (fromSelector) {
                        // Sample the actual selector frame away from the title,
                        // character and thin moving streaks. A movie-active flag
                        // alone passed while the whole frame was missing.
                        std::vector<std::uint8_t> pixels(200 * 400 * 4);
                        glReadPixels(0, 640, 200, 400, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                        unsigned framePixels = 0;
                        for (std::size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
                            if (pixels[pixel + 1] > 40 && pixels[pixel + 2] > 50) { ++framePixels; }
                        }
                        if (framePixels < 300) { ++checkFailures; }
                        std::printf("[airstrike-frame-check] item=%u pixels=%u failures=%u\n", airstrikeIndex, framePixels, checkFailures);
                    }
                }
            }
            float expectedDamage = 240;
            if (movingFrames != 0) { ++checkFailures; }
            if (fromSelector && warningFrames == 0) { ++checkFailures; }
            if (fromSelector && closingFrames == 0) { ++checkFailures; }
            if (!fromSelector && closingFrames != 0) { ++checkFailures; }
            if (airstrike.GetMoviePlayer().HasSelectorFrame()) { ++checkFailures; }
            std::printf("[airstrike-frame-lifecycle] item=%u selector=%d closing-frames=%u failures=%u\n",
                airstrikeIndex, fromSelector, closingFrames, checkFailures);
            if (airstrikeSession.GetLevel().IsPaused()) { ++checkFailures; }
            std::printf("[airstrike-route-check] item=%u selector=%d warning-frames=%u\n", airstrikeIndex, fromSelector, warningFrames);
            std::printf("[airstrike-freeze-check] item=%u moving-frames=%u failures=%u\n", airstrikeIndex, movingFrames, checkFailures);
            if (airstrikeIndex == 10) { expectedDamage = 500; }
            if (airstrikeIndex == 11) { expectedDamage = 1600; }
            if (airstrike.IsMovieActive() || airstrike.GetMoviePlayer().splashCount != 1 || splashTime < 1200 ||
                std::abs(target->model.enemy.combat.totalDamage - expectedDamage) > 0.01f ||
                outside->model.enemy.combat.hitCount != 0 || airstrike.GetCount() != 1) { ++checkFailures; }
            std::printf("[airstrike-check] item=%u time=%u movies=%u effects=%u damage=%.2f expected=%.2f outside-hits=%d stock=%u failures=%u\n",
                airstrikeIndex, splashTime, airstrike.GetMoviePlayer().movieCompletions, airstrike.GetMoviePlayer().effectCount,
                target->model.enemy.combat.totalDamage, expectedDamage, outside->model.enemy.combat.hitCount, airstrike.GetCount(), checkFailures);
            // Release the original intro normally, then verify real input works
            // again. A cleared pause flag alone does not prove gameplay resumed.
            for (unsigned frame = 0; frame < 100; ++frame) { airstrikeSession.Update(16, 0, 0, false); }
            const float resumedX = airstrikeScene.GetPlayer().x;
            airstrikeSession.Update(16, 1, 0, true);
            if (airstrikeScene.GetPlayer().x == resumedX) { ++checkFailures; }
            std::printf("[airstrike-resume-check] item=%u selector=%d moved=%d failures=%u\n",
                airstrikeIndex, fromSelector, airstrikeScene.GetPlayer().x != resumedX, checkFailures);
            if (!airstrike.Use()) { ++checkFailures; }
            airstrike.Update(400);
            airstrike.Reset();
            for (int elapsed = 0; elapsed < 8000; elapsed += 16) { airstrike.Update(16); }
            if (airstrike.GetMoviePlayer().splashCount != 1 || airstrike.IsMovieActive() || airstrike.GetCount() != 0) { ++checkFailures; }
            checkFailures += airstrike.failures + airstrike.GetMoviePlayer().failures;
        }
        session.Restart(startX, startY, startFacing);
        consumable.localIndex = 5;
        consumableProbe.AddPowerup(consumable, 2);
        powerupProbe.Select(5);
        if (!powerupProbe.Use() || powerupProbe.Use() || !player.weapon->brother.IsShield() || powerupProbe.GetCount() != 1) { ++checkFailures; }
        const float shieldHealth = vitals.health;
        player.weapon->brother.ReceiveDamage(1);
        if (vitals.health != shieldHealth) { ++checkFailures; }
        // Equipment must not reset an actor's active shield timer.
        const int shieldMs = player.powerups.shieldMs;
        if (!EquipControlledPlayer(tables, loaded, program, weapons[weaponSlot]) || player.powerups.shieldMs != shieldMs) { ++checkFailures; }
        AdvancePlayer(player, shieldMs);
        if (player.weapon->brother.IsShield()) { ++checkFailures; }
        player.weapon->brother.ReceiveDamage(1);
        if (std::abs(vitals.health - (shieldHealth - 1)) > 0.001f) { ++checkFailures; }
        session.Restart(startX, startY, startFacing);
        {
            // Auto Aim: a real stationary enemy, ordinary rifle and actual
            // projectiles. No caller-supplied aim or automatic pilot firing.
            if (!EquipControlledPlayer(tables, loaded, program, weapons[0])) { return 1; }
            ZWeaponEffects aimEffects(toc, tables, program);
            CLevel aimScene(tables, program, enemies, player, vitals, aimEffects, loaded.playerTemplate->gameScale);
            aimScene.Reset();
            aimScene.GetPlayer().x = 600;
            aimScene.GetPlayer().y = 650;
            aimScene.GetPlayer().facing = 270;
            ZCombatEnemy *target = aimScene.Spawn(0, 700, 650);
            if (target == nullptr) { return 1; }
            for (int elapsed = 0; elapsed < 1000; elapsed += 16) { target->model.enemy.Update(16); }
            // Maturation can queue an enemy shot before this fixture begins.
            // Discard only those setup actions; measured scene updates remain real.
            target->model.enemy.TakeActions();
            target->model.enemy.combat.health = 100000;
            target->model.enemy.combat.maxHealth = 100000;
            target->model.enemy.stun.SetStunned(6000, 0, 0);
            consumable.localIndex = 12;
            consumableProbe.AddPowerup(consumable, 2);
            ZPowerupScene aimPowerup(toc, tables, player, vitals, aimScene, aimEffects, consumableProbe);
            if (!aimPowerup.Init() || !aimPowerup.Select(12) || !aimPowerup.Use() || aimPowerup.Use() ||
                aimPowerup.GetCount() != 1 || player.powerups.autoFireMs != 90000) { ++checkFailures; }
            std::printf("[autoaim-probe] use stock=%u timer=%d failures=%u\n", aimPowerup.GetCount(), player.powerups.autoFireMs, checkFailures);
            for (int elapsed = 0; elapsed < 800; elapsed += 16) { aimScene.Update(16, 0, 0, false); }
            if (aimEffects.GetShotCount() != 0 || aimScene.GetAutoAimTarget() != 0) { ++checkFailures; }
            std::printf("[autoaim-probe] idle shots=%zu target=%llu failures=%u\n", aimEffects.GetShotCount(),
                static_cast<unsigned long long>(aimScene.GetAutoAimTarget()), checkFailures);
            for (int elapsed = 0; elapsed < 1800; elapsed += 16) { aimScene.Update(16, 0, 0, true); }
            if (aimEffects.GetShotCount() == 0 || target->model.enemy.combat.hitCount == 0 ||
                aimScene.GetAutoAimTarget() == 0 || std::abs(aimScene.GetPlayer().facing - 90) > 5.1f) { ++checkFailures; }
            std::printf("[autoaim-probe] hold facing=%.2f target=%.1f,%.1f shots=%zu hits=%d failures=%u\n",
                aimScene.GetPlayer().facing, target->model.enemy.combat.x, target->model.enemy.combat.y,
                aimEffects.GetShotCount(), target->model.enemy.combat.hitCount, checkFailures);
            aimScene.Update(16, 0, 0, false);
            const auto releasedShots = aimEffects.GetShotCount();
            for (int elapsed = 0; elapsed < 320; elapsed += 16) { aimScene.Update(16, 0, 0, false); }
            if (aimEffects.GetShotCount() != releasedShots || aimScene.GetAutoAimTarget() != 0) { ++checkFailures; }
            std::printf("[autoaim-probe] release shots=%zu previous=%zu failures=%u\n", aimEffects.GetShotCount(), releasedShots, checkFailures);
            const int remainingMs = player.powerups.autoFireMs;
            if (!EquipControlledPlayer(tables, loaded, program, weapons[weaponSlot]) ||
                player.powerups.autoFireMs != remainingMs) { ++checkFailures; }
            AdvancePlayer(player, remainingMs - 1);
            if (!player.weapon->brother.IsAutoFire()) { ++checkFailures; }
            AdvancePlayer(player, 1);
            if (player.weapon->brother.IsAutoFire()) { ++checkFailures; }
            std::printf("[autoaim-play-check] shots=%zu hits=%d hold/release/swap/90sec/expiry failures=%u\n",
                releasedShots, target->model.enemy.combat.hitCount, checkFailures);
        }
        session.Restart(startX, startY, startFacing);
        {
            ZWeaponEffects turretEffects(toc, tables, program);
            CLevel turretScene(tables, program, enemies, player, vitals, turretEffects, loaded.playerTemplate->gameScale);
            turretScene.Reset();
            ZCombatEnemy *target = turretScene.Spawn(0, 600, 460);
            if (target == nullptr) { return 1; }
            for (int elapsed = 0; elapsed < 1000; elapsed += 16) { target->model.enemy.Update(16); }
            target->model.enemy.TakeActions();
            target->model.enemy.combat.x = 600;
            target->model.enemy.combat.y = 460;
            target->model.enemy.combat.health = 100000;
            target->model.enemy.combat.maxHealth = 100000;
            target->model.enemy.stun.SetStunned(60000, 0, 0);
            consumable.localIndex = 19;
            consumableProbe.AddPowerup(consumable, 2);
            ZPowerupScene turretPowerup(toc, tables, player, vitals, turretScene, turretEffects, consumableProbe);
            if (!turretPowerup.Init() || !turretPowerup.Select(19) || !turretPowerup.Use() ||
                turretPowerup.GetCount() != 2 || !player.weapon->brother.IsTurretActive() || turretPowerup.Use()) { ++checkFailures; }
            int firstActiveMs = -1, stoppedMs = -1;
            unsigned peakTurrets = 0;
            for (int elapsed = 0; elapsed < 40000; elapsed += 16) {
                turretScene.Update(16, 0, 0, false);
                turretPowerup.Update(16);
                unsigned liveTurrets = 0;
                for (const auto &actor : turretScene.GetEnemies()) {
                    if (actor->model.enemy.combat.turret && !actor->model.enemy.combat.removed) { ++liveTurrets; }
                }
                peakTurrets = std::max(peakTurrets, liveTurrets);
                if (player.weapon->brother.IsTurretActive() && liveTurrets == 1 && firstActiveMs < 0) {
                    firstActiveMs = elapsed + 16;
                    if (turretPowerup.Use() || turretPowerup.GetCount() != 1) { ++checkFailures; }
                }
                if (firstActiveMs >= 0 && !player.weapon->brother.IsTurretActive()) {
                    stoppedMs = elapsed + 16;
                    break;
                }
            }
            if (firstActiveMs < 0 || stoppedMs <= firstActiveMs || peakTurrets != 1 ||
                target->model.enemy.combat.hitCount == 0 || turretPowerup.GetCount() != 1 ||
                turretPowerup.consumed != 1 || turretPowerup.failures != 0) { ++checkFailures; }
            if (!turretPowerup.Use()) { ++checkFailures; }
            // Cancelled pre-throw requests release their reservation, without
            // consuming another item or replacing a live turret's state.
            if (!EquipControlledPlayer(tables, loaded, program, weapons[weaponSlot])) { return 1; }
            turretPowerup.Update(16);
            if (player.weapon->brother.IsTurretActive() || turretPowerup.GetCount() != 1) { ++checkFailures; }
            if (!turretPowerup.Use()) { ++checkFailures; }
            vitals.dead = true;
            turretPowerup.Update(16);
            if (player.weapon->brother.IsTurretActive() || turretPowerup.GetCount() != 1) { ++checkFailures; }
            std::printf("[turret-play-check] active=%d stopped=%d peak=%u shots=%zu hits=%d stock=%u failures=%u\n",
                firstActiveMs, stoppedMs, peakTurrets, turretEffects.GetShotCount(),
                target->model.enemy.combat.hitCount, turretPowerup.GetCount(), checkFailures);
        }
        session.Restart(startX, startY, startFacing);
        const unsigned boosts[] = {18, 17, 16};
        // These BIG items are DM-only; effect research must use that mode too.
        CMPMatch boostMatch;
        powerupProbe.SetDeathmatch(&boostMatch);
        for (unsigned type = 0; type < 3; ++type) {
            consumable.localIndex = static_cast<std::uint8_t>(boosts[type]);
            consumableProbe.AddPowerup(consumable, 2);
            powerupProbe.Select(boosts[type]);
            if (!powerupProbe.Use() || powerupProbe.Use() || !player.weapon->brother.IsFrenzyType(type)) { ++checkFailures; }
            float expected = 332 / 256.0f;
            if (type == 2) { expected = 1.5f; }
            if (std::abs(scene.GetProjectilePowerupMultiplier(kPlayerCombatId) - expected) > 0.001f) { ++checkFailures; }
        }
        const float beforeDefense = vitals.health;
        player.weapon->brother.ReceiveDamage(1);
        if (std::abs(vitals.health - (beforeDefense - 256.0f / 332)) > 0.001f) { ++checkFailures; }
        AdvancePlayer(player, 15000);
        powerupProbe.Update(15000); // Advance the DM cooldown with the actor timers.
        if (scene.GetProjectilePowerupMultiplier(kPlayerCombatId) != 1 || player.weapon->brother.IsFrenzyType(2)) { ++checkFailures; }
        powerupProbe.SetDeathmatch(nullptr);
        consumable.localIndex = 6;
        consumableProbe.AddPowerup(consumable, 2);
        powerupProbe.Select(6);
        if (!powerupProbe.Use() || powerupProbe.Use() || !player.weapon->brother.IsFrenzy() ||
            player.powerups.legacyFrenzyMs != 21000 || scene.GetProjectilePowerupMultiplier(kPlayerCombatId) != 1) { ++checkFailures; }
        if (!EquipControlledPlayer(tables, loaded, program, weapons[weaponSlot]) ||
            player.powerups.legacyFrenzyMs != 21000) { ++checkFailures; }
        AdvancePlayer(player, 20000);
        // Isolate the legacy stop-all callback across explicit mode contexts.
        powerupProbe.SetDeathmatch(&boostMatch);
        powerupProbe.Select(18);
        if (!powerupProbe.Use() || !player.weapon->brother.IsFrenzyType(0)) { ++checkFailures; }
        powerupProbe.SetDeathmatch(nullptr);
        AdvancePlayer(player, 1000);
        if (player.weapon->brother.IsFrenzy() || player.weapon->brother.IsFrenzyType(0) ||
            player.powerups.legacyFrenzyMultiplier[0] != 1) { ++checkFailures; }
        std::printf("[tantrum-check] duration=21000 duplicate-blocked=1 equipment-preserved=1 stop-all-boosts=1 failures=%u\n", checkFailures);
        GameObjectRef equippedLeft = consumable;
        GameObjectRef equippedRight = consumable;
        equippedLeft.localIndex = 14;
        equippedRight.localIndex = 5;
        if (!powerupProbe.Equip(0, equippedLeft) || !powerupProbe.Equip(1, equippedRight)) { ++checkFailures; }
        if (!consumableProbe.SaveToDisk(TestOutput::Path("powerup-profile-check.dat"))) { ++checkFailures; }
        CProfileManager restoredConsumables;
        restoredConsumables.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), consumableRefinement);
        if (!restoredConsumables.LoadFromDisk(TestOutput::Path("powerup-profile-check.dat")) ||
            restoredConsumables.GetPowerupCount(consumable) != 1 ||
            restoredConsumables.configuration.powerups != consumableProbe.configuration.powerups) { ++checkFailures; }
        // Exercise Equip -> original DataStore write -> fresh battle host;
        // use an isolated copy, never the user's active profile or saves/.
        const auto equipDirectory = std::filesystem::path(TestOutput::Path("powerup-equip-check")) / std::to_string(window.GetTicksMs());
        CProfileManager equipProfile;
        if (!LoadProfile(toc, tables, equipProfile, equipDirectory, TestOutput::Fixtures())) { return 1; }
        ZPowerupScene equipHost(toc, tables, player, vitals, scene, effects, equipProfile);
        if (!equipHost.Init() || !equipHost.Equip(0, equippedLeft) || !equipHost.Equip(1, equippedRight) ||
            !equipProfile.SaveToDisk(equipDirectory)) { ++checkFailures; }
        CProfileManager reloadProfile;
        if (!LoadProfile(toc, tables, reloadProfile, equipDirectory)) { return 1; }
        ZPowerupScene reloadHost(toc, tables, player, vitals, scene, effects, reloadProfile);
        if (!reloadHost.Init()) { return 1; }
        const GameObjectRef reloadLeft = reloadHost.GetEquipped(0);
        const GameObjectRef reloadRight = reloadHost.GetEquipped(1);
        if (reloadLeft.packHash != equippedLeft.packHash || reloadLeft.localIndex != equippedLeft.localIndex ||
            reloadRight.packHash != equippedRight.packHash || reloadRight.localIndex != equippedRight.localIndex) { ++checkFailures; }
        std::printf("[powerup-equip-check] left=%u right=%u native-reload=1 host-reload=1 failures=%u\n",
            reloadLeft.localIndex, reloadRight.localIndex, checkFailures);
        std::printf("[powerup-play-check] shield/defense/priority/expiry/weapon-swap/save failures=%u\n", checkFailures);
        session.Restart(startX, startY, startFacing);
        {
            // Isolate impact contracts from steering: a stationary original
            // enemy receives real CBullet -> CombatScene -> script splash hits.
            ZWeaponEffects blastEffects(toc, tables, program);
            CLevel blastScene(tables, program, enemies, player, vitals, blastEffects, loaded.playerTemplate->gameScale);
            const unsigned grenadeBullets[] = {90, 93, 94};
            for (unsigned bulletIndex : grenadeBullets) {
                blastScene.Reset();
                ZCombatEnemy *target = blastScene.Spawn(0, 600, 450);
                if (target == nullptr) { return 1; }
                CEnemy &enemy = target->model.enemy;
                // Let the original spawn sequence reach its ordinary hit handler.
                for (int elapsed = 0; elapsed < 1000; elapsed += 16) { enemy.Update(16); }
                enemy.combat.health = 10000;
                enemy.combat.maxHealth = 10000;
                GameObjectRef bulletRef = consumable;
                bulletRef.localIndex = static_cast<std::uint8_t>(bulletIndex);
                if (blastEffects.SpawnProjectile(bulletRef, 600, 350, 0, 0, 0, kPlayerCombatId, 0) == 0) { ++checkFailures; }
                float matrix[16];
                blastScene.PlayerMatrix(matrix);
                unsigned impactState = 255;
                int maximumStunMs = 0;
                int lastImpactMs = 0;
                for (int elapsed = 0; elapsed < 4000; elapsed += 16) {
                    enemy.combat.x = 600;
                    enemy.combat.y = 450;
                    enemy.combat.targetAlive = false;
                    const int hitsBefore = enemy.combat.hitCount;
                    blastEffects.Update(player, matrix, 0, 16);
                    maximumStunMs = std::max(maximumStunMs, enemy.stun.GetRemainingMs());
                    enemy.Update(16);
                    if (enemy.combat.hitCount != hitsBefore) { lastImpactMs = elapsed; }
                    if (enemy.combat.hitCount > 0 && impactState == 255) { impactState = enemy.GetStateId(); }
                }
                float expectedDamage = 100;
                if (bulletIndex == 93) { expectedDamage = 30; }
                expectedDamage *= PlayerArmorMultiplier(player, 1);
                if (std::abs(enemy.combat.totalDamage - expectedDamage) > 0.01f) { ++checkFailures; }
                int expectedStunMs = 0;
                if (bulletIndex == 93) { expectedStunMs = 750; }
                if (bulletIndex == 94) { expectedStunMs = 2000; }
                // A late electrical pulse may outlive the 4-second damage probe.
                // Verify its actual expiry boundary instead of assuming spawn+4s.
                const int remainingStunMs = enemy.stun.GetRemainingMs();
                if (remainingStunMs > 0) {
                    if (remainingStunMs > expectedStunMs) { ++checkFailures; }
                    enemy.Update(remainingStunMs - 1);
                    if (!enemy.stun.IsActive() || enemy.stun.GetRemainingMs() != 1) { ++checkFailures; }
                    enemy.Update(1);
                }
                if (maximumStunMs != expectedStunMs || enemy.stun.IsActive()) { ++checkFailures; }
                std::printf("[powerup-impact-check] bullet=%u damage=%.3f expected=%.3f hits=%d state=%u failures=%u\n",
                    bulletIndex, enemy.combat.totalDamage, expectedDamage, enemy.combat.hitCount, impactState, checkFailures);
                std::printf("[powerup-impact-check] stun=%d expected=%d expired=%d last-impact=%d remaining=%d\n",
                    maximumStunMs, expectedStunMs, !enemy.stun.IsActive(), lastImpactMs, remainingStunMs);
            }
            if (powerupStudy) {
                std::ofstream attributeReport(TestOutput::Path("powerup-enemy-attributes.txt"));
                const unsigned attributes[] = {19, 6171, 23};
                for (unsigned enemyIndex = 0; enemyIndex < enemies.size(); ++enemyIndex) {
                    if (!enemies[enemyIndex].script.IsPresent()) { continue; }
                    for (unsigned flags : attributes) {
                        blastScene.Reset();
                        ZCombatEnemy *target = blastScene.Spawn(enemyIndex, 600, 450);
                        if (target == nullptr) { return 1; }
                        CEnemy &enemy = target->model.enemy;
                        for (int elapsed = 0; elapsed < 1000; elapsed += 16) { enemy.Update(16); }
                        enemy.combat.health = 10000;
                        enemy.combat.maxHealth = 10000;
                        const unsigned before = enemy.GetStateId();
                        ZCombatHit probe;
                        probe.owner = kPlayerCombatId;
                        probe.ownerType = 0;
                        probe.x = 600;
                        probe.y = 350;
                        probe.flags = flags;
                        probe.damage = 1;
                        probe.splash = true;
                        enemy.ReceiveHit(probe);
                        const unsigned immediately = enemy.GetStateId();
                        const int durationMs = enemy.stun.GetRemainingMs();
                        if (durationMs > 100) {
                            const float frozenX = enemy.combat.x, frozenY = enemy.combat.y;
                            const int frozenTime = enemy.GetPart(0).controller.GetAnimation().GetTimeMs();
                            enemy.Update(100);
                            if (enemy.combat.x != frozenX || enemy.combat.y != frozenY ||
                                enemy.GetPart(0).controller.GetAnimation().GetTimeMs() != frozenTime ||
                                enemy.stun.GetRemainingMs() != durationMs - 100) { ++checkFailures; }
                        }
                        for (int elapsed = 0; elapsed < 4000; elapsed += 16) { enemy.Update(16); }
                        checkFailures += static_cast<unsigned>(enemy.GetUnsupportedFunctionCount());
                        if (enemy.stun.IsActive()) { ++checkFailures; }
                        attributeReport << enemyIndex << ' ' << enemies[enemyIndex].owner << " flags=" << flags
                            << " before=" << before << " impact=" << immediately << " end=" << unsigned(enemy.GetStateId())
                            << " stun=" << durationMs << " damage=" << enemy.combat.totalDamage << " unsupported=" << enemy.GetUnsupportedFunctionCount() << '\n';
                    }
                }
                // Controller phase is based on remaining time, with no expiry
                // event for CEnemy's original empty callback methods.
                CStunController clock;
                clock.SetStunned(750, 50, 2);
                clock.Update(50);
                if (clock.GetOffset() != -2) { ++checkFailures; }
                clock.Update(50);
                if (clock.GetOffset() != 2) { ++checkFailures; }
                if (!clock.Update(650) || clock.IsActive() || clock.GetOffset() != 0) { ++checkFailures; }
                if (clock.Update(16)) { ++checkFailures; }
                std::printf("[powerup-attribute-check] combinations=228 timing/movement/animation/expiry failures=%u\n", checkFailures);
            }
        }
        session.Restart(startX, startY, startFacing);
        ZCombatHit forceProbe;
        const float originalMaximum = vitals.maximum;
        const float originalBrotherMaximum = brother.vitals.maximum;
        for (float maximum : {20.0f, 100.0f, 205.0f}) {
            session.Restart(startX, startY, startFacing);
            vitals.maximum = maximum;
            vitals.health = maximum;
            vitals.invincible = false;
            if (withBrother) {
                brother.vitals.maximum = maximum;
                brother.vitals.health = maximum;
                brother.vitals.invincible = false;
            }
            ZCombatHit percentage;
            percentage.ownerType = 1;
            percentage.damage = 10;
            percentage.percentDamage = true;
            percentage.x = scene.GetPlayer().x;
            percentage.y = scene.GetPlayer().y;
            scene.Splash(percentage, 200, 360, 0, 0);
            const float expected = std::max(0.0f, maximum * 0.1f * (2 - PlayerArmorMultiplier(player, 0)));
            if (std::abs(vitals.health - (maximum - expected)) > 0.001f) { ++checkFailures; }
            if (withBrother) {
                const float brotherExpected = std::max(0.0f, maximum * 0.1f * (2 - PlayerArmorMultiplier(brotherModel, 0)));
                if (std::abs(brother.vitals.health - (maximum - brotherExpected)) > 0.001f) { ++checkFailures; }
            }
            std::printf("[splash-check] maximum=%.0f percent=10 damage=%.3f brother=%d failures=%u\n",
                maximum, maximum - vitals.health, withBrother, checkFailures);
        }
        vitals.maximum = originalMaximum;
        brother.vitals.maximum = originalBrotherMaximum;
        session.Restart(startX, startY, startFacing);
        forceProbe.ownerType = 1;
        forceProbe.x = startX - 40;
        forceProbe.y = startY;
        scene.Splash(forceProbe, 60, 360, 100, 100);
        if (scene.GetPlayer().x != startX || scene.GetPlayer().y != startY) { ++checkFailures; }
        scene.Update(16, 0, 0, false);
        if (std::hypot(scene.GetPlayer().x - startX, scene.GetPlayer().y - startY) > 4) { ++checkFailures; }
        std::printf("[survival-check] map force gradual displacement=%.2f failures=%u\n",
            std::hypot(scene.GetPlayer().x - startX, scene.GetPlayer().y - startY), checkFailures);
        session.Restart(startX, startY, startFacing);
        const std::size_t initialActors = scene.AliveCount();
        if (gameContext == nullptr) {
            vitals.health = vitals.maximum * 0.5f;
            scene.AddExperience(progress.GetExperienceDelta());
            if (progress.GetLevel() != 2 || vitals.maximum != 9 || vitals.health != 4.5f) { ++checkFailures; }
            progress.SetExperience(0);
            scene.SetPlayerProgress(&progress);
            vitals.Reset();
        }
        const ZPlayerModel *stablePlayer = &player;
        const float armorBefore = PlayerArmorMultiplier(player, 0);
        const std::size_t alternateWeapon = (weaponSlot + 1) % weapons.size();
        if (!EquipControlledPlayer(tables, loaded, program, weapons[alternateWeapon]) ||
            !EquipControlledPlayer(tables, loaded, program, weapons[weaponSlot])) { return 1; }
        if (loaded.players[0].model.get() != stablePlayer || player.vitals != &vitals ||
            PlayerArmorMultiplier(player, 0) != armorBefore) { ++checkFailures; }
        // Kill through the actual shared hit path, then exercise the same R action.
        ZCombatHit fatal;
        fatal.ownerType = 1;
        fatal.damage = 10000;
        scene.ApplyHit(kPlayerCombatId, fatal);
        if (!vitals.dead || vitals.health != 0 || vitals.deaths != 1) { ++checkFailures; }
        if (session.IsDeathComplete()) {
            std::printf("[death-check] FAIL: postgame opens on fatal hit before death animation\n");
            return 1;
        }
        session.Restart(startX, startY, startFacing);
        if (vitals.dead || vitals.health != vitals.maximum || session.GetLevel().GetWave() != static_cast<int>(startWave) ||
            scene.AliveCount() != initialActors || effects.GetBulletCount() != 0 ||
            PlayerArmorMultiplier(player, 0) != armorBefore || scene.GetPlayer().x != startX || scene.GetPlayer().y != startY) { ++checkFailures; }
        std::printf("[survival-check] equipment/death/restart failures=%u\n", checkFailures);
        if (withBrother) {
            // A fatal shared hit must not kill the human. Run the original
            // death animation to its hold state, then the actual wave export.
            scene.ApplyHit(kBrotherCombatId, fatal);
            if (!brother.vitals.dead || vitals.dead) { ++checkFailures; }
            for (int elapsed = 0; elapsed < 8000; elapsed += 16) { AdvancePlayer(brotherModel, 16); }
            const int deadState = brotherModel.weapon->brother.GetStateId();
            brotherModel.weapon->brother.OnWaveCleared();
            for (int elapsed = 0; elapsed < 8000; elapsed += 16) { AdvancePlayer(brotherModel, 16); }
            if (brother.vitals.dead || brother.vitals.health != brother.vitals.maximum ||
                !brotherModel.weapon->brother.CanMove() || !brotherModel.weapon->brother.CanShoot()) { ++checkFailures; }
            std::printf("[brother-check] death-state=%d revived-state=%d health=%.1f failures=%u\n",
                deadState, brotherModel.weapon->brother.GetStateId(), brother.vitals.health, checkFailures);
            session.Restart(startX, startY, startFacing);
            brother.vitals.invincible = true;
        }
        // This harness uses real projectiles and enemy death scripts, with
        // invincibility only to keep the automated pilot running deterministically.
        vitals.invincible = true;
        const int targetWave = std::min(static_cast<int>(startWave + checkWaves), session.GetLevel().GetWaveLimit());
        auto pilot = CreateSurvivalInputDriver(scene, loaded.map.GetVisibleBounds());
        if (!pilot) { return 1; }
        float previousDamage = 0;
        int stalledMs = 0;
        // Late waves contain hundreds of actors. Bound a wave generously, but
        // stop promptly when actual damage and kills cease for two minutes.
        for (int elapsed = 0; elapsed < static_cast<int>(checkWaves * 600000); elapsed += 16) {
            float moveX = 0;
            float moveY = 0;
            pilot->Update(16, moveX, moveY);
            session.Update(16, moveX, moveY, !withBrother || gameContext != nullptr);
            float damage = scene.damageDealt;
            for (const auto &actor : scene.GetEnemies()) { damage += actor->model.enemy.combat.totalDamage; }
            stalledMs += 16;
            if (damage != previousDamage) { stalledMs = 0; previousDamage = damage; }
            if (stalledMs > 120000) {
                std::printf("[survival-check] stopped: no damage progress for 120 seconds\n");
                break;
            }
            if (session.GetLevel().GetWave() >= targetWave) { break; }
        }
        pilot->Report();
        const unsigned unsupportedLevel = session.GetLevel().GetUnimplementedCallCount();
        const unsigned unsupportedSpawner = session.GetLevel().GetSpawner().GetUnsupportedCount();
        checkFailures += unsupportedLevel + unsupportedSpawner;
        std::printf("[survival-script-check] level=%u spawner=%u\n", unsupportedLevel, unsupportedSpawner);
        checkFailures += pickups.failures;
        std::printf("[pickup-check] spawned=%u collected=%u remaining=%zu failures=%u\n",
            pickups.spawned, pickups.collected, pickups.GetCount(), pickups.failures);
        if (withBrother) {
            // AI-only research must acquire targets. In a profile run the
            // human's long-range gun may kill everything before the 200px AI scan.
            if (gameContext == nullptr && brother.GetTargetCount() == 0) { ++checkFailures; }
            std::printf("[brother-check] targets=%u shots=%zu position=%.1f,%.1f hp=%.1f failures=%u\n",
                brother.GetTargetCount(), effects.GetShotCount(), brother.x, brother.y, brother.vitals.health, checkFailures);
        }
        if (session.GetLevel().GetWave() < targetWave || session.GetKills() == 0 || scene.GetInvalidSpawnCount() != 0) { ++checkFailures; }
        if (targetWave == session.GetLevel().GetWaveLimit() && !session.GetLevel().IsCleared()) { ++checkFailures; }
        if (scene.GetClearedWaves() != targetWave - startWave) { ++checkFailures; }
        // AI-only kills grant XP but not the human's Xplodium kill streak.
        // CLevel::OnEnemyKilled :119566 tests GetBrotherType, not IsPlayer.
        if (progress.GetExperience() == 0 || progress.GetLevel() < 2 || (!withBrother && scene.GetXplodium() == 0) ||
            vitals.maximum != progress.GetHealth()) { ++checkFailures; }
        std::printf("[survival-check] progress level=%u xp=%llu xplodium=%llu\n",
            progress.GetLevel(), progress.GetExperience(), scene.GetXplodium());
        checkFailures += props.GetFailures();
        std::printf("[prop-check] actual-hits=%u failures=%u\n", props.GetHitCount(), props.GetFailures());
        std::printf("[survival-check] wave=%d kills=%u alive=%d spawned=%u invalid=%u failures=%u\n",
            session.GetLevel().GetWave(), session.GetKills(), session.CountEnemies(), scene.GetSpawnCount(), scene.GetInvalidSpawnCount(), checkFailures);
        // CEnemySpawner::GetSpawnPointOffScreen keeps rule-driven spawns out of
        // the camera rectangle; on-screen ones would be the "in your face" case.
        if (session.GetOnScreenSpawns() != 0) { ++checkFailures; }
        std::printf("[survival-check] closest-spawn=%.1f on-screen-spawns=%u failures=%u\n",
            session.GetClosestSpawnDistance(), session.GetOnScreenSpawns(), checkFailures);
        // Sound cues are the audible one-shots after per-tick coalescing; they
        // are what a spread-out kill streak turns into.
        std::printf("[survival-check] shots=%zu sounds=%zu player=%.1f,%.1f stun=%d brother=%d\n",
            effects.GetShotCount(), effects.GetSoundCueCount(), scene.GetPlayer().x, scene.GetPlayer().y,
            vitals.stunMs, player.weapon->brother.GetStateId());
        for (const auto &actor : scene.GetEnemies()) {
            const ZEnemyCombat &enemy = actor->model.enemy.combat;
            if (!enemy.dead) {
                std::printf("[survival-check] alive %s pos=%.1f,%.1f health=%.1f state=%d behaviour=%d\n",
                    actor->data->owner.c_str(), enemy.x, enemy.y, enemy.health,
                    actor->model.enemy.GetStateId(), enemy.behaviour);
                ILayerPath *path = loaded.map.GetPathLayer(session.GetLevel().GetPathLayer());
                if (path != nullptr) {
                    const int first = path->FindNearest(enemy.x, enemy.y);
                    const int last = path->FindNearest(scene.GetPlayer().x, scene.GetPlayer().y);
                    std::printf("[survival-check] navigation=%d to=%.1f,%.1f radius=%.1f path=%d->%d next=%d\n",
                        enemy.hasNavigationTarget, enemy.navigationX, enemy.navigationY, actor->model.enemy.GetPart(0).radius,
                        first, last, path->FindNext(first, last));
                    const int containing = path->FindNode(enemy.x, enemy.y);
                    const int goal = path->FindNode(scene.GetPlayer().x, scene.GetPlayer().y);
                    const int next = path->FindNext(containing, goal);
                    if (next >= 0) {
                        const auto &point = path->GetNodes()[next];
                        std::printf("[survival-check] containing=%d goal=%d next=%d center=%.1f,%.1f\n", containing, goal, next, point.x, point.y);
                    }
                    const auto &vertices = loaded.collisionScene.GetVertices();
                    for (const auto &edge : loaded.collisionScene.GetEdges()) {
                        const ZCollisionPoint &a = vertices[edge.firstVertex];
                        const ZCollisionPoint &b = vertices[edge.secondVertex];
                        if (std::hypot((a.x + b.x) * 0.5f - enemy.x, (a.y + b.y) * 0.5f - enemy.y) < 85) {
                            std::printf("[survival-check] nearby edge %.1f,%.1f -> %.1f,%.1f\n", a.x, a.y, b.x, b.y);
                        }
                    }
                }
            }
        }
        capturePath = TestOutput::Path("survival-check-") + packShortName + ".png";
    }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}
