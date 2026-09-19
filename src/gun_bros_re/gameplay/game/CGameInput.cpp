/** CGame session host implementation; original ownership follows game.cpp.
 * SDL/GL submission and borrowed desktop resources are host adaptations.
 */
#include "gun_bros_re/gameplay/game/CGameRuntime.h"
#include "gun_bros_re/host/ZGameKeys.h"
#include "gun_bros_re/debug/SurvivalDebug.h"
#include "gun_bros_re/debug/DebugKeys.h"
#include "gun_bros_re/cheats/CheatActions.h"
#include "gun_bros_re/debug/DebugMaps.h"
#include <ctime>

using namespace MapDetail;

int CGame::Session::ProcessInput() {
    survivalHud.AdvanceMenu(static_cast<unsigned>(frameTicks - menuTicks));
    menuTicks = frameTicks;

    for (std::string cheat = window.TakeCheatCode(); !cheat.empty(); cheat = window.TakeCheatCode()) {
        CombatCheatResult result;
        if (!ApplyCombatCheat(cheat, scene, vitals, powerups, session, gameContext, result, progressData, progress)) {
            return 1;
        }
        if (result.botShop && !brother.vitals.dead && !powerups.GetPowerup().IsPresentationActive() &&
            !peerPowerups.GetPowerup().IsPresentationActive()) {
            OpenShop(1, true);
        }
        if (result.botPowerup && !liveShop.Active() && !powerups.GetPowerup().IsPresentationActive()) {
            localBot->UsePowerup(peerPowerups, true);
        }
        if (result.challengesUpdated && !gameContext->tutorial && !launch.deathmatch) {
            // Discard the old day's pending wave deltas before binding the new list.
            scene.TakeChallengeKills();
            scene.TakeChallengePowerups();
            if (!challenges.Bind(toc, tables, gameContext->profile, static_cast<unsigned>(std::time(nullptr)))) {
                return 1;
            }
            session.SetChallenges(&challenges, &gameContext->profile, &weapons);
            survivalHud.SetChallenges(&challenges);
            if (!session.SubmitChallenges(false)) { return 1; }
        }
        if (result.resume) {
            paused = false;
            shopOpen = false;
            itemChoice = false;
            liveShop.Close(liveShop.Owner());
            deathShop = false;
        }
        if (result.resetClock) {
            scene.SetPaused(paused || shopOpen);
            accumulator = 0;
            previous = window.GetTicksMs();
        }
    }

    if (gameContext && gameContext->profile.nativeArchive && !gameContext->tutorial && !launch.deathmatch &&
        GameHostSettings().isConnected && challenges.current.empty()) {
        // Enabling the connection during combat establishes the same local clock.
        scene.TakeChallengeKills();
        scene.TakeChallengePowerups();
        if (!challenges.InitProgressData(toc, tables, gameContext->profile,
                                         static_cast<unsigned>(std::time(nullptr)))) {
            return 1;
        }
        session.SetChallenges(&challenges, &gameContext->profile, &weapons);
        survivalHud.SetChallenges(&challenges);
        if (!session.SubmitChallenges(false) || !gameContext->SaveProfile()) { return 1; }
    }

    int inputWidth = 0, inputHeight = 0;
    window.GetDrawableSize(inputWidth, inputHeight);
    float inputX = -1, inputY = -1;
    window.GetMousePosition(inputX, inputY);
    inputX *= 1024.0f / std::max(1, inputWidth);
    inputY *= 768.0f / std::max(1, inputHeight);
    const ZInputPadState inputState = BuildHudState();
    float menuScroll = window.TakeWheelDelta();
    int menuDragX = 0, menuDragY = 0;
    window.TakeDragDelta(menuDragX, menuDragY);
    survivalHud.ScrollMenuInput(inputState, menuScroll, float(menuDragX) * 1024 / inputWidth,
                                float(menuDragY) * 768 / inputHeight);
    bool pointerDown = window.IsLeftMouseDown();

    frame.inputState = inputState;
    frame.inputX = inputX;
    frame.inputY = inputY;
    frame.pointerDown = pointerDown;
    if (launch.observer != nullptr) {
        const int result = launch.observer->OnFrame(ZGameObserver::FramePhase::Pointer, frame);
        if (result >= 0) { return result; }
    }
    inputX = frame.inputX;
    inputY = frame.inputY;
    pointerDown = frame.pointerDown;

    hudOwnsPointer = survivalHud.CapturesPointer(inputState, inputX, inputY);
    ZInputPadAction action = survivalHud.Pointer(inputState, inputX, inputY, pointerDown);
    // Input-pad controls cannot interrupt the active powerup presentation.
    if (powerups.GetPowerup().IsPresentationActive() || peerPowerups.GetPowerup().IsPresentationActive()) {
        action = ZInputPadAction::None;
    }
    if (launch.deathmatch && session.IsFinished()) { action = ZInputPadAction::None; }
    if (action == ZInputPadAction::Exit) {
        // Surrender leaves a paused menu; the same BGM continues into results.
        music.SetPaused(false);
        music.SetVolume(1.0f);
        return -3;
    }
    if (action == ZInputPadAction::OpenShop) { OpenShop(0); }
    if (action == ZInputPadAction::CloseShop) { CloseShop(); }
    if (action == ZInputPadAction::CancelItem) { itemChoice = false; }
    const CStoreItem::Entry *shopItem = survivalHud.SelectedItem();
    if (launch.deathmatch && shopItem != nullptr && action == ZInputPadAction::SelectMatchGun) {
        const auto &gun = shopItem->data.objects.front().object;
        if (!scene.SelectMatchGun(0, survivalHud.MatchSelectionSlot(), gun)) { return 1; }
        survivalHud.AdvanceMatchSelection();
    }
    if (shopItem != nullptr && (action == ZInputPadAction::BuyItem || action == ZInputPadAction::SelectItem)) {
        const GameObjectRef &resource = shopItem->data.objects.front().object;
        if (action == ZInputPadAction::BuyItem) {
            const CProfileManager::PurchaseResult result = pickupProfile->AcquireItem(shopItem->data, progress.GetLevel());
            if (result == CProfileManager::PurchaseResult::Purchased && gameContext != nullptr && !gameContext->SaveProfile()) {
                return 1;
            }
            // CPowerUpSelector::OnPurchase :184861 updates quantity in place.
            // An icon tap, not a purchase, enters the use/equip selection state.
            itemChoice = false;
            survivalHud.ReportSelectorPurchase(result, inputState);
        } else {
            itemChoice = pickupProfile->GetPowerupCount(resource) > 0;
        }
    }
    if (shopItem != nullptr && itemChoice &&
        (action == ZInputPadAction::EquipLeft || action == ZInputPadAction::EquipRight ||
         action == ZInputPadAction::UseNow)) {
        const GameObjectRef &resource = shopItem->data.objects.front().object;
        if (action == ZInputPadAction::EquipLeft || action == ZInputPadAction::EquipRight) {
            unsigned slot = 0;
            if (action == ZInputPadAction::EquipRight) { slot = 1; }
            if (powerups.Equip(slot, resource)) {
                leftPowerup = powerups.GetEquipped(0);
                rightPowerup = powerups.GetEquipped(1);
                itemChoice = false;
                if (gameContext != nullptr && !gameContext->SaveProfile()) { return 1; }
            }
        }
        if (action == ZInputPadAction::UseNow) {
            if (powerups.SelectResource(resource) && powerups.UseSelected(true)) { CloseShop(); }
        }
    }
    if (action == ZInputPadAction::UseLeft && !paused && !shopOpen && !session.IsTransitioning()) {
        if (powerups.SelectResource(leftPowerup)) { powerups.UseSelected(); }
    }
    if (action == ZInputPadAction::Sound || action == ZInputPadAction::Music ||
        action == ZInputPadAction::DockedSticks) {
        if (action == ZInputPadAction::Sound) { pickupProfile->soundEnabled = !pickupProfile->soundEnabled; }
        if (action == ZInputPadAction::Music) { pickupProfile->musicEnabled = !pickupProfile->musicEnabled; }
        if (action == ZInputPadAction::DockedSticks) { pickupProfile->options.ToggleDockedSticks(); }
        music.SetEnabled(pickupProfile->musicEnabled);
        ZAudioPlayer::SetEffectsEnabled(pickupProfile->soundEnabled);
        if (gameContext != nullptr && !gameContext->SaveProfile()) { return 1; }
    }
    auto &inputs = frame.inputs;
    inputs.clear();
    auto &actions = frame.actions;
    actions.clear();
    // Buttons submit actions directly; no synthetic F/R/G keycodes.
    if (action == ZInputPadAction::Resume || action == ZInputPadAction::Continue) { action = ZInputPadAction::Pause; }
    if (action == ZInputPadAction::Weapon1) { actions.push_back(ZInputPadAction::OpenShop); }
    if (action == ZInputPadAction::Weapon2) { action = ZInputPadAction::SwapWeapon; }
    if (action == ZInputPadAction::Pause || action == ZInputPadAction::Retry || action == ZInputPadAction::UseItem ||
        action == ZInputPadAction::NextItem || action == ZInputPadAction::SwapWeapon) {
        actions.push_back(action);
    }
    for (ZKeyCode key = window.TakeKeyPress(); key != ZKeyCode::None; key = window.TakeKeyPress()) {
        if (launch.deathmatch && session.IsFinished()) { continue; }
        // Replay escape bypasses pause, dialogs, death and powerup movies.
        if (gameContext != nullptr && gameContext->debugTutorial && key == ZGameKeys::Back) {
            std::printf("[debug-tutorial] escape no-save=1\n");
            return 0;
        }
        if (HandleDebugKey(key, window, showCollisions)) { continue; }
        if (launch.debugSelection != nullptr && GameDebugKeys::OpensMapBrowser(key, window)) {
            music.SetPaused(true);
            const bool selected = ShowDebugMapPicker(toc, tables, window, *launch.debugSelection);
            music.SetPaused(paused);
            previous = window.GetTicksMs();
            menuTicks = previous;
            accumulator = 0;
            inputs.clear();
            actions.clear();
            if (selected) {
                if (!session.SubmitChallenges(true)) { return 1; }
                if (!CGame::SaveProgress(gameContext, progress, session.GetLevel(), accountedXplodium)) { return 1; }
                return kDebugMapSessionChoice;
            }
            continue;
        }
        if (launch.debugMap != nullptr && key == GameDebugKeys::MapBack) { return 0; }
        ZGameKeys::AppendShortcut(inputs, key);
    }

    if (launch.observer != nullptr) {
        const int result = launch.observer->OnFrame(ZGameObserver::FramePhase::Keys, frame);
        if (result >= 0) { return result; }
    }

    for (ZKeyCode key : inputs) {
        const auto mapped = ZGameKeys::Action(key);
        if (mapped != ZInputPadAction::None) { actions.push_back(mapped); }
    }
    for (ZInputPadAction command : actions) {
        if ((launch.localLive || launch.deathmatch) && liveShop.Active() && liveShop.Owner() == 1) { continue; }
        if (powerups.GetPowerup().IsPresentationActive() || peerPowerups.GetPowerup().IsPresentationActive()) {
            continue;
        }
        // The original death script hides the input pad. Do not open an
        // invisible pause menu while the formal death animation is running.
        if (vitals.dead && gameContext != nullptr && !shopOpen && !launch.deathmatch) { continue; }
        if (shopOpen) {
            if (command == ZInputPadAction::Pause) {
                if (survivalHud.BackFromSelectorPrompt()) { continue; }
                if (itemChoice) {
                    itemChoice = false;
                } else {
                    CloseShop();
                }
            }
            continue;
        }
        if (command == ZInputPadAction::Pause) {
            if (!paused || !survivalHud.BackFromHelp()) { paused = !paused; }
        }
        if ((command == ZInputPadAction::UseLeft || command == ZInputPadAction::UseItem) && !paused && !vitals.dead &&
            !session.IsTransitioning()) {
            GameObjectRef item = rightPowerup;
            if (command == ZInputPadAction::UseLeft) { item = leftPowerup; }
            if (powerups.SelectResource(item)) { powerups.UseSelected(); }
            continue;
        }
        if (command == ZInputPadAction::OpenShop && gameContext != nullptr && !paused && !vitals.dead &&
            !session.IsTransitioning()) {
            OpenShop(0);
            continue;
        }
        if (command == ZInputPadAction::NextItem) {
            powerups.SelectResource(rightPowerup);
            powerups.Cycle();
            if (powerups.GetSelected() != nullptr) { rightPowerup = powerups.GetSelected()->resource; }
        }
        if (command == ZInputPadAction::Retry) {
            pendingWeapon = weapons.size();
            swapEventAccepted = false;
            if (!session.SubmitChallenges(true)) { return 1; }
            if (!CGame::SaveProgress(gameContext, progress, session.GetLevel(), accountedXplodium)) { return 1; }
            session.Restart(startX, startY, startFacing);
            accountedXplodium = 0;
            if (launch.deathmatch && gameContext != nullptr) {
                gameContext->accountedPeerKills = 0;
                gameContext->accountedPeerXplodium = 0;
            }
            if (gameContext != nullptr) {
                gameContext->accountedKills = 0;
                gameContext->accountedWeaponExperience.clear();
            }
            liveShop = {};
            botShop = {};
            deathShopSerial = UINT32_MAX;
            deathShop = false;
            if (!session.HasHud()) { survivalHud.ResetNotices(); }
            savedDeath = false;
            paused = false;
            shopOpen = false;
            itemChoice = false;
        }
        std::size_t nextWeapon = weaponSlot;
        if (launch.deathmatch) {
            if (!paused && !vitals.dead && (command == ZInputPadAction::SwapWeapon)) {
                scene.RequestMatchWeaponSwap(0);
            }
            continue;
        }
        if (gameContext != nullptr && !paused && !vitals.dead && pendingWeapon >= weapons.size() &&
            (command == ZInputPadAction::SwapWeapon)) {
            pendingEquippedSlot = 1 - equippedWeaponSlot;
            const GameObjectRef &ref = gameContext->profile.configuration.guns[pendingEquippedSlot];
            for (std::size_t index = 0; index < weapons.size(); ++index) {
                if (weapons[index].packHash == ref.packHash && weapons[index].ordinal == ref.localIndex) {
                    nextWeapon = index;
                    break;
                }
            }
        }
        if (nextWeapon != weaponSlot && !vitals.dead && gameContext != nullptr) {
            // Load both authored guns before starting the lowering move.
            // Keep the brother, torso controller, health and effects alive.
            if (player.uiOtherWeapon == nullptr) {
                primaryEquippedSlot = equippedWeaponSlot;
                if (!player.PrepareSecondaryWeapon(tables, weapons[nextWeapon].data, weapons[nextWeapon].owner) ||
                    !player.CreateBuffers(program)) {
                    return 1;
                }
            }
            pendingWeapon = nextWeapon;
            swapEventAccepted = false;
            continue;
        }
        if (gameContext != nullptr && !vitals.dead) { gameContext->profile.activeWeaponSlot = equippedWeaponSlot; }
    }

    if (launch.observer != nullptr) {
        const int result = launch.observer->OnFrame(ZGameObserver::FramePhase::AfterKeys, frame);
        if (result >= 0) { return result; }
    }

    return -1;
}
