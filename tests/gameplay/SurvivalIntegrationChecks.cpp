#include "gameplay/SurvivalChecks.h"
#include "TestOutput.h"
using namespace MapDetail;

int CheckSurvivalPowerupInventory(SurvivalPowerupInventoryFixture fixture) {
    auto &researchProfile = fixture.researchProfile;
    auto & powerupStudy = fixture.powerupStudy;

    if (powerupStudy) {
        ZPackTables tables(fixture.toc);
        std::vector<ZStoreEntry> store;
        if (!LoadStoreCatalog(fixture.toc, tables, store)) { return 1; }
        for (const auto &entry : store) {
            if (entry.data.type < 10 || entry.data.type > 13) { continue; }
            for (const auto &object : entry.data.objects) {
                if (object.type == 17) { researchProfile.AddPowerup(object.object, 10); }
            }
        }
        std::printf("[powerup-study] isolated inventory: 10 of each store powerup\n");
    }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}

int CheckSurvivalLevelSounds(SurvivalLevelSoundsFixture fixture) {
    auto & checkFailures = fixture.checkFailures;
    auto & check = fixture.check;
    auto & session = fixture.session;

    if (check) { checkFailures += CheckLevelSounds(session.GetLevel(), fixture.scene); }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}

int CheckSurvivalPropRoutes(SurvivalPropRoutesFixture fixture) {
    auto & checkFailures = fixture.checkFailures;
    auto & check = fixture.check;
    if (check) { checkFailures += CheckPropEntryRoutes(fixture.loaded, fixture.scene); }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}

int CheckSurvivalTriggerRoutes(SurvivalTriggerRoutesFixture fixture) {
    auto & checkFailures = fixture.checkFailures;
    auto & check = fixture.check;
    auto & session = fixture.session;
    auto & startX = fixture.startX;
    auto & startY = fixture.startY;
    auto & startFacing = fixture.startFacing;

    if (check) { checkFailures += CheckTriggerRoutes(session, fixture.map, fixture.scene, startX, startY, startFacing); }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}

int CheckSurvivalPlacedProps(SurvivalPlacedPropsFixture fixture) {
    auto & check = fixture.check;
    auto & loaded = fixture.loaded;

    if (check) {
        // Inspect the actual placed resources, after LEVEL messages and Bind.
        for (const ZPlacedProp &prop : loaded.props) {
            if (!prop.active || prop.runtime == nullptr) { continue; }
            std::printf("[map-interaction] prop=%08x:%u id=%d pos=%.1f,%.1f state=%u removed=%d health=%.1f animations=%d,%d,%d body=%zu bullet=%zu\n",
                prop.sprite->resource.packHash, prop.sprite->resource.localIndex, prop.objectId, prop.x, prop.y,
                prop.runtime->GetStateId(), prop.runtime->IsRemoved(), prop.runtime->GetHealth(),
                prop.runtime->GetAnimation(0), prop.runtime->GetAnimation(1), prop.runtime->GetAnimation(2),
                prop.runtime->GetCollision().GetEdges().size(), prop.runtime->GetCollision(true).GetEdges().size());
        }
    }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}

int CheckSurvivalBrotherPose(SurvivalBrotherPoseFixture fixture) {
    auto & checkFailures = fixture.checkFailures;
    auto & check = fixture.check;
    auto & withBrother = fixture.withBrother;
    auto & scene = fixture.scene;
    auto & brother = fixture.brother;
    auto & brotherModel = fixture.brotherModel;

    if (check && withBrother) {
        const float distance = std::hypot(scene.GetPlayer().x - brother.x, scene.GetPlayer().y - brother.y);
        const bool separated = distance >= scene.GetPlayerRadius() * 2;
        std::printf("[brother-spawn-check] player=%.1f,%.1f brother=%.1f,%.1f distance=%.1f separated=%d\n",
            scene.GetPlayer().x, scene.GetPlayer().y, brother.x, brother.y, distance, separated);
        if (!separated) { ++checkFailures; }
        // The first visible frame must already use the selected idle pose,
        // even while the level intro postpones the first simulation tick.
        auto &torso = brotherModel.weapon->brother.GetTorso();
        const int torsoIndex = torso.GetMeshConfigIndex();
        ZPlayerPart *part = brotherModel.parts[torsoIndex].get();
        if (brotherModel.weapon->brother.TorsoUsesWeapon()) { part = brotherModel.weapon->configs[torsoIndex].get(); }
        std::vector<float> expectedPose;
        const bool ready = torso.GetAnimation().Evaluate(expectedPose) && !expectedPose.empty() && expectedPose == part->pose;
        std::printf("[brother-pose-check] evaluated=%zu uploaded=%zu ready=%d\n", expectedPose.size(), part->pose.size(), ready);
        if (!ready) { ++checkFailures; }
    }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}

int CheckSurvivalTutorial(SurvivalTutorialFixture fixture) {
    auto & checkFailures = fixture.checkFailures;
    auto & capturePath = fixture.capturePath;
    auto & check = fixture.check;
    auto & gameContext = fixture.gameContext;
    auto & tables = fixture.tables;
    auto & weapons = fixture.weapons;
    auto & vitals = fixture.vitals;
    auto & progress = fixture.progress;
    auto & program = fixture.program;
    auto & loaded = fixture.loaded;
    auto & player = fixture.player;
    auto & weaponSlot = fixture.weaponSlot;
    auto & equippedWeaponSlot = fixture.equippedWeaponSlot;
    auto & scene = fixture.scene;
    auto & brother = fixture.brother;
    auto & session = fixture.session;
    auto & powerups = fixture.powerups;
    auto & pickups = fixture.pickups;
    auto & pickupProfile = fixture.pickupProfile;
    auto & tutorial = fixture.tutorial;
    auto & accountedXplodium = fixture.accountedXplodium;

    if (check && tutorial) {
        const CScript &script = loaded.playerTemplate->script;
        for (unsigned index = 0; index < script.GetFunctions().size(); ++index) {
            std::printf("[tutorial-script] function=%u ", index);
            const CScriptCode &code = script.GetFunctions()[index];
            for (unsigned byte = 0; byte <= code.GetByteLength(); ++byte) { std::printf("%02X ", code.Begin()[byte]); }
            std::printf("\n");
        }
        for (unsigned index = 0; index < script.GetStates().size(); ++index) {
            const CScriptState &state = script.GetStates()[index];
            for (const auto &handler : state.GetExports()) {
                std::printf("[tutorial-script] state=%u export=%u ", index, handler.id);
                for (unsigned byte = 0; byte <= handler.code.GetByteLength(); ++byte) { std::printf("%02X ", handler.code.Begin()[byte]); }
                std::printf("\n");
            }
            const auto &code = state.GetEnterCode();
            if (code.Begin() != nullptr) {
                std::printf("[tutorial-script] state=%u enter ", index);
                for (unsigned byte = 0; byte <= code.GetByteLength(); ++byte) { std::printf("%02X ", code.Begin()[byte]); }
                std::printf("\n");
            }
        }
        auto pilot = CreateSurvivalInputDriver(scene, loaded.map.GetVisibleBounds());
        if (!pilot) { return 1; }
        int previousStep = -2;
        int grenadeWaitMs = 0;
        bool grenadeGateChecked = false;
        bool barrelGateChecked = false;
        bool armorBreakChecked = false;
        // Exercise the original script with actual movement and projectiles.
        for (int elapsed = 0; elapsed < 180000; elapsed += 16) {
            const int step = session.GetLevel().GetTutorialStep();
            if (step != previousStep) {
                std::printf("[tutorial-check] time=%d step=%d enemies=%d kills=%u grenades=%u\n",
                    elapsed, step, session.CountEnemies(), session.GetKills(), powerups.GetCount(13));
                previousStep = step;
                if (!SaveSurvivalProgress(gameContext, progress, scene, session.GetLevel(), accountedXplodium)) { return 1; }
            }
            if (step == -1) { break; }
            if (step == 2) {
                SetPlayerInput(player, false, false);
                player.weapon->brother.OnSwapGun();
            }
            float moveX = 0, moveY = 0;
            pilot->Update(16, moveX, moveY);
            if (step == 0) { moveX = 1; moveY = 0; }
            float pickupX = 0, pickupY = 0;
            if (pickups.GetObjectPosition(501, pickupX, pickupY)) {
                moveX = pickupX - scene.GetPlayer().x;
                moveY = pickupY - scene.GetPlayer().y;
            }
            if (step == 5 && powerups.GetCount(13) > 0) {
                grenadeWaitMs += 16;
                if (grenadeWaitMs >= 5000) {
                    // User reference: the large tutorial enemy ignores gunfire
                    // until a grenade lands. Keep real fire active for five seconds.
                    if (!grenadeGateChecked) {
                        grenadeGateChecked = true;
                        if (session.GetKills() != 1 || session.CountEnemies() != 1) { ++checkFailures; }
                        std::printf("[tutorial-check] grenade-gate gunfire-ms=%d kills=%u alive=%d failures=%u\n",
                            grenadeWaitMs, session.GetKills(), session.CountEnemies(), checkFailures);
                    }
                    if (!barrelGateChecked) {
                        for (const auto &actor : scene.GetEnemies()) {
                            CEnemy &enemy = actor->model.enemy;
                            if (enemy.combat.dead || !enemy.combat.enabled) { continue; }
                            const float originalX = enemy.combat.x;
                            const float originalY = enemy.combat.y;
                            const float healthBefore = enemy.combat.health;
                            const ZCombatId enemyId = enemy.combat.id;
                            unsigned barrels = 0;
                            for (ZPlacedProp &prop : loaded.props) {
                                if (!prop.active || !prop.runtime || prop.runtime->GetHealth() <= 0 ||
                                    prop.sprite->interactiveKind != ZInteractivePropKind::Barrel) { continue; }
                                // Trigger the real barrel Flow and its scene damage dispatch.
                                enemy.combat.x = prop.x + 30;
                                enemy.combat.y = prop.y;
                                prop.runtime->Damage(prop.runtime->GetHealth(), 0);
                                for (unsigned tick = 0; tick < 8; ++tick) {
                                    session.Update(16, 0, 0, false);
                                    // A regression may kill and retire the actor during Update.
                                    const ZCombatEnemy *target = scene.Find(enemyId);
                                    if (!target || target->model.enemy.combat.health != healthBefore) {
                                        std::printf("[tutorial-check] armored barrel damaged enemy object=%d\n", prop.objectId);
                                        return 1;
                                    }
                                }
                                ++barrels;
                                if (enemy.combat.dead) { break; }
                            }
                            std::printf("[tutorial-check] barrel-gate enemy=%08x:%u barrels=%u hp=%.0f->%.0f state=%d\n",
                                actor->data->packHash, actor->data->ordinal, barrels, healthBefore,
                                enemy.combat.health, enemy.GetStateId());
                            enemy.combat.x = originalX;
                            enemy.combat.y = originalY;
                            if (barrels == 0 || enemy.combat.health != healthBefore) { return 1; }
                        }
                        barrelGateChecked = true;
                    }
                    bool inGrenadeRange = false;
                    for (const auto &actor : scene.GetEnemies()) {
                        const auto &enemy = actor->model.enemy.combat;
                        if (enemy.dead || !enemy.enabled) { continue; }
                        const float dx = enemy.x - scene.GetPlayer().x, dy = enemy.y - scene.GetPlayer().y;
                        moveX = dx; moveY = dy;
                        if (std::hypot(dx, dy) <= 95) { inGrenadeRange = true; moveX = 0; moveY = 0; }
                    }
                    if (inGrenadeRange) { powerups.Select(13); powerups.UseSelected(); }
                }
            }
            vitals.invincible = true;
            const bool fireGun = step != 2 && (step != 5 || grenadeWaitMs < 5000);
            session.Update(16, moveX, moveY, fireGun);
            if (barrelGateChecked && !armorBreakChecked && powerups.GetCount(13) == 0) {
                for (const auto &actor : scene.GetEnemies()) {
                    CEnemy &enemy = actor->model.enemy;
                    if (enemy.combat.dead || !enemy.combat.enabled || enemy.GetPartCount() != 1) { continue; }
                    // ENEMY27 Flow @0xA9..0xB9 removes armor and rejects the
                    // first grenade. Later hits use @0x86 without a grace timer.
                    const float healthBefore = enemy.combat.health;
                    std::printf("[tutorial-check] armor-break hp=%.0f parts=%u hits=%u\n",
                        healthBefore, enemy.GetPartCount(), enemy.combat.hitCount);
                    if (healthBefore != enemy.combat.maxHealth || enemy.combat.hitCount != 0) { return 1; }
                    armorBreakChecked = true;
                    break;
                }
            }
            if (player.weapon->brother.TakeWeaponSwap()) {
                const GameObjectRef &rifle = pickupProfile->configuration.guns[1];
                for (std::size_t index = 0; index < weapons.size(); ++index) {
                    if (weapons[index].packHash != rifle.packHash || weapons[index].ordinal != rifle.localIndex) { continue; }
                    if (!EquipControlledPlayer(tables, loaded, program, weapons[index])) { return 1; }
                    weaponSlot = index;
                    equippedWeaponSlot = 1;
                    break;
                }
            }
        }
        if (session.GetLevel().GetTutorialStep() != -1 || !grenadeGateChecked ||
            !barrelGateChecked || !armorBreakChecked) { ++checkFailures; }
        std::printf("[tutorial-check] final-step=%d failures=%d\n", session.GetLevel().GetTutorialStep(), checkFailures);
        capturePath = TestOutput::Path("tutorial-check.png");
    }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}

int CheckSurvivalHorde(SurvivalHordeFixture fixture) {
    auto & checkFailures = fixture.checkFailures;
    auto & capturePath = fixture.capturePath;
    auto & check = fixture.check;
    auto & startWave = fixture.startWave;
    auto & vitals = fixture.vitals;
    auto & loaded = fixture.loaded;
    auto & scene = fixture.scene;
    auto & session = fixture.session;
    auto & horde = fixture.horde;

    if (check && horde) {
        vitals.invincible = true;
        auto pilot = CreateSurvivalInputDriver(scene, loaded.map.GetVisibleBounds());
        if (!pilot) { return 1; }
        const int initialWave = session.GetLevel().GetWave();
        int targetWave = initialWave + 1;
        if (initialWave == 0) { targetWave = 2; }
        for (int elapsed = 0; elapsed < 1800000 && session.GetLevel().GetWave() < targetWave; elapsed += 16) {
            float moveX = 0, moveY = 0;
            pilot->Update(16, moveX, moveY);
            session.Update(16, moveX, moveY, true);
            AdvanceProps(loaded.props, 16);
            AdvanceTileLayers(loaded.map, 16);
        }
        // Finish the real HUD transition callback; it restores BOKOR time scale
        // and releases its next state. A first-wave-only check misses this seam.
        for (int elapsed = 0; elapsed < 60000 && session.IsTransitioning(); elapsed += 16) { session.Update(16, 0, 0, false); }
        if (session.GetLevel().GetWave() < targetWave || session.GetKills() == 0 || scene.GetScore() == 0 ||
            session.GetLevel().GetObjectTimeScale() != 1 ||
            scene.GetInvalidSpawnCount() != 0 || session.GetLevel().GetUnimplementedCallCount() != 0 ||
            session.GetLevel().GetSpawner().GetUnsupportedCount() != 0) { ++checkFailures; }
        std::printf("[horde-check] initial=%d next=%d spawned=%u kills=%u stopwatch=%d slow=%.4f failures=%u\n",
            initialWave, session.GetLevel().GetWave(), scene.GetSpawnCount(), session.GetKills(),
            session.GetLevel().GetStopwatchTime(), session.GetLevel().GetObjectTimeScale(), checkFailures);
        capturePath = TestOutput::Path("horde-check-") + std::to_string(startWave) + ".png";
    }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}

int CheckSurvivalCampaign(SurvivalCampaignFixture fixture) {
    auto & checkFailures = fixture.checkFailures;
    auto & capturePath = fixture.capturePath;
    auto & packShortName = fixture.packShortName;
    auto & mapIndex = fixture.mapIndex;
    auto & check = fixture.check;
    auto & archiveMission = fixture.archiveMission;
    auto & vitals = fixture.vitals;
    auto & loaded = fixture.loaded;
    auto & scene = fixture.scene;
    auto & session = fixture.session;
    auto & pickups = fixture.pickups;
    auto & horde = fixture.horde;

    if (check && archiveMission != nullptr && !horde) {
        // Research input pilot: visit authored pickups and trigger edges without
        // teleporting actors or directly invoking trigger/death callbacks.
        vitals.invincible = true;
        std::vector<ZCollisionPoint> goals;
        for (unsigned index = 0; index < loaded.map.GetObjectLayerCount(); ++index) {
            const CLayerObject &layer = loaded.map.GetObjectLayer(index);
            if (static_cast<int>(layer.GetLayerIndex()) != session.GetLevel().GetObjectLayer()) { continue; }
            for (const ZPlacedObject &object : layer.GetObjects()) {
                if (object.objectType == static_cast<unsigned>(ZPlacedObjectType::Pickup)) { goals.emplace_back(object.x, object.y); }
            }
        }
        for (unsigned index = 0; index < loaded.map.GetCollisionLayerCount(); ++index) {
            const CLayerCollision &layer = loaded.map.GetCollisionLayer(index);
            if (static_cast<int>(layer.GetLayerIndex()) != session.GetLevel().GetTriggerLayer()) { continue; }
            for (const ZCollisionEdge &edge : layer.GetCollision().GetEdges()) {
                const auto &a = layer.GetCollision().GetVertices()[edge.firstVertex];
                const auto &b = layer.GetCollision().GetVertices()[edge.secondVertex];
                const float length = std::hypot(b.x - a.x, b.y - a.y);
                if (length < 1) { continue; }
                const float normalX = (a.y - b.y) / length * 40;
                const float normalY = (b.x - a.x) / length * 40;
                goals.emplace_back((a.x + b.x) * 0.5f + normalX, (a.y + b.y) * 0.5f + normalY);
                goals.emplace_back((a.x + b.x) * 0.5f - normalX, (a.y + b.y) * 0.5f - normalY);
            }
        }
        auto pilot = CreateSurvivalInputDriver(scene, loaded.map.GetVisibleBounds());
        if (!pilot) { return 1; }
        unsigned goal = 0, reached = 0;
        int goalElapsed = 0;
        for (int elapsed = 0; elapsed < 360000 && !session.GetLevel().IsCleared(); elapsed += 16) {
            float moveX = 0, moveY = 0;
            pilot->Update(16, moveX, moveY);
            if (goal < goals.size()) {
                goalElapsed += 16;
                const ZCollisionPoint &target = goals[goal];
                float waypointX = target.x, waypointY = target.y;
                scene.GetBrotherWaypoint(scene.GetPlayer().x, scene.GetPlayer().y, target.x, target.y, waypointX, waypointY);
                moveX = waypointX - scene.GetPlayer().x;
                moveY = waypointY - scene.GetPlayer().y;
                if (std::hypot(target.x - scene.GetPlayer().x, target.y - scene.GetPlayer().y) < 20 || goalElapsed > 20000) {
                    const bool arrived = goalElapsed <= 20000;
                    if (arrived) { ++reached; }
                    std::printf("[campaign-check] goal=%u target=%.0f,%.0f reached=%d player=%.1f,%.1f\n",
                        goal, target.x, target.y, arrived, scene.GetPlayer().x, scene.GetPlayer().y);
                    ++goal;
                    goalElapsed = 0;
                }
            }
            session.Update(16, moveX, moveY, true);
            AdvanceProps(loaded.props, 16);
            AdvanceTileLayers(loaded.map, 16);
        }
        if (scene.GetSpawnCount() == 0 || session.GetKills() == 0 || scene.GetInvalidSpawnCount() != 0) { ++checkFailures; }
        std::printf("[campaign-check] goals=%u/%zu spawned=%u kills=%u pickups=%u cleared=%d failures=%u\n",
            reached, goals.size(), scene.GetSpawnCount(), session.GetKills(), pickups.collected, session.GetLevel().IsCleared(), checkFailures);
        capturePath = TestOutput::Path("campaign-check-") + packShortName + "-" + std::to_string(mapIndex) + ".png";
    }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}

int CheckSurvivalPowerupCapture(SurvivalPowerupCaptureFixture fixture) {
    auto & capturePath = fixture.capturePath;
    auto & powerupStudy = fixture.powerupStudy;
    auto & scene = fixture.scene;
    auto & powerups = fixture.powerups;

    if (powerupStudy && !capturePath.empty()) {
        powerups.Select(5);
        powerups.UseSelected();
        powerups.Select(16);
        powerups.UseSelected();
        for (int elapsed = 0; elapsed < 256; elapsed += 16) {
            scene.Update(16, 0, 0, false);
            scene.UpdatePowerup(powerups.GetPowerup(), 16);
        }
        powerups.Select(19);
        powerups.UseSelected();
        for (int elapsed = 0; elapsed < 1400; elapsed += 16) {
            scene.Update(16, 0, 0, false);
            scene.UpdatePowerup(powerups.GetPowerup(), 16);
        }
        capturePath = TestOutput::Path("powerup-play-check.png");
    }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}
