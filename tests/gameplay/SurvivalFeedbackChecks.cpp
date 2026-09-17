#include "gameplay/SurvivalChecks.h"
#include "TestOutput.h"
using namespace MapDetail;

int CheckSurvivalFeedback(SurvivalFeedbackFixture fixture) {
    auto & checkFailures = fixture.checkFailures;
    auto & capturePath = fixture.capturePath;
    auto & feedbackStudy = fixture.feedbackStudy;
    auto & toc = fixture.toc;
    auto & tables = fixture.tables;
    auto & weapons = fixture.weapons;
    auto & enemies = fixture.enemies;
    auto & vitals = fixture.vitals;
    auto & survivalHud = fixture.survivalHud;
    auto & program = fixture.program;
    auto & loaded = fixture.loaded;
    auto & player = fixture.player;
    auto & scene = fixture.scene;
    auto & session = fixture.session;
    auto & props = fixture.props;
    auto & startX = fixture.startX;
    auto & startY = fixture.startY;
    auto & startFacing = fixture.startFacing;

    if (feedbackStudy) {
        // Real BIG instances and the same clocks as RunSurvival; no source save.
        vitals.invincible = true;
        session.SetHud(&survivalHud);
        session.Restart(startX, startY, startFacing);
        for (unsigned tick = 0; tick < 300; ++tick) { session.Update(16, 0, 0, false); }
        survivalHud.OnWaveClear(session.GetLevel().GetWave(), true, 100, false);
        const float beforeX = scene.GetPlayer().x;
        const float beforeY = scene.GetPlayer().y;
        for (unsigned tick = 0; tick < 20; ++tick) { session.Update(16, 1, 0, false); }
        const float moved = std::hypot(scene.GetPlayer().x - beforeX, scene.GetPlayer().y - beforeY);
        if (moved <= 0) { ++checkFailures; }
        std::printf("[feedback-check] notice-moving=%.3f interstitial=%d failures=%u\n", moved, survivalHud.HasInterstitial(), checkFailures);
        // Reproduce duplicate object index 0 in the actual layer-2/layer-3 map.
        for (const auto &prop : loaded.props) {
            if (!prop.active || prop.sprite->interactiveKind != ZInteractivePropKind::Spire) { continue; }
            float targetX = 0, targetY = 0;
            const unsigned target = props.ResolveIndicatorTarget(prop.objectId);
            const bool bound = target != 0 && props.GetIndicatorTarget(target, targetX, targetY);
            float placedX = 0, placedY = 0;
            const bool originalOrder = props.GetObjectPosition(prop.objectId, placedX, placedY) &&
                placedX == prop.x && placedY == prop.y;
            bool found = false;
            for (const auto &marker : session.GetLevel().GetIndicators()) {
                if (marker.type != 1 || marker.objectId != prop.objectId) { continue; }
                found = bound && marker.targetKey == ((2ULL << 32) | target) && marker.x == targetX && marker.y == targetY;
            }
            if (!found || !originalOrder) { ++checkFailures; }
            // A later enemy sharing the map index must not steal the marker.
            auto *duplicate = scene.Spawn(0, prop.x + 600, prop.y + 600);
            if (!duplicate) { return 1; }
            duplicate->objectId = prop.objectId;
            float afterX = 0, afterY = 0;
            const bool retained = session.GetLevel().GetIndicatorTarget((2ULL << 32) | target, afterX, afterY) &&
                afterX == targetX && afterY == targetY;
            if (!retained) { ++checkFailures; }
            duplicate->model.enemy.combat.removed = true;
            std::printf("[indicator-check] layer=%u object=%d placed=%.1f,%.1f target=%.1f,%.1f original-order=%d bound=%d duplicate-retained=%d failures=%u\n",
                prop.objectLayer, prop.objectId, prop.x, prop.y, targetX, targetY, originalOrder, found, retained, checkFailures);
        }
        // The spire's four phases come from pack7 PROP33's Flow. Compare the
        // runtime clock against native 58's actual Q8 scale, not guessed seconds.
        for (ZPlacedProp &prop : loaded.props) {
            if (!prop.active || prop.runtime == nullptr || prop.sprite->interactiveKind != ZInteractivePropKind::Spire) { continue; }
            CProp &spire = *prop.runtime;
            const bool layersCorrect = spire.GetAnimation(2) == 255 &&
                BackgroundSlotFor(prop) == RuntimeSlotFor(prop, 0) &&
                ForegroundSlotFor(prop) == RuntimeSlotFor(prop, 2);
            if (!layersCorrect) { ++checkFailures; }
            spire.HandleMessage(0);
            const unsigned activeState = spire.GetStateId();
            const int timer = spire.GetTimerMs();
            const int step = std::max(1, int(std::lround(16 * session.GetLevel().GetObjectTimeScale())));
            const int expected = (timer + step - 1) / step * 16;
            int elapsed = 0;
            while (spire.GetStateId() == activeState && elapsed < expected + 32) { props.Update(16); elapsed += 16; }
            if (elapsed != expected) { ++checkFailures; }
            const unsigned cooldownState = spire.GetStateId();
            const int cooldown = spire.GetTimerMs();
            int cooled = 0;
            while (spire.GetStateId() == cooldownState && cooled < cooldown + 32) { props.Update(16); cooled += 16; }
            if (cooled != (cooldown + 15) / 16 * 16) { ++checkFailures; }
            std::printf("[feedback-check] spire-layers=%d active=%d expected=%d cooldown=%d expected=%d failures=%u\n",
                layersCorrect, elapsed, expected, cooled, cooldown, checkFailures);
        }
        // Two real attacks in one update, with separate human and AI owners.
        // Probe pack1's first sixteen authored enemy templates (including
        // shields and dormant map objects), rather than assuming all accept hits.
        unsigned simultaneousHits = 0;
        for (std::size_t index = 0; index < enemies.size(); ++index) {
            if (enemies[index].packHash != CStringToKey("pack1") || enemies[index].ordinal > 15) { continue; }
            ZCombatEnemy *actor = scene.Spawn(index, scene.GetPlayer().x + 250, scene.GetPlayer().y);
            if (actor == nullptr) { ++checkFailures; continue; }
            CEnemy &enemy = actor->model.enemy;
            for (unsigned tick = 0; tick < 100; ++tick) { enemy.Update(16); }
            const float health = enemy.combat.health;
            ZCombatHit hit;
            hit.owner = kPlayerCombatId;
            hit.ownerType = 0;
            hit.damage = health / 10;
            hit.part = 0;
            hit.x = enemy.combat.x;
            hit.y = enemy.combat.y + 100;
            hit.projectile = 10001;
            const ZHitResult first = scene.ApplyHit(enemy.combat.id, hit);
            hit.owner = kBrotherCombatId;
            hit.projectile = 10002;
            const ZHitResult second = scene.ApplyHit(enemy.combat.id, hit);
            for (unsigned tick = 0; tick < 100; ++tick) { enemy.Update(16); }
            if (first == ZHitResult::Hit && second == ZHitResult::Hit) {
                ++simultaneousHits;
                if (enemy.combat.hitCount != 2 || std::abs(health - enemy.combat.health - hit.damage * 2) > 0.01f) { ++checkFailures; }
            }
            std::printf("[feedback-hit] enemy=%s first=%d second=%d hp=%.2f->%.2f pending=%d hits=%d\n",
                enemies[index].owner.c_str(), int(first), int(second), health, enemy.combat.health, enemy.combat.collisionPending, enemy.combat.hitCount);
        }
        {
            // Native HandleCollision does not pause a projectile when a state
            // has no hit handler. A dormant original turret must not swallow a
            // penetrating round before it reaches the ordinary enemy behind it.
            ZWeaponEffects probeEffects(toc, tables, program);
            CLevel probe(tables, program, enemies, player, vitals, probeEffects, loaded.playerTemplate->gameScale);
            probe.Reset();
            ZCombatEnemy *front = nullptr, *back = nullptr;
            for (std::size_t index = 0; index < enemies.size(); ++index) {
                if (enemies[index].packHash != CStringToKey("pack1")) { continue; }
                if (enemies[index].ordinal == 6) { front = probe.Spawn(index, 600, 450); }
                if (enemies[index].ordinal == 0) { back = probe.Spawn(index, 600, 550); }
            }
            if (front == nullptr || back == nullptr) { return 1; }
            for (unsigned tick = 0; tick < 100; ++tick) { back->model.enemy.Update(16); }
            GameObjectRef round;
            for (const auto &gun : weapons) {
                std::vector<std::uint8_t> bytes;
                const auto &ref = gun.data.GetBulletRef();
                if (ref.IsNull() || !tables.ReadSectionResource(ref.packHash, ZGameSection::Bullet, ref.localIndex, bytes)) { continue; }
                CArrayInputStream input(bytes);
                CBullet::Template bullet;
                if (!bullet.Init(input)) { return 1; }
                if ((bullet.GetFlags() & 0x140) == 0x40) { round = ref; break; }
            }
            if (round.IsNull()) { return 1; }
            if (probeEffects.SpawnProjectile(round, 600, 350, 0, 90, 600, kBrotherCombatId, 0) == 0) { return 1; }
            float matrix[16];
            probe.PlayerMatrix(matrix);
            for (unsigned tick = 0; tick < 90; ++tick) { probeEffects.Update(player, matrix, 0, 16); }
            const float damage = back->model.enemy.combat.totalDamage;
            if (damage <= 0) { ++checkFailures; }
            std::printf("[feedback-piercing] bullet=%08x:%u front-pending=%d back-damage=%.2f failures=%u\n",
                round.packHash, round.localIndex, front->model.enemy.combat.collisionPending, damage, checkFailures);

            // CBullet::CanBeCulled :60583 retires a projectile that has left
            // the camera rectangle travelling away from it, and keeps one that
            // is still heading towards it. Same real BULLET template, same
            // update path; only the camera rectangle is supplied here.
            ZWeaponEffects cullEffects(toc, tables, program);
            CLevel cullScene(tables, program, enemies, player, vitals, cullEffects, loaded.playerTemplate->gameScale);
            cullScene.Reset();
            cullScene.SetViewCenter(600, 450);
            cullEffects.SetViewBounds(600, 450, 200, 200);  // y in [350, 550]
            float cullMatrix[16];
            cullScene.PlayerMatrix(cullMatrix);
            // Both start just below the view. One travels away from it, one
            // towards it; the outbound one is the only one culled at once.
            if (cullEffects.SpawnProjectile(round, 600, 560, 0, 90, 600, kBrotherCombatId, 0) == 0) { return 1; }
            if (cullEffects.SpawnProjectile(round, 600, 560, 0, -90, 600, kBrotherCombatId, 0) == 0) { return 1; }
            // 160ms: the outbound one is well clear of the near edge and gone,
            // the inbound one has entered the view and is still travelling.
            for (unsigned tick = 0; tick < 10; ++tick) { cullEffects.Update(player, cullMatrix, 0, 16); }
            const std::size_t afterOutbound = cullEffects.GetBulletCount();
            // Out the far side, well before the 3000ms expiry could retire it.
            for (unsigned tick = 0; tick < 30; ++tick) { cullEffects.Update(player, cullMatrix, 0, 16); }
            const std::size_t afterCrossing = cullEffects.GetBulletCount();
            if (afterOutbound != 1 || afterCrossing != 0) { ++checkFailures; }
            std::printf("[feedback-cull] leaving-culled inbound-alive=%zu after-far-edge=%zu age=640ms failures=%u\n",
                afterOutbound, afterCrossing, checkFailures);
        }
        // Regression: the native bar size reads LEVEL variable 4, not the
        // revolution index. Exercise actual BIG enemies and the production draw data.
        {
            CLevel &level = session.GetLevel();
            const int savedWave = level.GetWave();
            const auto savedFlag = *level.VariableResolver(4);
            *level.VariableResolver(4) = 0;
            level.SetWave(0);
            const auto firstBars = scene.EnemyHealthBars();
            const auto screenBars = scene.EnemyHealthBars(1600.0f / 480);
            if (screenBars.empty() || screenBars[0].width != 100 || screenBars[0].height != 13) { ++checkFailures; }
            level.SetWave(level.GetWavesPerRevolution());
            const auto laterBars = scene.EnemyHealthBars();
            *level.VariableResolver(4) = 1;
            const auto flaggedBars = scene.EnemyHealthBars();
            if (firstBars.empty() || laterBars.empty() || flaggedBars.empty()) { ++checkFailures; }
            else {
                if (firstBars[0].width != laterBars[0].width || flaggedBars[0].width != firstBars[0].width * 2) { ++checkFailures; }
                std::printf("[audio-health-check] bar first=%.1f later=%.1f flag=%.1f failures=%u\n",
                    firstBars[0].width, laterBars[0].width, flaggedBars[0].width, checkFailures);
            }
            level.SetWave(savedWave);
            *level.VariableResolver(4) = savedFlag;
        }
        // A single-part original enemy supplies an independent GetBounds
        // centre. Check the final screen rectangle, not just its dimensions.
        {
            ZWeaponEffects anchorEffects(toc, tables, program);
            CLevel anchorScene(tables, program, enemies, player, vitals, anchorEffects, loaded.playerTemplate->gameScale);
            ZCombatEnemy *target = nullptr;
            for (std::size_t index = 0; index < enemies.size(); ++index) {
                if (enemies[index].packHash == CStringToKey("pack1") && enemies[index].ordinal == 0) {
                    target = anchorScene.Spawn(index, 640, 480);
                    break;
                }
            }
            if (target == nullptr || target->model.enemy.GetPartCount() != 1) { ++checkFailures; }
            else {
                const auto *mesh = target->model.enemy.GetPart(0).controller.GetAnimation().GetMesh();
                if (mesh == nullptr) { ++checkFailures; }
                else {
                    const float centre = target->model.enemy.combat.x + int(mesh->GetBounds().centerX);
                    float maxError = 0;
                    unsigned cases = 0;
                    for (int screenWidth : {1024, 1600, 1920}) {
                        const int screenHeight = screenWidth * 3 / 4;
                        for (float zoom : {0.5f, 1.0f, 2.6f}) {
                            auto bars = anchorScene.EnemyHealthBars(std::min(screenWidth / 480.0f, screenHeight / 320.0f));
                            ProjectEnemyHealthBars(bars, 100, 200, zoom, screenWidth, screenHeight);
                            if (bars.size() != 1) { ++checkFailures; continue; }
                            const float actual = (bars[0].x + bars[0].width * 0.5f) * screenWidth / 1024;
                            const float expected = (centre - 100) * zoom;
                            maxError = std::max(maxError, std::abs(actual - expected));
                            ++cases;
                        }
                    }
                    if (maxError > 0.01f || cases != 9) { ++checkFailures; }
                    std::printf("[healthbar-alignment-check] cases=%u max-centre-error-px=%.3f failures=%u\n", cases, maxError, checkFailures);
                }
            }
        }
        // Reproduce a group death through the original enemy export and the
        // same CombatScene/WeaponEffects path used by ordinary combat.
        ZAudioPlayer backendAudio;
        std::vector<std::uint64_t> deathWavs;
        // A real SOUNDEFFECT reference, for the one-voice-per-WAV check below.
        GameObjectRef effectSound;
        // Effects headroom: the configured 0..10 dial reaches the mix as
        // dial x 0.1, the same scale the original's voices use.
        const float configuredGain = GameHostSettings().effectsVolume * 0.1f;
        if (std::abs(ZAudioPlayer::GetEffectsGain() - configuredGain) > 0.001f) { ++checkFailures; }
        std::printf("[audio-health-check] effects-dial=%d gain=%.2f music-gain=0.30 failures=%u\n",
            GameHostSettings().effectsVolume, ZAudioPlayer::GetEffectsGain(), checkFailures);
        for (unsigned kinds = 1; kinds <= 2; ++kinds) {
            std::vector<GameObjectRef> batchDeathSounds;
            ZWeaponEffects deathEffects(toc, tables, program);
            CLevel deathScene(tables, program, enemies, player, vitals, deathEffects, loaded.playerTemplate->gameScale);
            deathScene.Reset();
            for (std::size_t index = 0; index < enemies.size(); ++index) {
                if (enemies[index].packHash != CStringToKey("pack1") || enemies[index].ordinal >= kinds) { continue; }
                for (unsigned count = 0; count < 12; ++count) {
                    ZCombatEnemy *actor = deathScene.Spawn(index, 800, 500);
                    if (actor == nullptr) { ++checkFailures; continue; }
                    actor->model.enemy.Damage(actor->model.enemy.combat.health);
                    if (count == 0) {
                        const auto moveIndex = actor->model.enemy.GetPart(0).controller.GetMoveIndex();
                        if (moveIndex >= 0) {
                            for (const auto &sound : enemies[index].moveSet.GetMoves()[moveIndex].sounds) {
                                std::vector<std::uint8_t> bytes;
                                const auto pack = enemies[index].moveSet.GetPackHash();
                                const auto key = (std::uint64_t(pack) << 32) | sound.soundId;
                                if (!tables.ReadSectionResource(pack, ZGameSection::Wav, sound.soundId, bytes) || !backendAudio.Load(key, bytes)) { ++checkFailures; continue; }
                                if (std::find(deathWavs.begin(), deathWavs.end(), key) == deathWavs.end()) { deathWavs.push_back(key); }
                                GameObjectRef deathSound;
                                deathSound.packHash = pack;
                                deathSound.localIndex = sound.soundId;
                                batchDeathSounds.push_back(deathSound);
                                std::printf("[audio-health-check] death-move-wav=%08x:%u enemy=%s\n", pack, sound.soundId, enemies[index].owner.c_str());
                            }
                        }
                    }
                    if (kinds == 2 && count == 0) {
                        // Inspect, but do not consume, the original death export.
                        for (const auto &action : actor->model.enemy.combat.actions) {
                            if (action.kind != CEnemy::Action::Kind::Sound) { continue; }
                            std::vector<std::uint8_t> bytes;
                            if (!tables.ReadSectionResource(action.resource.packHash, ZGameSection::SoundEffect, action.resource.localIndex, bytes)) { ++checkFailures; continue; }
                            CArrayInputStream input(bytes);
                            CGameAssetRef wav;
                            wav.Init(input);
                            if (input.Overran() || wav.assetId < 0 || !tables.ReadSectionResource(wav.packHash, ZGameSection::Wav, wav.assetId, bytes)) { ++checkFailures; continue; }
                            const auto key = (std::uint64_t(wav.packHash) << 32) | wav.assetId;
                            if (!backendAudio.Load(key, bytes)) { ++checkFailures; continue; }
                            deathWavs.push_back(key);
                            effectSound = action.resource;
                            std::printf("[audio-health-check] death-wav=%08x:%d enemy=%s\n", wav.packHash, wav.assetId, enemies[index].owner.c_str());
                        }
                    }
                }
            }
            deathScene.Update(16, 0, 0, false);
            const auto sounds = deathEffects.GetSoundCueCount();
            if (sounds != kinds) { ++checkFailures; }
            std::printf("[audio-health-check] group-death kinds=%u actors=%u sounds=%zu expected=%u failures=%u\n",
                kinds, kinds * 12, sounds, kinds, checkFailures);
            // Repeat the actual authored sound across a production tick
            // boundary. Do not assume a random death move always cues at t=0.
            deathScene.Update(16, 0, 0, false);
            const auto beforeRepeat = deathEffects.GetSoundCueCount();
            for (const auto &sound : batchDeathSounds) { deathEffects.PlayMoveSound(sound); }
            // Host audio adaptation: the copy already playing still covers it.
            if (deathEffects.GetSoundCueCount() != beforeRepeat) { ++checkFailures; }
            std::printf("[audio-health-check] next-tick kinds=%u new-sounds=%zu failures=%u\n",
                kinds, deathEffects.GetSoundCueCount() - beforeRepeat, checkFailures);
            // ... and is audible again once that copy has finished. Its own
            // scene has no actors, so nothing else can cue a sound meanwhile.
            ZWeaponEffects windowEffects(toc, tables, program);
            CLevel windowScene(tables, program, enemies, player, vitals, windowEffects, loaded.playerTemplate->gameScale);
            windowScene.Reset();
            const GameObjectRef &repeated = batchDeathSounds.front();
            windowEffects.PlayMoveSound(repeated);
            const auto opened = windowEffects.GetSoundCueCount();
            windowScene.Update(16, 0, 0, false);
            windowEffects.PlayMoveSound(repeated);
            const auto covered = windowEffects.GetSoundCueCount();
            for (unsigned tick = 0; tick < 250; ++tick) { windowScene.Update(16, 0, 0, false); }
            windowEffects.PlayMoveSound(repeated);
            const auto reopened = windowEffects.GetSoundCueCount();
            if (opened != 1 || covered != 1 || reopened != 2) { ++checkFailures; }
            std::printf("[audio-health-check] move-window wav=%08x:%u first=%zu covered=%zu after-4s=%zu failures=%u\n",
                repeated.packHash, repeated.localIndex, opened, covered, reopened, checkFailures);
            // Gun-style cues keep their rate but never stack: every repeat
            // restarts the one voice that WAV is allowed, so a fast weapon
            // stays at the level its WAV was authored at.
            if (effectSound.IsNull()) {
                // Any real SOUNDEFFECT entry will do; take the first that
                // resolves to a WAV rather than inventing a resource.
                const auto corePack = toc.GetPack(toc.GetCorePackIndex())->GetPackHash();
                const unsigned soundCount = tables.GetObjectPack(toc.GetCorePackIndex()).GetObjectCount(ZGameSection::SoundEffect);
                for (unsigned index = 0; index < soundCount && effectSound.IsNull(); ++index) {
                    std::vector<std::uint8_t> bytes;
                    if (!tables.ReadSectionResource(corePack, ZGameSection::SoundEffect, index, bytes)) { continue; }
                    CArrayInputStream input(bytes);
                    CGameAssetRef wav;
                    wav.Init(input);
                    if (input.Overran() || wav.assetId < 0) { continue; }
                    if (!tables.ReadSectionResource(wav.packHash, ZGameSection::Wav, wav.assetId, bytes)) { continue; }
                    effectSound.packHash = corePack;
                    effectSound.localIndex = static_cast<std::uint8_t>(index);
                }
            }
            if (!effectSound.IsNull()) {
                ZGunCue sound;
                sound.kind = ZGunCue::Kind::Sound;
                sound.resource = effectSound;
                const auto before = windowEffects.GetSoundCueCount();
                unsigned peakVoices = 0;
                for (unsigned tick = 0; tick < 5; ++tick) {
                    windowEffects.Emit(sound, 600, 450, 0, 0, kPlayerCombatId);
                    peakVoices = std::max(peakVoices, windowEffects.GetVoiceCount());
                    windowScene.Update(16, 0, 0, false);
                }
                const auto retriggers = windowEffects.GetSoundCueCount() - before;
                if (retriggers != 5 || peakVoices > 1) { ++checkFailures; }
                std::printf("[audio-health-check] one-voice sound=%08x:%u retriggers=%zu peak-voices=%u failures=%u\n",
                    effectSound.packHash, effectSound.localIndex, retriggers, peakVoices, checkFailures);
            }
        }
        if (deathWavs.size() != 2 || deathWavs[0] == deathWavs[1]) { ++checkFailures; }
        else { checkFailures += backendAudio.CheckSilentPlayback(deathWavs[0], deathWavs[1]); }
        if (simultaneousHits == 0) { ++checkFailures; }
        std::printf("[feedback-hit] simultaneous-templates=%u failures=%u\n", simultaneousHits, checkFailures);
        std::printf("[feedback-check] failures=%u\n", checkFailures);
        // Capture through the production map/HUD draw, centred on the spire.
        session.Restart(startX, startY, startFacing);
        for (unsigned tick = 0; tick < 300; ++tick) { session.Update(16, 0, 0, false); }
        for (const ZPlacedProp &prop : loaded.props) {
            if (!prop.active || prop.sprite->interactiveKind != ZInteractivePropKind::Spire) { continue; }
            scene.GetPlayer().x = prop.x + 150;
            scene.GetPlayer().y = prop.y - 30;
            for (std::size_t index = 0; index < enemies.size(); ++index) {
                // ENEMY20's actual states 4/6 show/hide the bar. ENEMY3's
                // actual spawn export hides it. Do not write variable 15 here.
                if (enemies[index].packHash == CStringToKey("pack1") && enemies[index].ordinal == 3) {
                    const auto before = scene.EnemyHealthBars().size();
                    ZCombatEnemy *hidden = scene.Spawn(index, prop.x + 270, prop.y - 80);
                    if (hidden == nullptr || hidden->model.enemy.combat.variables[15] != 0 || scene.EnemyHealthBars().size() != before) { ++checkFailures; }
                    std::printf("[audio-health-check] authored-hidden enemy=pack1:3 failures=%u\n", checkFailures);
                }
                if (enemies[index].packHash == CStringToKey("pack1") && enemies[index].ordinal == 20) {
                    ZCombatEnemy *target = scene.Spawn(index, prop.x + 270, prop.y + 80);
                    if (target != nullptr) {
                        const auto before = scene.EnemyHealthBars().size();
                        if (target->model.enemy.combat.variables[15] != 0) { ++checkFailures; }
                        target->model.enemy.SetState(4);
                        if (target->model.enemy.combat.variables[15] != 1 || scene.EnemyHealthBars().size() != before + 1) { ++checkFailures; }
                        target->model.enemy.SetState(6);
                        if (target->model.enemy.combat.variables[15] != 0 || scene.EnemyHealthBars().size() != before) { ++checkFailures; }
                        target->model.enemy.SetState(4);
                        std::printf("[audio-health-check] authored-toggle enemy=pack1:20 states=4/6 failures=%u\n", checkFailures);
                    }
                }
            }
            break;
        }
        capturePath = TestOutput::Path("healthbar-alignment.png");
        for (unsigned tick = 0; tick < 30; ++tick) { session.Update(16, 0, 0, false); }
    }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}
