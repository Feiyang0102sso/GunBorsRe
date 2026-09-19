#include "gameplay/SurvivalChecks.h"
#include "TestOutput.h"
#include "gun_bros_re/debug/Capture.h"
using namespace MapDetail;

int CheckSurvivalBoss(SurvivalBossFixture fixture) {
    auto & checkFailures = fixture.checkFailures;
    auto & packShortName = fixture.packShortName;
    auto & bossStudy = fixture.bossStudy;
    auto & toc = fixture.toc;
    auto & tables = fixture.tables;
    auto & enemies = fixture.enemies;
    auto & vitals = fixture.vitals;
    auto & window = fixture.window;
    auto & program = fixture.program;
    auto & loaded = fixture.loaded;
    auto & player = fixture.player;
    auto & scene = fixture.scene;
    auto & session = fixture.session;
    auto & startX = fixture.startX;
    auto & startY = fixture.startY;
    auto & startFacing = fixture.startFacing;

    if (bossStudy) {
        // Test fixtures only. Retail resources and Flow still choose the Boss,
        // spawn node, armor transitions, damage and subsequent ordinary wave.
        vitals.invincible = true;
        // Placed map mechanisms (Haven's two turrets) participate in LEVEL
        // counts. The production shortcut now owns preservation and draining.
        if (!PushBossCheckKey(window, 's', false, true)) { return 1; }
        for (char letter : std::string("wasdchd")) {
            if (!PushBossCheckKey(window, letter)) { return 1; }
        }
        if (window.TakeCheatCode() != "chd") { ++checkFailures; }
        for (char letter : std::string("stbos")) {
            if (!PushBossCheckKey(window, letter)) { return 1; }
        }
        if (!PushBossCheckKey(window, 's', true) || !window.TakeCheatCode().empty()) { ++checkFailures; }
        if (!PushBossCheckKey(window, 's')) { return 1; }
        const std::string bossCode = window.TakeCheatCode();
        if (bossCode != "stboss" || !window.TakeCheatCode().empty() || window.IsKeyDown(ZKeyCode::S)) { ++checkFailures; }
        std::printf("[stboss-input-check] code=%s repeats-ignored=1 released-s=%d failures=%u\n",
            bossCode.c_str(), !window.IsKeyDown(ZKeyCode::S), checkFailures);
        if (bossCode != "stboss" || !session.SkipToBoss()) { return 1; }
        const unsigned introBeforeRepeat = session.GetLevel().GetBossIntroSerial();
        if (session.SkipToBoss() || session.GetLevel().GetBossIntroSerial() != introBeforeRepeat) { ++checkFailures; }
        CEnemy *boss = nullptr;
        for (auto &actor : scene.GetEnemies()) {
            if (actor->CanReceiveProjectile(0, Collision::Player)) { boss = actor.get(); }
        }
        if (session.GetLevel().GetBossIntroSerial() != 1 || boss == nullptr) {
            std::printf("[boss-check] %s missing scripted boss state=%d\n", packShortName.c_str(), session.GetLevel().GetStateId());
            return 1;
        }
        const Collision::ObjectId bossId = boss->combat.id;
        CCamera &camera = loaded.GetCamera();
        // Compare the real camera with the original target operation at the
        // same authored bounds. This also permits legitimate edge clamping.
        CCamera expected = camera;
        expected.SetTarget(boss->combat.x, boss->combat.y);
        expected.SetCameraMode(2);
        CCamera actual = camera;
        const CLayerCamera::Rectangle bounds = loaded.GetVisibleBounds();
        actual.Update(1000);
        expected.Update(1000);
        actual.UpdatePosition(scene.GetPlayer().x, scene.GetPlayer().y, bounds.x, bounds.y, bounds.width, bounds.height, 480, 320);
        expected.UpdatePosition(scene.GetPlayer().x, scene.GetPlayer().y, bounds.x, bounds.y, bounds.width, bounds.height, 480, 320);
        const float targetError = std::hypot(actual.GetX() - expected.GetX(), actual.GetY() - expected.GetY());
        if (camera.GetMode() != 2 || targetError > 0.01f) { ++checkFailures; }
        std::printf("[boss-check] %s camera-error=%.3f boss=%s\n", packShortName.c_str(), targetError, boss->data->owner.c_str());
        int introElapsed = 0;
        while (introElapsed < 60000 && (!session.GetLevel().CanPlayerMove() ||
            !session.GetLevel().CanPlayerShoot() || camera.GetMode() != 0)) {
            session.Update(16, 0, 0, false);
            introElapsed += 16;
        }
        boss = scene.Find(bossId);
        if (boss == nullptr) { return 1; }
        if (!session.GetLevel().CanPlayerMove() || !session.GetLevel().CanPlayerShoot() || camera.GetMode() != 0) { ++checkFailures; }
        std::printf("[boss-check] intro-ms=%d state=%u mode=%u move=%d shoot=%d\n", introElapsed,
            boss->GetStateId(), camera.GetMode(), session.GetLevel().CanPlayerMove(), session.GetLevel().CanPlayerShoot());

        if (packShortName == "pack7") {
            // Follow the actual ENEMY/LEVEL attack exports; do not spawn BULLET104 here.
            bool sawBeam = false;
            for (int elapsed = 0; elapsed < 90000 && !sawBeam; elapsed += 16) {
                session.Update(16, 0, 0, false);
                for (const auto &item : scene.GetBulletRenderItems()) {
                    const CBullet &bullet = *item.bullet;
                    if (bullet.removed || !bullet.beam || bullet.owner != bossId) { continue; }
                    sawBeam = true;
                    const auto &ref = bullet.data->GetSpriteRef();
                    std::printf("[boss-live-beam-check] elapsed=%d bullet=%08x:%u sprite=%u/%u body=%d caps=%d/%d group=%d length=%.1f\n",
                        elapsed, bullet.source.resource.packHash, bullet.source.resource.localIndex,
                        ref.archetype, ref.animation, bullet.animation, bullet.beamSourceAnimation,
                        bullet.beamEndAnimation, item.group, bullet.length);
                    if (bullet.animation != ref.animation || item.group != 5) { ++checkFailures; }
                    break;
                }
            }
            if (!sawBeam) { ++checkFailures; }
            if (sawBeam) {
                int width, height;
                window.GetDrawableSize(width, height);
                ZQuadBatch batch;
                if (!batch.Create(program)) { return 1; }
                float mvp[kMatrix4dElements];
                Matrix4dOrthoTopLeft(static_cast<float>(width), static_cast<float>(height), kMapDepthRange, mvp);
                boss = scene.Find(bossId);
                if (boss == nullptr) { return 1; }
                Matrix4dTranslate(mvp, width * 0.5f - boss->combat.x, height * 0.5f - boss->combat.y);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                loaded.DrawBackground(batch, true, false);
                batch.Draw(program, mvp);
                CRenderQueue::Draw(loaded, batch, program, mvp, true, &scene, nullptr, 0, width);
                scene.Draw(mvp, nullptr, kLevelCameraScale, true);
                if (!Capture::SaveFrame(window, TestOutput::Path("haven-boss-live-beam.png"))) { return 1; }
            }
            std::printf("[boss-live-beam-check] observed=%d failures=%u\n", sawBeam, checkFailures);
        }

        GameObjectRef grenade;
        grenade.packHash = toc.GetPack(toc.GetPackIndexFromName("pack5"))->GetPackHash();
        const unsigned grenadeOrdinals[] = {90, 93, 94};
        std::array<CBullet::Template, 3> grenadeTemplates;
        for (unsigned index = 0; index < 3; ++index) {
            std::vector<std::uint8_t> bytes;
            if (!tables.ReadSectionResource(grenade.packHash, ZGameSection::Bullet, grenadeOrdinals[index], bytes)) { return 1; }
            CArrayInputStream input(bytes);
            if (!grenadeTemplates[index].Init(input)) { return 1; }
        }
        CEnemy &enemy = *boss;
        const unsigned initialParts = enemy.GetPartCount();
        const float initialHealth = enemy.combat.health;
        Collision::Hit hit;
        hit.owner = Collision::Player;
        hit.ownerType = 0;
        hit.damage = grenadeTemplates[0].GetBaseDamage();
        hit.flags = grenadeTemplates[0].GetFlags();
        hit.x = enemy.combat.x;
        hit.y = enemy.combat.y;
        const unsigned contactState = enemy.GetStateId();
        // Exercise repeated direct contacts through the real world dispatcher.
        for (int contact = 0; contact < 5; ++contact) { scene.ApplyHit(bossId, hit); }
        if (enemy.GetPartCount() != initialParts || enemy.combat.health != initialHealth || enemy.GetStateId() != contactState) { ++checkFailures; }
        std::printf("[boss-check] direct contacts=5 parts=%u expected=%u hp=%.1f\n", enemy.GetPartCount(), initialParts, enemy.combat.health);
        // Restart this fixture after the deliberate failing contact probe so
        // independent explosion assertions stay meaningful on the old code.
        const CEnemy::Template *bossData = boss->data;
        std::size_t bossEntry = static_cast<std::size_t>(bossData - enemies.data());
        CLevel blastScene(toc, tables, program);
    blastScene.BindCombat(enemies, player, vitals, loaded.GetResources().playerTemplate->GetGameScale());
        for (unsigned kind = 0; kind < 3; ++kind) {
            blastScene.Reset();
            vitals.invincible = true;
            CEnemy *target = blastScene.Spawn(bossEntry, 600, 450);
            if (target == nullptr) { return 1; }
            CEnemy &blastEnemy = *target;
            // Each authored intro emits LEVEL event 11 when its animation
            // completes. Wait for that cue instead of assuming a duration.
            bool introComplete = false;
            for (int elapsed = 0; elapsed < 60000 && !introComplete; elapsed += 16) {
                blastEnemy.Update(16);
                for (const CEnemy::Action &action : blastEnemy.combat.actions) {
                    if (action.kind == CEnemy::Action::Kind::LevelEvent && action.slot == 11) { introComplete = true; }
                }
                blastEnemy.combat.actions.clear();
            }
            if (!introComplete) { ++checkFailures; }
            const unsigned readyState = blastEnemy.GetStateId();
            const float health = blastEnemy.combat.health;
            const unsigned parts = blastEnemy.GetPartCount();
            grenade.localIndex = static_cast<std::uint8_t>(grenadeOrdinals[kind]);
            // A real stationary grenade overlaps the Boss throughout its fuse:
            // this catches repeated direct collisions before the splash cue.
            unsigned throws = 1;
            if (kind == 0) { throws = 3; }
            for (unsigned number = 0; number < throws; ++number) {
                blastEnemy.combat.x = 600;
                blastEnemy.combat.y = 450;
                if (blastScene.SpawnProjectile(grenade, 600, 450, 0, 0, 0, Collision::Player, 0) == 0) { return 1; }
                float matrix[16];
                blastScene.PlayerMatrix(matrix);
                for (int elapsed = 0; elapsed < 4000; elapsed += 16) {
                    blastEnemy.combat.x = 600;
                    blastEnemy.combat.y = 450;
                    blastEnemy.combat.behaviour = 7;
                    blastEnemy.combat.targetAlive = false;
                    blastScene.Update(player, matrix, 0, 16);
                    blastEnemy.Update(16);
                }
                unsigned expectedParts = parts;
                // The ice handler calls internal 7 after applying stun: it
                // also removes one part. The earlier research missed this call.
                if (kind == 2) { expectedParts = parts - 1; }
                if (kind == 0) {
                    expectedParts = parts - number - 1;
                    if (parts == 7) {
                        const unsigned strippedParts[] = {5, 3, 2};
                        expectedParts = strippedParts[number];
                    }
                }
                if (blastEnemy.GetPartCount() != expectedParts || blastEnemy.combat.health != health) { ++checkFailures; }
                std::printf("[boss-check] grenade=%u throw=%u parts=%u expected=%u hp=%.1f expected-hp=%.1f state=%u\n",
                    grenade.localIndex, number + 1, blastEnemy.GetPartCount(), expectedParts, blastEnemy.combat.health, health, blastEnemy.GetStateId());
            }
            if (kind == 0) {
                // The hit animation's parent uses 2x for attribute 0; ordinary
                // behavior uses 4x. Let Flow return before probing that branch.
                for (int elapsed = 0; elapsed < 60000 && blastEnemy.GetStateId() != readyState; elapsed += 16) {
                    blastEnemy.Update(16);
                }
                hit.flags = 1;
                hit.damage = 1;
                blastScene.ApplyHit(blastEnemy.combat.id, hit);
                if (std::abs(health - blastEnemy.combat.health - 4) > 0.01f) { ++checkFailures; }
                std::printf("[boss-check] stripped-bullet-damage=%.1f expected=4\n", health - blastEnemy.combat.health);
            }
        }
        // Fixture Bosses changed the shared camera; restore normal gameplay
        // before exercising the real LEVEL death callback and wave transition.
        camera.SetCameraMode(0);
        vitals.invincible = true;
        enemy.Damage(enemy.combat.health);
        for (int elapsed = 0; elapsed < 12000; elapsed += 16) { session.Update(16, 0, 0, false); }
        if (session.GetLevel().HasLargeEnemyHealthBars() || !session.GetLevel().CanPlayerMove() ||
            !session.GetLevel().CanPlayerShoot() || session.GetLevel().GetBossIntroSerial() != 1 || session.GetLevel().GetWave() < 1) { ++checkFailures; }
        std::printf("[boss-check] %s return-wave=%d state=%d failures=%u\n",
            packShortName.c_str(), session.GetLevel().GetWave(), session.GetLevel().GetStateId(), checkFailures);
        // Mid-revolution, revolution end and final supported wave use their
        // own original spawn quotas; no caller kills enemies between requests.
        for (int wave : {24, 49, 249, 450, 499}) {
            session.SetStartWave(wave);
            session.Restart(startX, startY, startFacing);
            vitals.invincible = false;
            if (!session.SkipToBoss() || vitals.invincible || session.GetLevel().GetWave() != wave ||
                session.GetLevel().GetBossIntroSerial() != 1) { ++checkFailures; }
            // Compare authored health tiers and REV multipliers at the actual
            // production shortcut, including the first wave of REV10.
            for (const auto &actor : scene.GetEnemies()) {
                if (actor->mapPlaced || !actor->CanReceiveProjectile(0, Collision::Player)) { continue; }
                const auto &combat = actor->combat;
                float baseHealth = 100;
                const int realWave = wave % 50;
                if (realWave >= 10) { baseHealth = 300; }
                if (realWave >= 20) { baseHealth = 600; }
                if (realWave >= 30) { baseHealth = 900; }
                if (realWave >= 40) { baseHealth = 1500; }
                const float multiplier = session.GetLevel().GetEnemyMultiplier(combat.templateRef, 1);
                float revolutionMultiplier = float(wave / 50 + 1);
                // Haven LEVEL adds two to each REV's health factor.
                if (packShortName == "pack9") { revolutionMultiplier += 2; }
                const float expectedHealth = baseHealth * revolutionMultiplier;
                if (std::abs(combat.health - expectedHealth) > 0.01f) { ++checkFailures; }
                std::printf("[boss-health-check] %s wave=%d hp=%.1f multiplier=%.2f expected=%.1f failures=%u\n",
                    packShortName.c_str(), wave, combat.health, multiplier, expectedHealth, checkFailures);
            }
            std::printf("[stboss-wave-check] %s requested=%d actual=%d failures=%u\n",
                packShortName.c_str(), wave, session.GetLevel().GetWave(), checkFailures);
            if (wave == 450 || wave == 499) {
                blastScene.Reset();
                CEnemy *target = blastScene.Spawn(bossEntry, 600, 450);
                if (target == nullptr) { return 1; }
                CEnemy &blastEnemy = *target;
                bool ready = false;
                for (int time = 0; time < 60000 && !ready; time += 16) {
                    blastEnemy.Update(16);
                    for (const auto &action : blastEnemy.combat.actions) {
                        if (action.kind == CEnemy::Action::Kind::LevelEvent && action.slot == 11) { ready = true; }
                    }
                    blastEnemy.combat.actions.clear();
                }
                if (!ready) { ++checkFailures; }
                const auto readyState = blastEnemy.GetStateId();
                const float health = blastEnemy.combat.health;
                grenade.localIndex = 90;
                for (unsigned number = 0; number < 4; ++number) {
                    for (int time = 0; time < 60000 && blastEnemy.GetStateId() != readyState; time += 16) {
                        blastEnemy.Update(16);
                    }
                    const float before = blastEnemy.combat.health;
                    blastScene.SpawnProjectile(grenade, 600, 450, 0, 0, 0, Collision::Player, 0);
                    float matrix[16];
                    blastScene.PlayerMatrix(matrix);
                    for (int time = 0; time < 4000; time += 16) {
                        blastEnemy.combat.x = 600; blastEnemy.combat.y = 450;
                        blastEnemy.combat.behaviour = 7;
                        blastEnemy.combat.targetAlive = false;
                        blastScene.Update(player, matrix, 0, 16);
                        blastEnemy.Update(16);
                    }
                    float expectedDamage = 0;
                    // Ordinary grenades use internal 5, not the 4x gun branch.
                    // pack5 Boss @0xACD explicitly sets HP to 1 * REV before
                    // ApplyCollision, so its fourth frag is an authored kill.
                    if (number == 3) {
                        expectedDamage = 100 * player.GetArmorMultiplier(1);
                        if (packShortName == "pack12") { expectedDamage = before; }
                    }
                    if (std::abs(before - blastEnemy.combat.health - expectedDamage) > 0.01f) { ++checkFailures; }
                    std::printf("[boss-rev10-grenade-check] %s wave=%d throw=%u initial=%.1f hp=%.1f damage=%.1f expected=%.1f armor-attack=%.2f failures=%u\n",
                        packShortName.c_str(), wave, number + 1, health, blastEnemy.combat.health,
                        before - blastEnemy.combat.health, expectedDamage, player.GetArmorMultiplier(1), checkFailures);
                }
            }
        }
        if (checkFailures > 0) { return 1; }
        return 0;
    }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}
