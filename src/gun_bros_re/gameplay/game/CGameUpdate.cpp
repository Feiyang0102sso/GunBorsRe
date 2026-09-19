/** CGame session host implementation; original ownership follows game.cpp.
 * SDL/GL submission and borrowed desktop resources are host adaptations.
 */
#include "gun_bros_re/gameplay/game/CGameRuntime.h"
#include "gun_bros_re/host/ZGameKeys.h"
#include "gun_bros_re/debug/DebugMaps.h"
using namespace MapDetail;

int CGame::Session::Update() {
    window.GetDrawableSize(width, height);
    loaded.GetResources().players[0].x = scene.GetPlayer().x;
    loaded.GetResources().players[0].y = scene.GetPlayer().y;
    baselineZoom = GameViewCameraZoom(width, height);
    camera.zoom = baselineZoom * loaded.GetCamera().GetScale() / kLevelCameraScale;
    session.SetViewSize(width / baselineZoom, height / baselineZoom);
    FollowPlayerCamera(loaded, width, height, camera);
    scene.SetViewCenter(camera.x + width / camera.zoom * 0.5f, camera.y + height / camera.zoom * 0.5f);
    scene.SetTextView(camera.x, camera.y, camera.zoom * 1024 / width, camera.zoom * 768 / height);
    float mouseX = 0, mouseY = 0;
    if (options.mouseAim && window.GetMousePosition(mouseX, mouseY) && !vitals.dead &&
        !powerups.GetPowerup().IsPresentationActive() && !peerPowerups.GetPowerup().IsPresentationActive()) {
        scene.GetPlayer().facing = std::atan2(camera.y + mouseY / camera.zoom - scene.GetPlayer().y,
                                              camera.x + mouseX / camera.zoom - scene.GetPlayer().x) *
                                       kRadiansToDegrees +
                                   90;
    }
    frame.moveX = 0;
    frame.moveY = 0;
    float &moveX = frame.moveX;
    float &moveY = frame.moveY;
    if (window.IsKeyDown(ZGameKeys::MoveLeft)) { --moveX; }
    if (window.IsKeyDown(ZGameKeys::MoveRight)) { ++moveX; }
    if (window.IsKeyDown(ZGameKeys::MoveUp)) { --moveY; }
    if (window.IsKeyDown(ZGameKeys::MoveDown)) { ++moveY; }
    const std::uint64_t now = window.GetTicksMs();
    // Re-evaluate after input and cheats. No leftover simulation tick may
    // move either actor while either peer owns a visible selector.
    if (launch.localLive || launch.deathmatch) { shopOpen = liveShop.Visible(now); }
    // CPowerUpSelector::Show only suspends non-DM worlds (:1864xx).
    const bool worldPaused = paused || (shopOpen && !launch.deathmatch);
    scene.SetMatchShopping(0, shopOpen);
    scene.SetMatchShopping(1, botShop.Active());
    session.SetSuspended(worldPaused);
    if (worldPaused) { accumulator = 0; }
    if (!worldPaused && options.realtime) {
        accumulator += static_cast<int>(std::min<std::uint64_t>(now - previous, 100));
    }
    previous = now;

    scene.SetPaused(worldPaused || (launch.deathmatch && session.IsDeathmatchFading()));
    if (session.IsBossSkipActive()) {
        session.AdvanceBossSkip();
        accumulator = 0;
        previous = window.GetTicksMs();
    }
    // CGunBros::OnSuspend :78263 lowers BGM to half without stopping it.
    // Gameplay and effects stay suspended; the music stream keeps advancing.
    float musicScale = 1.0f;
    if (paused || shopOpen) { musicScale = 0.5f; }
    music.SetVolume(musicScale);
    music.Update();

    frame.forceFire = false;
    if (launch.observer != nullptr) {
        const int result = launch.observer->OnFrame(ZGameObserver::FramePhase::BeforeSimulation, frame);
        if (result >= 0) { return result; }
    }

    frame.updateSteps = 0;
    while (accumulator >= 16) {
        ++frame.updateSteps;
        if (powerups.GetPowerup().IsPresentationActive() || peerPowerups.GetPowerup().IsPresentationActive()) {
            session.Update(16, 0, 0, false);
            accumulator -= 16;
            continue;
        }
        if (!session.HasHud()) { survivalHud.Advance(16); }
        if (!vitals.dead && pendingWeapon < weapons.size() && !swapEventAccepted) {
            player.SetInput(false, false);
            swapEventAccepted = player.OnSwapGun();
        }
        if (!vitals.dead || launch.localLive || launch.deathmatch) {
            bool shoot =
                pendingWeapon >= weapons.size() && (frame.forceFire || (window.IsLeftMouseDown() && !hudOwnsPointer));
            if (frame.suppressFire) { shoot = false; }
            session.Update(16, moveX, moveY, shoot);
        } else {
            // CLevel::UpdateAfterDeath keeps an already-used powerup alive.
            session.UpdateAfterDeath(16);
        }
        if (pendingWeapon < weapons.size() && player.TakeWeaponSwap()) {
            scene.RetireOwner(Collision::Player);
            if (launch.observer != nullptr) {
                const int result = launch.observer->OnFrame(ZGameObserver::FramePhase::BeforeWeaponSwap, frame);
                if (result >= 0) { return result; }
            }
            player.SelectWeapon(pendingEquippedSlot == primaryEquippedSlot);

            if (launch.observer != nullptr) {
                const int result = launch.observer->OnFrame(ZGameObserver::FramePhase::AfterWeaponSwap, frame);
                if (result >= 0) { return result; }
            }

            weaponSlot = pendingWeapon;
            equippedWeaponSlot = pendingEquippedSlot;
            player.gunSlot = equippedWeaponSlot;
            player.gunResource.packHash = weapons[weaponSlot].packHash;
            player.gunResource.localIndex = static_cast<std::uint8_t>(weapons[weaponSlot].ordinal);
            player.masteryExperience = gameContext->profile.GetWeaponExperience(player.gunResource);
            player.ActiveWeapon().SetMasteryExperience(player.masteryExperience);
            gameContext->profile.activeWeaponSlot = equippedWeaponSlot;
            pendingWeapon = weapons.size();
        }
        if (launch.deathmatch) {
            const auto active = scene.ActiveMatchGun(0);
            for (std::size_t index = 0; index < weapons.size(); ++index) {
                if (weapons[index].packHash == active.packHash && weapons[index].ordinal == active.localIndex) {
                    weaponSlot = index;
                    break;
                }
            }
            equippedWeaponSlot = player.gunSlot;
        }
        const int worldDeltaMs = session.GetLevel().TransformWorldElapseMS(16);
        if (!launch.deathmatch || !session.IsFinished()) { loaded.UpdateLayers(worldDeltaMs); }
        accumulator -= 16;
    }

    if (launch.observer != nullptr) {
        const int result = launch.observer->OnFrame(ZGameObserver::FramePhase::AfterSimulation, frame);
        if (result >= 0) { return result; }
    }

    if (session.GetLevel().GetWave() != lastSavedWave || (session.IsDeathComplete() && !savedDeath) ||
        session.GetLevel().GetTutorialStep() != lastSavedTutorialStep) {
        if (!CGame::SaveProgress(gameContext, progress, session.GetLevel(), accountedXplodium,
                                 session.IsDeathComplete())) {
            return 1;
        }
        lastSavedWave = session.GetLevel().GetWave();
        savedDeath = session.IsDeathComplete();
        lastSavedTutorialStep = session.GetLevel().GetTutorialStep();
    }
    // Gameplay death opens the original postgame flow; research keeps its
    // death/restart controls so existing isolated checks remain available.
    // The archive browser owns preview wrap-up instead of the retail menu.
    if (launch.debugMap != nullptr && session.GetLevel().IsCleared()) { return kDebugMapSessionComplete; }
    if (gameContext != nullptr && gameContext->debugTutorial && options.exitOnCompletion &&
        session.GetLevel().GetTutorialStep() == -1) {
        std::printf("[debug-tutorial] completed no-save=1\n");
        return 0;
    }
    if (session.IsReadyForResults() && gameContext != nullptr && options.exitOnCompletion) { return -3; }
    loaded.GetResources().players[0].x = scene.GetPlayer().x;
    loaded.GetResources().players[0].y = scene.GetPlayer().y;
    loaded.GetResources().players[0].facingDegrees = scene.GetPlayer().facing;
    camera.zoom = baselineZoom * loaded.GetCamera().GetScale() / kLevelCameraScale;
    FollowPlayerCamera(loaded, width, height, camera);
    return -1;
}
