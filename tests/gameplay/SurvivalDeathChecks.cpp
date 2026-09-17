#include "gun_bros_re/debug/Capture.h"
#include "gameplay/SurvivalChecks.h"
#include "TestOutput.h"
using namespace MapDetail;

int CheckSurvivalDeath(SurvivalDeathFixture fixture) {
    auto & checkFailures = fixture.checkFailures;
    auto & packShortName = fixture.packShortName;
    auto & deathStudy = fixture.deathStudy;
    auto & vitals = fixture.vitals;
    auto & window = fixture.window;
    auto & program = fixture.program;
    auto & batch = fixture.batch;
    auto & loaded = fixture.loaded;
    auto & player = fixture.player;
    auto & scene = fixture.scene;
    auto & brother = fixture.brother;
    auto & brotherModel = fixture.brotherModel;
    auto & session = fixture.session;
    auto & startX = fixture.startX;
    auto & startY = fixture.startY;
    auto & startFacing = fixture.startFacing;

    if (deathStudy) {
        // Fixtures use original actors and resources. Only input and fixed time
        // are supplied by this check; production exits through the same gate.
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        const auto captureDeath = [&](const std::string &suffix) {
            loaded.players[0].x = scene.GetPlayer().x;
            loaded.players[0].y = scene.GetPlayer().y;
            loaded.players[0].facingDegrees = scene.GetPlayer().facing;
            const float zoom = GameViewCameraZoom(width, height);
            float mvp[kMatrix4dElements];
            Matrix4dOrthoTopLeft(width / zoom, height / zoom, kMapDepthRange, mvp);
            Matrix4dTranslate(mvp, -scene.GetPlayer().x + width / zoom / 2, -scene.GetPlayer().y + height / zoom / 2);
            glViewport(0, 0, width, height);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            BuildGeometry(loaded, batch, true, true, false);
            batch.Draw(program, mvp);
            DrawMapObjects(loaded, batch, program, mvp, true, &scene, &brotherModel, brother.y, width);
            return Capture::SaveFrame(window, TestOutput::Path("player-death-") + packShortName + "-" + suffix + ".png");
        };
        for (unsigned scenario = 0; scenario < 2; ++scenario) {
            session.Restart(startX, startY, startFacing);
            vitals.invincible = true;
            brother.vitals.invincible = true;
            // Finish the real intro so it cannot alter the scale under test.
            for (int elapsed = 0; elapsed < 5000; elapsed += 16) { session.Update(16, 0, 0, false); }
            if (scenario == 0) {
                vitals.invincible = false;
                ZCombatHit fatal;
                fatal.ownerType = 1;
                fatal.damage = 10000;
                if (scene.ApplyHit(kPlayerCombatId, fatal) != ZHitResult::Killed) { ++checkFailures; }
                if (vitals.flash != 1) { ++checkFailures; }
                if (scene.ApplyHit(kPlayerCombatId, fatal) != ZHitResult::Ignored) { ++checkFailures; }
            } else {
                // Includes an autorepeated last letter, which must not complete.
                for (char letter : std::string("stsuicid")) {
                    if (!PushBossCheckKey(window, letter)) { return 1; }
                }
                if (!PushBossCheckKey(window, 'e', true) || !window.TakeCheatCode().empty()) { ++checkFailures; }
                if (!PushBossCheckKey(window, 'e') || window.TakeCheatCode() != "stsuicide" || !scene.Suicide()) { ++checkFailures; }
                if (!window.TakeCheatCode().empty() || window.IsKeyDown(ZKeyCode::S) || window.IsKeyDown(ZKeyCode::E)) { ++checkFailures; }
                for (ZKeyCode key = window.TakeKeyPress(); key != ZKeyCode::None; key = window.TakeKeyPress()) {
                    if (key == ZKeyCode::E || key == ZKeyCode::C) { ++checkFailures; }
                }
            }
            if (!vitals.dead || vitals.deaths != 1 || session.IsDeathComplete() || !vitals.inputHidden ||
                std::abs(session.GetLevel().GetWorldTimeScale() - 76 / 256.0f) > 0.0001f) { ++checkFailures; }
            if (scene.Suicide() || vitals.deaths != 1) { ++checkFailures; }
            auto &torso = player.weapon->brother.GetTorso();
            const int startTime = torso.GetAnimation().GetTimeMs();
            const int duration = torso.GetAnimation().GetRangeDurationMs();
            const auto &move = torso.GetMoveSet()->GetMoves()[torso.GetMoveIndex()];
            const float deathX = scene.GetPlayer().x, deathY = scene.GetPlayer().y;
            if (duration <= 0 || player.weapon->brother.TorsoUsesWeapon()) { ++checkFailures; }
            if (scenario == 1) {
                // Native perturbation proves there is no fixed host death delay.
                const std::int16_t scale = 128;
                session.GetLevel().FunctionResolver(5, &scale, 1);
            }
            const int step = session.GetLevel().TransformWorldElapseMS(16);
            const int moveStep = std::max(1, static_cast<int>(step * move.speed + 0.5f));
            int elapsed = 0;
            int animationElapsed = 0;
            bool savedMiddle = false;
            if (scenario == 0 && !captureDeath("start")) { ++checkFailures; }
            while (!session.IsDeathComplete() && elapsed < 20000) {
                session.Update(16, 1, 1, true);
                // Other actors keep shooting during death. Inspect ownership,
                // not the scene-wide shot counter (pack2's enemies fire here).
                for (const auto &shot : scene.GetProjectileStates()) {
                    if (shot.owner == kPlayerCombatId) { ++checkFailures; }
                }
                elapsed += 16;
                animationElapsed += moveStep;
                // Use scaled world time: death slow motion also slows the fade.
                if (scenario == 0) {
                    const float expectedFlash = std::max(0.0f, 1.0f - (elapsed / 16) * step * 0.002f);
                    if (std::abs(vitals.flash - expectedFlash) > 0.0001f) { ++checkFailures; }
                }
                if (animationElapsed < duration && session.IsDeathComplete()) { ++checkFailures; }
                if (!session.IsDeathComplete() && torso.GetAnimation().GetTimeMs() != startTime + animationElapsed) { ++checkFailures; }
                if (!savedMiddle && animationElapsed >= duration / 2) {
                    if (scenario == 0 && !captureDeath("middle")) { ++checkFailures; }
                    savedMiddle = true;
                }
            }
            if (!session.IsDeathComplete() || elapsed <= duration || !savedMiddle ||
                scene.GetPlayer().x != deathX || scene.GetPlayer().y != deathY) { ++checkFailures; }
            // The original UpdateNormal fades hit red while the death Flow runs.
            std::printf("[death-check] %s scenario=%u player-corpse-flash=%.3f\n",
                packShortName.c_str(), scenario, vitals.flash);
            if (vitals.flash != 0) { ++checkFailures; }
            if (scenario == 0 && !captureDeath("complete")) { ++checkFailures; }
            std::printf("[death-check] %s scenario=%u range-ms=%d speed=%.3f step=%d wall-ms=%d complete=%d failures=%u\n",
                packShortName.c_str(), scenario, duration, move.speed, step, elapsed, session.IsDeathComplete(), checkFailures);
        }
        session.Restart(startX, startY, startFacing);
        if (vitals.dead || vitals.deathAnimationComplete || vitals.inputHidden || session.GetLevel().GetWorldTimeScale() != 1) { ++checkFailures; }
        brother.vitals.invincible = false;
        ZCombatHit fatal;
        fatal.ownerType = 1;
        fatal.damage = 10000;
        scene.ApplyHit(kBrotherCombatId, fatal);
        if (brother.vitals.flash != 1) { ++checkFailures; }
        AdvancePlayer(brotherModel, 250);
        if (std::abs(brother.vitals.flash - 0.5f) > 0.0001f) { ++checkFailures; }
        for (int elapsed = 0; elapsed < 8000; elapsed += 16) { AdvancePlayer(brotherModel, 16); }
        if (!brother.vitals.deathAnimationComplete || session.IsDeathComplete() || session.GetLevel().GetWorldTimeScale() != 1) { ++checkFailures; }
        std::printf("[death-check] %s brother-corpse-flash=%.3f\n",
            packShortName.c_str(), brother.vitals.flash);
        if (brother.vitals.flash != 0) { ++checkFailures; }
        brotherModel.weapon->brother.OnWaveCleared();
        for (int elapsed = 0; elapsed < 8000; elapsed += 16) { AdvancePlayer(brotherModel, 16); }
        if (brother.vitals.dead || brother.vitals.deathAnimationComplete) { ++checkFailures; }
        std::printf("[death-check] %s restart/brother failures=%u\n", packShortName.c_str(), checkFailures);
        if (checkFailures != 0) { return 1; }
        return 0;
    }
    return -1; // Continue the same session; 0/1 retain the original check exit semantics.
}
