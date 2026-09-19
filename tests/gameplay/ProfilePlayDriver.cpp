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

int ProfilePlayDriver::OnFrame(ZGameObserver::FramePhase phase, ZGameObserver::Frame &frame) {
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
            pointerDown = true;
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
