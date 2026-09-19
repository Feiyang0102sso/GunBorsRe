/** CGame session host implementation; original ownership follows game.cpp.
 * SDL/GL submission and borrowed desktop resources are host adaptations.
 */
#include "gun_bros_re/gameplay/game/CGameRuntime.h"
#include "gun_bros_re/debug/SurvivalDebug.h"
#include "gun_bros_re/debug/CollisionOverlay.h"
#include "gun_bros_re/gameplay/map/CRenderQueue.h"
using namespace MapDetail;

int CGame::Session::Draw() {
    loaded.DrawBackground(batch, true, false);
    if (const int result = Notify(ZGameObserver::FramePhase::GeometryDrawn); result >= 0) { return result; }
    glViewport(0, 0, width, height);
    glClearColor(0.04f, 0.05f, 0.07f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    float mvp[kMatrix4dElements];
    Matrix4dOrthoTopLeft(width / camera.zoom, height / camera.zoom, kMapDepthRange, mvp);
    Matrix4dTranslate(mvp, -camera.x, -camera.y);
    batch.Draw(program, mvp);
    scene.DrawPickups(mvp, kLevelCameraScale);
    // Historical explanation of the old separate model pass:
    // The AI brother is a 3D model like the player and the enemies: with no
    // depth test his torso, legs and gun paint over each other in submission
    // order and the model's dark inside covers its front -- the black
    // speckles. DrawModels already cleared depth and drew the player, so this
    // shares the same depth buffer.
    // Correction: the shared queue now clears depth per model and sorts
    // BOTH brothers and enemies among props, preserving internal depth.
    CBrother *drawBrother = nullptr;
    if (withBrother) { drawBrother = &brotherModel; }
    CRenderQueue::Draw(loaded, batch, program, mvp, true, &scene, drawBrother, brother.y, width);
    scene.Draw(mvp, nullptr, kLevelCameraScale, true);

    if (launch.observer != nullptr) {
        const int result = launch.observer->OnStage(ZGameObserver::Stage::WorldDrawn, *this);
        if (result >= 0) { return result; }
    }

    if (showCollisions) {
        const CBrotherAI *collisionBrother = nullptr;
        if (withBrother) { collisionBrother = &brother; }
        DrawCollisionOverlay(markers, markerProgram, mvp, 1 / camera.zoom, &loaded, &scene, collisionBrother, &scene);
    }
    if (const int result = Notify(ZGameObserver::FramePhase::WorldDrawn); result >= 0) { return result; }
    ZInputPadState hudState = BuildHudState();
    // CCamera constructor :64622: viewport factor is independent of zoom.
    const float healthBarViewportScale = std::min(width / 480.0f, height / 320.0f);
    hudState.enemyHealthBars = scene.EnemyHealthBars(healthBarViewportScale);
    ProjectEnemyHealthBars(hudState.enemyHealthBars, camera.x, camera.y, camera.zoom, width, height);
    hudState.indicators = session.GetLevel().GetIndicators();
    scene.UpdatePeerIndicator(menuElapsed, camera.x, camera.y, width / camera.zoom, height / camera.zoom);
    if (scene.PeerIndicator() != nullptr) { hudState.indicators.push_back(*scene.PeerIndicator()); }
    for (CLevelIndicator &indicator : hudState.indicators) {
        indicator.x = (indicator.x - camera.x) * camera.zoom * 1024 / width;
        indicator.y = (indicator.y - camera.y) * camera.zoom * 768 / height;
    }
    if (withBrother) {
        // CLevel::Bind :121881 leaves this empty for the default local
        // partner; only a selected friend or multiplayer peer supplies a name.
        // CLevel::DrawBrotherLabel :120351 anchors three collision radii
        // above the AI's world position and follows the level's alpha.
        hudState.brotherLabelX = (brother.x - camera.x) * camera.zoom * 1024 / width;
        hudState.brotherLabelY = (brother.y - scene.GetPlayerRadius() * 3 - camera.y) * camera.zoom * 768 / height;
        hudState.brotherLabelAlpha = session.GetLevel().GetBrotherLabelAlpha();
        if (launch.localLive || launch.localBot || launch.deathmatch) {
            hudState.brotherName = brotherName;
            hudState.brotherLabelAlpha = 1;
        }
    }
    hudState.localLive = launch.localLive;
    hudState.deathmatch = launch.deathmatch;
    if (launch.deathmatch) {
        hudState.inputHidden = false;
        hudState.guns[0] = scene.MatchGun(0, 0);
        hudState.guns[1] = scene.MatchGun(0, 1);
        hudState.matchScore[0] = match.Score(0);
        hudState.matchScore[1] = match.Score(1);
        hudState.matchLimit = match.Data().killLimit;
        hudState.respawnMs = match.GetLife(0).respawnMs;
    }
    hudState.reviveProgress = scene.GetReviveProgress();
    if (launch.localLive && hudState.reviveProgress > 0) {
        float x = brother.x, y = brother.y;
        if (vitals.dead) {
            x = scene.GetPlayer().x;
            y = scene.GetPlayer().y;
        }
        // CBrother::GetBounds :134186 is a native 100x100 box at
        // (trunc(x)-50, trunc(y)-50), independent of mesh and weapon.
        const int barWidth = static_cast<int>(30 * healthBarViewportScale);
        const int barHeight = static_cast<int>(4 * healthBarViewportScale);
        const int padding = static_cast<int>(healthBarViewportScale);
        hudState.reviveBar.width = float(barWidth) * 1024 / width;
        hudState.reviveBar.height = float(barHeight) * 768 / height;
        hudState.revivePaddingX = float(padding) * 1024 / width;
        hudState.revivePaddingY = float(padding) * 768 / height;
        hudState.reviveBar.x = (static_cast<int>(x) - barWidth / 2 - camera.x) * camera.zoom * 1024 / width;
        hudState.reviveBar.y = (static_cast<int>(y) - 50 - camera.y) * camera.zoom * 768 / height;
    }

    hudState.playerX = scene.GetPlayer().x;
    hudState.playerY = scene.GetPlayer().y;
    hudState.damageDealt = scene.damageDealt;
    const float moveLength = std::max(1.0f, std::hypot(frame.moveX, frame.moveY));
    hudState.moveX = frame.moveX / moveLength;
    hudState.moveY = frame.moveY / moveLength;
    if (window.IsLeftMouseDown() && !hudOwnsPointer) {
        hudState.aimX = std::sin(scene.GetPlayer().facing / kRadiansToDegrees);
        hudState.aimY = -std::cos(scene.GetPlayer().facing / kRadiansToDegrees);
    }
    if (!survivalHud.DrawExperienceTexts(scene.GetExperienceTexts(), horde) || !survivalHud.Draw(hudState)) {
        return 1;
    }
    if (!powerups.GetPowerup().Draw() || (launch.localLive && !peerPowerups.GetPowerup().Draw())) { return 1; }
    if (gameContext != nullptr && gameContext->debugTutorial &&
        !survivalHud.DrawTutorialDebugNotice(window.GetTicksMs())) {
        return 1;
    }

    if (launch.observer != nullptr) {
        const int result = launch.observer->OnFrame(ZGameObserver::FramePhase::Drawn, frame);
        if (result >= 0) { return result; }
        if (result == -2) { return -2; }
    }

    return -1;
}
