#define NOMINMAX
#include "gameplay/ProfilePlayDriver.h"
#include "gun_bros_re/gameplay/game/CGame.h"
#include "gun_bros_re/gameplay/audio/CBGM.h"
#include "gun_bros_re/ui/hud/CInputPad.h"
#include "gun_bros_re/debug/Capture.h"
#include "gun_bros_re/debug/DebugKeys.h"
#include "gun_bros_re/host/ZGameKeys.h"
#include "TestOutput.h"
#include <cmath>
#include <cstdio>

ProfilePlayDriver::~ProfilePlayDriver() {
    if (mouseCheckStarted) { GameHostSettings().control = previousControl; }
}

int ProfilePlayDriver::CheckMouseFire(ZGameObserver::FramePhase phase, ZGameObserver::Frame &frame) {
    if (mouseControlMode == 1) { return CheckScreenFire(phase, frame); }
    if (phase == FramePhase::Begin) {
        frame.vitals.invincible = true;
        // Complete the authored intro before supplying ordinary mouse input.
        for (unsigned tick = 0; tick < 500; ++tick) { frame.session.Update(16, 0, 0, false); }
    }
    if (phase == FramePhase::Pointer) {
        if (!mouseCheckStarted) {
            previousControl = GameHostSettings().control;
            GameHostSettings().control = 2;
            mouseCheckStarted = true;
        }
        float x = 0, y = 0, radius = 0;
        if (!frame.hud.FireStickGeometry(x, y, radius)) { return 1; }
        frame.inputX = x;
        frame.inputY = y;
        frame.pointerDown = mouseFrame != 0 && mouseFrame != 5 && mouseFrame != 10;
        if (mouseFrame == 2 || mouseFrame >= 7) { frame.inputY -= radius * 2; }
        if (mouseFrame == 3) {
            ZMovieRegion button;
            if (!frame.hud.FindActionRegion(frame.inputState, ZInputPadAction::Pause, button)) { return 1; }
            frame.inputX = button.x + button.width / 2;
            frame.inputY = button.y + button.height / 2;
        }
        if (mouseFrame == 6) { frame.inputX = 512; frame.inputY = 384; }
        if (mouseFrame == 11) { frame.inputX = x; frame.inputY = y; }
        mouseExpectedFacing = std::atan2(frame.inputX - x, y - frame.inputY) * 180.0f / 3.14159265358979323846f;
    }
    if (phase == FramePhase::Keys) {
        // Keyboard pause must cancel an already captured drag in the same frame.
        if (mouseFrame == 13 || mouseFrame == 14) { frame.inputs.push_back(ZGameKeys::Pause); }
    }
    if (phase == FramePhase::BeforeSimulation) {
        frame.forceFire = false;
        frame.accumulator = 0;
        if (!frame.paused) { frame.accumulator = 960; }
        mouseShotsBefore = frame.scene.GetShotCount();
    }
    if (phase == FramePhase::AfterSimulation) {
        const bool shouldFire = mouseFrame == 2 || mouseFrame == 3 || mouseFrame == 12;
        const std::size_t shots = frame.scene.GetShotCount() - mouseShotsBefore;
        bool passed = true;
        if (shouldFire) {
            const float error = std::remainder(frame.scene.GetPlayer().facing - mouseExpectedFacing, 360.0f);
            // A wave can finish during these 960 ms and stop the weapon at
            // the final tick; verify shots and aim over the input interval.
            passed = shots > 0 && std::abs(error) < 0.1f && !frame.paused;
        } else if (frame.paused) {
            passed = shots == 0;
        } else {
            // GetShotCount includes enemy projectiles; inspect the player's gun
            // when checking release instead of treating enemy fire as input.
            passed = !frame.player.ActiveWeapon().IsShooting();
        }
        std::printf("[mouse-fire-session] step=%u shots=%zu player-firing=%d facing=%.2f expected=%.2f paused=%d passed=%d\n",
            mouseFrame, shots, frame.player.ActiveWeapon().IsShooting(), frame.scene.GetPlayer().facing, mouseExpectedFacing, frame.paused, passed);
        if (!passed) { ++checkFailures; }
    }
    if (phase == FramePhase::Drawn) {
        if (mouseFrame == 2 && !Capture::SaveFrame(frame.window, TestOutput::Path("mouse-stick-fire.png"))) { return 1; }
        ++mouseFrame;
        if (mouseFrame == 16) {
            GameHostSettings().control = previousControl;
            if (checkFailures != 0) { return 1; }
            return 0;
        }
        return -2;
    }
    return -1;
}

int ProfilePlayDriver::CheckScreenFire(ZGameObserver::FramePhase phase, ZGameObserver::Frame &frame) {
    if (phase == FramePhase::Begin) {
        previousControl = GameHostSettings().control;
        GameHostSettings().control = 1;
        mouseCheckStarted = true;
        frame.vitals.invincible = true;
        for (unsigned tick = 0; tick < 500; ++tick) { frame.session.Update(16, 0, 0, false); }
        screenInitialSlot = frame.equippedWeaponSlot;
        screenLeftCount = frame.profile->GetPowerupCount(frame.leftPowerup);
        screenRightCount = frame.profile->GetPowerupCount(frame.rightPowerup);
    }
    if (phase == FramePhase::Pointer) {
        frame.inputX = 512;
        frame.inputY = 384;
        frame.pointerDown = mouseFrame != 0 && mouseFrame != 7 && mouseFrame != 9 && mouseFrame != 10 &&
            mouseFrame != 11 && mouseFrame != 15 && mouseFrame != 17 && mouseFrame != 20 && mouseFrame != 22;
        const ZInputPadAction crossed[] = {ZInputPadAction::Pause, ZInputPadAction::OpenShop,
            ZInputPadAction::SwapWeapon, ZInputPadAction::UseLeft, ZInputPadAction::UseItem};
        ZInputPadAction target = ZInputPadAction::None;
        if (mouseFrame >= 2 && mouseFrame <= 6) { target = crossed[mouseFrame - 2]; }
        if (mouseFrame == 7) { target = ZInputPadAction::UseItem; }
        if (mouseFrame == 8 || mouseFrame == 9) { target = ZInputPadAction::OpenShop; }
        if (mouseFrame == 12 || (mouseFrame >= 14 && mouseFrame <= 17)) { target = ZInputPadAction::Pause; }
        if (target != ZInputPadAction::None) {
            ZMovieRegion region;
            if (!frame.hud.FindActionRegion(frame.inputState, target, region)) { return 1; }
            frame.inputX = region.x + region.width / 2;
            frame.inputY = region.y + region.height / 2;
        }
    }
    if (phase == FramePhase::Keys && (mouseFrame == 10 || mouseFrame == 18)) { frame.inputs.push_back(ZGameKeys::Pause); }
    if (phase == FramePhase::BeforeSimulation) {
        frame.accumulator = 160;
        if (frame.paused || frame.shopOpen) { frame.accumulator = 0; }
        frame.forceFire = false;
        mouseShotsBefore = frame.scene.GetShotCount();
    }
    if (phase == FramePhase::AfterSimulation) {
        const bool firing = (mouseFrame >= 1 && mouseFrame <= 6) || mouseFrame == 21;
        bool passed = frame.paused == (mouseFrame == 17) && frame.shopOpen == (mouseFrame == 9);
        // Actor hit reactions can suspend its gun while the mouse gesture
        // remains held. Assert the actual input consumed by CGame::Update.
        if (frame.pointerFire != firing) { passed = false; }
        unsigned expectedSlot = screenInitialSlot;
        if (frame.equippedWeaponSlot != expectedSlot ||
            frame.profile->GetPowerupCount(frame.leftPowerup) != screenLeftCount ||
            frame.profile->GetPowerupCount(frame.rightPowerup) != screenRightCount) { passed = false; }
        if (firing) { screenShots += frame.scene.GetShotCount() - mouseShotsBefore; }
        if (!passed) { ++checkFailures; }
        std::printf("[screen-fire-session] step=%u input=%d weapon=%d expected=%d paused=%d passed=%d\n",
            mouseFrame, frame.pointerFire, frame.player.ActiveWeapon().IsShooting(), firing, frame.paused, passed);
    }
    if (phase == FramePhase::Drawn) {
        if (mouseFrame == 3 && !Capture::SaveFrame(frame.window, TestOutput::Path("screen-fire-over-shop.png"))) { return 1; }
        ++mouseFrame;
        if (mouseFrame == 23) {
            GameHostSettings().control = previousControl;
            if (checkFailures != 0 || screenShots == 0) { return 1; }
            return 0;
        }
        return -2;
    }
    return -1;
}

int ProfilePlayDriver::OnFrame(ZGameObserver::FramePhase phase, ZGameObserver::Frame &frame) {
    if (mouseOnly) { return CheckMouseFire(phase, frame); }
    auto *pickupProfile = frame.profile;
    auto &survivalHud = frame.hud;
    auto &session = frame.session;
    auto &scene = frame.scene;
    auto &player = frame.player;
    auto &vitals = frame.vitals;
    auto &window = frame.window;
    auto &music = frame.music;
    auto &leftPowerup = frame.leftPowerup;
    auto &rightPowerup = frame.rightPowerup;
    auto &paused = frame.paused;
    auto &shopOpen = frame.shopOpen;
    auto &equippedWeaponSlot = frame.equippedWeaponSlot;
    auto &accumulator = frame.accumulator;
    auto &inputState = frame.inputState;
    auto &inputX = frame.inputX;
    auto &inputY = frame.inputY;
    auto &pointerDown = frame.pointerDown;
    auto &inputs = frame.inputs;
    // Run the real HUD hit tests and the same host action path. No OS input.
    constexpr ZInputPadAction controlActions[] = {ZInputPadAction::OpenShop, ZInputPadAction::CloseShop,
        ZInputPadAction::SwapWeapon, ZInputPadAction::Pause, ZInputPadAction::Resume};
    constexpr unsigned controlClickCount = sizeof(controlActions) / sizeof(controlActions[0]);
    // Test through the production key dispatcher after the mouse regression.
    const ZKeyCode controlKeys[] = {ZKeyCode::Digit1, ZKeyCode::Escape, ZKeyCode::Digit2,
        ZKeyCode::Q, ZKeyCode::None, ZKeyCode::E, ZKeyCode::F, ZKeyCode::R,
        ZKeyCode::G, ZKeyCode::N, ZKeyCode::M};
    const unsigned controlKeyCount = sizeof(controlKeys) / sizeof(controlKeys[0]);

    switch (phase) {
    case ZGameObserver::FramePhase::Begin: {
    controlFrame = 0;
    checkFailures = 0;
    combatSwapEvents = 0;
    // The keyboard fixture needs owned charges; selector purchases/equipping
    // are separately exercised by RunOriginalPowerupSelectorCheck.
    
    pickupProfile->AddPowerup(rightPowerup, 2);

    controlsInitialBucks = pickupProfile->warbucks;
    controlsInitialSound = pickupProfile->soundEnabled;
    controlsInitialGrenades = pickupProfile->GetPowerupCount(rightPowerup);
    controlsInitialLeft = leftPowerup;
    controlsBeforeKeys = 0;
    controlsLeftBeforeKeys = 0;
        break;
    }
    case ZGameObserver::FramePhase::Pointer: {
        if (controlFrame < controlClickCount) {
            if (controlClickPhase != 0) {
                inputX = controlClickX;
                inputY = controlClickY;
                pointerDown = controlClickPhase == 1;
                break;
            }
            // Bind and finish authored menu entrance before querying its live hitbox.
            if (!survivalHud.Draw(inputState)) { return 1; }
            survivalHud.AdvanceMenu(2000);
            if (!survivalHud.Draw(inputState)) { return 1; }
            ZMovieRegion target;
            if (!survivalHud.FindActionRegion(inputState, controlActions[controlFrame], target)) {
                std::printf("[combat-controls-check] missing original action=%d frame=%u\n", int(controlActions[controlFrame]), controlFrame);
                return 1;
            }
            inputX = target.x + target.width / 2;
            inputY = target.y + target.height / 2;
            survivalHud.Pointer(inputState, inputX, inputY, false);
            controlClickX = inputX;
            controlClickY = inputY;
            pointerDown = false;
        }
        break;
    }
    case ZGameObserver::FramePhase::Keys: {
        controlsWaveBeforeInput = session.GetLevel().GetWave();
        if (controlFrame >= controlClickCount && controlFrame < controlClickCount + controlKeyCount) {
            const unsigned step = controlFrame - controlClickCount;
            if (step == 0 || step == 4) {
                vitals.invincible = true;
                // Wait through real intro/cooldown updates; do not bypass Use().
                for (unsigned tick = 0; tick < 250; ++tick) { session.Update(16, 0, 0, false); }
            }
            if (step == 0) {
                // Distinct slots catch accidental Q -> right-item routing.
                leftPowerup = controlsInitialLeft;
                // The isolated legacy test account starts without this item.
                pickupProfile->AddPowerup(leftPowerup, 1);
                controlsLeftBeforeKeys = pickupProfile->GetPowerupCount(leftPowerup);
                controlsBeforeKeys = pickupProfile->GetPowerupCount(rightPowerup);
                vitals.health = 1;
            }
            ZGameKeys::AppendShortcut(inputs, controlKeys[step]);
        }
        break;
    }
    case ZGameObserver::FramePhase::AfterKeys: {
        if ((controlFrame == controlClickCount + 3 || controlFrame == controlClickCount + 5)) {
            // Throwing consumes inventory at the authored animation event.
            for (unsigned tick = 0; tick < 60; ++tick) { session.Update(16, 0, 0, false); }
        }
        break;
    }
    case ZGameObserver::FramePhase::BeforeSimulation: {
        if (controlFrame < controlClickCount && controlClickPhase < 2) {
            accumulator = 0;
            checkSwapFiring = false;
            break;
        }
        if (!paused && !shopOpen) { accumulator = 960; }
        if (controlFrame < controlClickCount + controlKeyCount) {
            const auto playback = music.GetPlaybackState();
            float expectedVolume = 0;
            if (pickupProfile->musicEnabled) {
                expectedVolume = 0.3f;
                if (paused || shopOpen) { expectedVolume *= 0.5f; }
            }
            if (playback.paused || std::abs(playback.volume - expectedVolume) > 0.001f) { ++checkFailures; }
            std::printf("[pause-bgm-check] frame=%u menu=%d paused-stream=%d gain=%.2f expected=%.2f failures=%u\n",
                controlFrame, paused || shopOpen, playback.paused, playback.volume, expectedVolume, checkFailures);
        }
        shotsBeforeSwap = scene.GetShotCount();
        checkSwapFiring = controlFrame == 2 || controlFrame == controlClickCount + 2;
        frame.forceFire = checkSwapFiring;

        break;
    }
    case ZGameObserver::FramePhase::AfterSimulation: {
        if (checkSwapFiring) {
            const std::size_t shots = scene.GetShotCount() - shotsBeforeSwap;
            if (shots == 0) { ++checkFailures; }
            std::printf("[combat-swap-check] fire-after-switch=%zu failures=%u\n", shots, checkFailures);
        }
        break;
    }
    case ZGameObserver::FramePhase::Drawn: {
        if (controlFrame < controlClickCount && controlClickPhase < 2) {
            ++controlClickPhase;
            return -2;
        }
        controlClickPhase = 0;
        if (controlFrame < controlClickCount) {
            if (controlFrame == 0 && !Capture::SaveFrame(window, TestOutput::Path("combat-controls-shop.png"))) { return 1; }
            if (controlFrame == 3 && !Capture::SaveFrame(window, TestOutput::Path("combat-controls-pause.png"))) { return 1; }
            if (controlFrame + 1 == controlClickCount) {
                const bool inventoryUnchanged = pickupProfile->warbucks == controlsInitialBucks &&
                    pickupProfile->GetPowerupCount(rightPowerup) == controlsInitialGrenades;
                const bool controlsPassed = inventoryUnchanged && equippedWeaponSlot == 1 &&
                    pickupProfile->soundEnabled == controlsInitialSound && !paused && !shopOpen;
                std::printf("[combat-controls-check] original-hitboxes=5 inventory-unchanged=%d weapon=%u resume=%d failures=%d\n",
                    inventoryUnchanged, equippedWeaponSlot, !paused && !shopOpen, !controlsPassed);
                if (!controlsPassed) { ++checkFailures; }
            }
        }

        if (controlFrame >= controlClickCount && controlFrame < controlClickCount + controlKeyCount) {
            const unsigned step = controlFrame - controlClickCount;
            bool passed = true;
            if (step == 0) { passed = shopOpen; }
            if (step == 1) { passed = !shopOpen && !paused; }
            if (step == 2) { passed = equippedWeaponSlot == 0 && combatSwapEvents == 2; }
            if (step == 3) { passed = equippedWeaponSlot == 0 && controlsLeftBeforeKeys > 0 &&
                pickupProfile->GetPowerupCount(leftPowerup) == controlsLeftBeforeKeys - 1 &&
                pickupProfile->GetPowerupCount(rightPowerup) == controlsBeforeKeys; }
            if (step == 5) { passed = equippedWeaponSlot == 0 && pickupProfile->GetPowerupCount(rightPowerup) == controlsBeforeKeys - 1; }
            if (step >= 6) { passed = inputs.empty() && equippedWeaponSlot == 0 &&
                session.GetLevel().GetWave() == controlsWaveBeforeInput && rightPowerup.localIndex == 13; }
            std::printf("[shortcut-check] step=%u key=%d passed=%d inventory=%u\n", step,
                static_cast<int>(controlKeys[step]), passed, pickupProfile->GetPowerupCount(rightPowerup));
            if (!passed) { ++checkFailures; }
        }

        if (controlFrame < controlClickCount + controlKeyCount) {
            ++controlFrame;
            if (controlFrame < controlClickCount + controlKeyCount) { return -2; }
        }

        if (checkFailures != 0) { return 1; }

        break;
    }
    case ZGameObserver::FramePhase::BeforeWeaponSwap: {
        const auto &torso = player.GetTorso().GetAnimation();
        outgoingMesh = torso.GetMesh();
        outgoingTime = torso.GetTimeMs();
        break;
    }
    case ZGameObserver::FramePhase::AfterWeaponSwap: {
        const auto &torso = player.GetTorso().GetAnimation();
        if (torso.GetMesh() != outgoingMesh || torso.GetTimeMs() != outgoingTime) { ++checkFailures; }
        ++combatSwapEvents;
        break;
    }
    }
    return -1;
}
