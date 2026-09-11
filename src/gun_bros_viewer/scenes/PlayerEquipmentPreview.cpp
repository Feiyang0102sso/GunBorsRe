#include "gun_bros_viewer/ViewerControls.h"
#include "gun_bros_viewer/ViewerSettings.h"
#include "engine/core/Paths.h"
/**
 * @file PlayerEquipmentPreview.cpp
 * @brief Fixed-position equipment display using PlayerModel and WeaponEffects.
 * Catalog and camera helpers are shared with MeshPreview; all assembly,
 * animation and firing behavior stays in the existing game implementations.
 */

#define NOMINMAX
#include "gun_bros_viewer/scenes/MeshPreview.h"

#include "gun_bros_re/data/PackTables.h"
#include "gun_bros_re/gameplay/PlayerModel.h"
#include "gun_bros_re/gameplay/EnemyModel.h"
#include "gun_bros_re/data/ArmorCatalog.h"
#include "gun_bros_re/data/WeaponCatalog.h"
#include "gun_bros_re/data/StoreCatalog.h"
#include "gun_bros_re/gameplay/CombatGeometry.h"
#include "gun_bros_re/gameplay/WeaponEffects.h"
#include "gun_bros_re/gameplay/CParticleEffect.h"

#include "engine/resources/CArrayInputStream.h"
#include "engine/core/CMatrix4d.h"
#include "engine/graphics/CMeshBuffer.h"
#include "engine/graphics/CPNG.h"
#include "engine/graphics/CShaderProgram.h"
#include "engine/graphics/CTexture.h"
#include "engine/platform/CWindow.h"
#include "engine/platform/GLLoader.h"
#include "engine/glu/script/CScript.h"
#include "gun_bros_re/gameplay/CArmor.h"
#include "gun_bros_re/gameplay/CBrother.h"
#include "gun_bros_re/gameplay/CBullet.h"
#include "gun_bros_re/data/CGameAssetRef.h"
#include "gun_bros_re/data/CGameObjectPack.h"
#include "gun_bros_re/gameplay/CGun.h"
#include "engine/graphics/CMesh.h"
#include "engine/graphics/CMeshAnimationController.h"
#include "engine/graphics/CMeshCamera.h"
#include "engine/graphics/CMoveSetMesh.h"
#include "engine/graphics/CMoveSetMeshController.h"
#include "engine/resources/CResTOCManager.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "gun_bros_viewer/scenes/MeshPreviewInternal.h"
using namespace MeshPreviewDetail;


using namespace MeshPreviewDetail;

int RunPlayerEquipmentPreview(const std::string &bigDirectory, std::uint32_t gunIndex,
                    float spinDegrees, const std::string &screenshotPath,
                    std::uint32_t advanceMs, bool firePreview, int armorIndex) {
    std::printf("=== Player equipment: a whole character ===\n\n");

    CResTOCManager tocManager;
    if (!tocManager.InitAuto(bigDirectory) || !tocManager.Bind()) {
        return 1;
    }

    PackTables tables(tocManager);
    std::vector<WeaponEntry> weapons;
    PlayerTemplateData playerTemplate;
    if (!LoadWeaponCatalog(tocManager, tables, weapons)) { return 1; }

    std::vector<ArmorEntry> armors;
    if (armorIndex >= 0 && !LoadArmorCatalog(tocManager, tables, armors)) {
        return 1;
    }
    if (armorIndex >= static_cast<int>(armors.size()) && armorIndex >= 0) {
        std::printf("[armor] index %d out of range\n", armorIndex);
        return 1;
    }

    if (!FindPlayerTemplate(tocManager, tables, playerTemplate)) {
        std::printf("[equipment] no player template found\n");
        return 1;
    }
    if (weapons.empty()) {
        std::printf("[equipment] no gun names a weapon model\n");
        return 1;
    }
    std::printf("\n[equipment] %s, %zu weapon models\n", playerTemplate.owner.c_str(),
                weapons.size());

    std::size_t gunSlot = gunIndex;
    if (gunSlot >= weapons.size()) {
        std::printf("[equipment] weapon index out of range\n"); return 1;
    }

    std::string equipmentView = "Player weapon";
    if (armorIndex >= 0) { equipmentView = "Player armor"; }
    CWindow window;
    if (!OpenViewerWindow(window, equipmentView)) {
        return 1;
    }
    ViewerBindingSet bindings = weaponview::Bindings;
    if (armorIndex >= 0) { bindings = armorview::Bindings; }
    ViewerControls controls(window, bindings);
    if (!controls.Init()) { return 1; }


    CShaderProgram program;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) {
        return 1;
    }

    // Held by pointer for the same reason one part is: swapping the gun
    // rebuilds the whole thing, and nothing in it can be moved.
    std::unique_ptr<PlayerModel> character(new PlayerModel());
    if (!BuildPlayerBody(tables, playerTemplate.moveSet, *character) ||
        !EquipPlayerWeapon(tables, playerTemplate.script, weapons[gunSlot].data,
                           weapons[gunSlot].owner, *character) ||
        !CreatePlayerBuffers(*character, program)) {
        return 1;
    }

    std::size_t moveSlot = 0;
    SelectPlayerMoveSlot(*character, moveSlot, true);
    SetPlayerInput(*character, false, firePreview);
    window.SetRightDrag(false);
    window.SetTitle(ViewerWindowTitle(equipmentView, WeaponSelectionLabel(weapons, gunSlot)));
    if (armorIndex >= 0) {
        if (!EquipPlayerArmor(tables, armors[armorIndex].data, program, *character)) {
            return 1;
        }
        window.SetTitle(ViewerWindowTitle(equipmentView, std::to_string(armorIndex) + "/" +
            std::to_string(armors.size() - 1) + " | " + armors[armorIndex].owner));
        std::printf("[armor] Left/Right: armor; B: remove all; 1-7,N/M: weapon; F: fire; WASD: walk\n");
    }
    PosePlayer(*character);
    WeaponEffects effects(tocManager, tables, program);

    glEnable(GL_DEPTH_TEST);

    // Meshes carry alpha: the turret's ground shadow is a faded disc, and
    // without this it draws as a white plate.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Turntable view;
    view.spinDegrees = kUiFacingDegrees + spinDegrees;
    view.tiltDegrees = kUiTiltDegrees;
    view.extraTilt = 0.0f;
    view.zoom = 1.0f;

    // Input help is generated by ViewerControls from ViewerBindings.h.

    std::uint64_t previousTicks = window.GetTicksMs();
    bool paused = false;
    bool singleStep = false;
    bool reportedFirstFrame = false;
    std::uint32_t warmUpRemaining = advanceMs;

    while (controls.PumpEvents()) {
        int drawableWidth = 0;
        int drawableHeight = 0;
        controls.GetDrawableSize(drawableWidth, drawableHeight);

        const std::size_t previousGunSlot = gunSlot;
        for (KeyCode key = controls.TakeKeyPress(); key != KeyCode::None;
             key = controls.TakeKeyPress()) {
            if (armorIndex >= 0 && (controls.IsPressed(key, ViewerAction::Previous) || controls.IsPressed(key, ViewerAction::Next))) {
                int next = armorIndex + 1;
                if (controls.IsPressed(key, ViewerAction::Previous)) {
                    next = armorIndex + static_cast<int>(armors.size()) - 1;
                }
                next %= static_cast<int>(armors.size());
                if (!EquipPlayerArmor(tables, armors[next].data, program, *character)) {
                    return 1;
                }
                armorIndex = next;
                window.SetTitle(ViewerWindowTitle(equipmentView, std::to_string(armorIndex) + "/" +
                    std::to_string(armors.size() - 1) + " | " + armors[armorIndex].owner));
                continue;
            }
            if (armorIndex >= 0 && controls.IsPressed(key, ViewerAction::ClearArmor)) {
                ClearPlayerArmor(*character);
                continue;
            }
            const std::size_t gunCount = weapons.size();
            gunSlot = SelectWeaponKey(weapons, gunSlot, controls.WeaponSelectionKey(key));
            if (controls.IsPressed(key, ViewerAction::Next)) {
                gunSlot = (gunSlot + 1) % gunCount;
            } else if (controls.IsPressed(key, ViewerAction::Previous)) {
                gunSlot = (gunSlot + gunCount - 1) % gunCount;
            } else if (controls.IsPressed(key, ViewerAction::NextPage)) {
                gunSlot = (gunSlot + 10) % gunCount;
            } else if (controls.IsPressed(key, ViewerAction::PreviousPage)) {
                gunSlot = (gunSlot + gunCount - 10) % gunCount;
            } else if (controls.IsPressed(key, ViewerAction::NextVariant)) {
                // N/M now select equipment through the shared catalogue above.
            } else if (controls.IsPressed(key, ViewerAction::PreviousVariant)) {
                // The torso has the longest move list, so step by it and let
                // the shorter ones wrap inside SelectMoveSlot.
                // The old synchronized slot convention is superseded by scripts.
            } else if (controls.IsPressed(key, ViewerAction::Pause)) {
                paused = !paused;
                std::printf("[equipment] %s\n", paused ? "paused" : "playing");
            } else if (controls.IsPressed(key, ViewerAction::Step)) {
                singleStep = true;
            } else if (controls.IsPressed(key, ViewerAction::Tilt)) {
                if (view.tiltDegrees == kUiTiltDegrees) {
                    view.tiltDegrees = kGameTiltDegrees;
                } else {
                    view.tiltDegrees = kUiTiltDegrees;
                }
                std::printf("[equipment] tilt %.0f degrees\n", view.tiltDegrees);
            } else if (controls.IsPressed(key, ViewerAction::ResetView)) {
                view.spinDegrees = kUiFacingDegrees + spinDegrees;
                view.extraTilt = 0.0f;
                view.zoom = 1.0f;
            }
        }

        if (gunSlot != previousGunSlot) {
            std::printf("\n[equipment] --- weapon %zu of %zu ---\n", gunSlot + 1,
                        weapons.size());

            std::unique_ptr<PlayerModel> replacement(new PlayerModel());
            if (BuildPlayerBody(tables, playerTemplate.moveSet, *replacement) &&
                EquipPlayerWeapon(tables, playerTemplate.script, weapons[gunSlot].data,
                                  weapons[gunSlot].owner, *replacement) &&
                CreatePlayerBuffers(*replacement, program)) {
                // Weapon changes preserve all independently equipped armour slots.
                for (std::uint32_t slot = 0; slot < kArmorSlotCount; ++slot) {
                    replacement->armor[slot] = std::move(character->armor[slot]);
                }
                character = std::move(replacement);
                effects.Clear();
                SelectPlayerMoveSlot(*character, moveSlot, true);
                PosePlayer(*character);
                window.SetTitle(ViewerWindowTitle(equipmentView, WeaponSelectionLabel(weapons, gunSlot)));
            } else {
                std::printf("[equipment] staying on the previous weapon\n");
                gunSlot = previousGunSlot;
            }
        }

        const std::uint64_t nowTicks = window.GetTicksMs();
        std::uint64_t elapsedMs = nowTicks - previousTicks;
        previousTicks = nowTicks;

        if (elapsedMs > kMaxFrameMs) {
            elapsedMs = kMaxFrameMs;
        }
        if (paused) {
            elapsedMs = 0;
        }
        if (singleStep) {
            elapsedMs = static_cast<std::uint64_t>(kSingleStepMs);
            singleStep = false;
        }
        // Screenshots advance by the requested fixed steps, independent of loading time.
        if (!screenshotPath.empty()) { elapsedMs = 0; }
        if (elapsedMs > 0) {
            const bool moving = controls.IsDown(ViewerAction::MoveUp) || controls.IsDown(ViewerAction::MoveLeft) ||
                controls.IsDown(ViewerAction::MoveDown) || controls.IsDown(ViewerAction::MoveRight);
            SetPlayerInput(*character, moving, firePreview || controls.IsDown(ViewerAction::Fire));
            AdvancePlayer(*character, static_cast<std::int32_t>(elapsedMs));
        }

        int dragX = 0;
        int dragY = 0;
        controls.TakeDragDelta(dragX, dragY);
        view.spinDegrees += static_cast<float>(dragX) * kDragToDegrees;
        view.extraTilt += static_cast<float>(dragY) * kDragToDegrees;

        const float wheel = controls.TakeWheelDelta();
        for (float notch = 0.0f; notch < wheel; notch += 1.0f) {
            view.zoom *= kZoomPerNotch;
        }
        for (float notch = 0.0f; notch > wheel; notch -= 1.0f) {
            view.zoom /= kZoomPerNotch;
        }
        if (view.zoom < kMinZoom) {
            view.zoom = kMinZoom;
        }
        if (view.zoom > kMaxZoom) {
            view.zoom = kMaxZoom;
        }

        glViewport(0, 0, drawableWidth, drawableHeight);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float base[kMatrix4dElements];
        BuildModelViewProjection(PlayerBounds(*character), view, drawableWidth,
                                 drawableHeight, base);
        float viewport[kMatrix4dElements];
        Matrix4dIdentity(viewport);
        viewport[0] = drawableWidth * 0.5f;
        viewport[3] = drawableWidth * 0.5f;
        viewport[5] = -drawableHeight * 0.5f;
        viewport[7] = drawableHeight * 0.5f;
        float modelToScreen[kMatrix4dElements];
        Matrix4dMultiply(viewport, base, modelToScreen);
        // Simulate both viewers in the same world units. The turntable only
        // projects the result; spread is applied before the camera rotation.
        const float worldScale = PlayerModelWorldScale(*character, playerTemplate.gameScale, 1);
        float modelToWorld[kMatrix4dElements];
        Matrix4dScale(worldScale, modelToWorld);
        float inverseScale[kMatrix4dElements];
        Matrix4dScale(1.0f / worldScale, inverseScale);
        float worldToScreen[kMatrix4dElements];
        Matrix4dMultiply(modelToScreen, inverseScale, worldToScreen);
        // Looking straight into a barrel projects its travel to a point.
        // Do not turn floating-point noise at 90 degrees into diagonal shots.
        while (warmUpRemaining > 0) {
            const int step = static_cast<int>(std::min<std::uint32_t>(warmUpRemaining, kWarmUpFrameMs));
            AdvancePlayer(*character, step);
            effects.Update(*character, modelToWorld, 0, step);
            warmUpRemaining -= step;
        }
        effects.SetPaused(paused);
        effects.Update(*character, modelToWorld, 0, static_cast<int>(elapsedMs));
        float screenMvp[kMatrix4dElements];
        Matrix4dOrthoTopLeft(static_cast<float>(drawableWidth), static_cast<float>(drawableHeight), 1000.0f, screenMvp);
        effects.Draw(screenMvp, worldToScreen, 1.0f, WeaponDrawPass::BehindPlayer);
        glEnable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        DrawPlayer(*character, program, base);
        effects.Draw(screenMvp, worldToScreen, 1.0f, WeaponDrawPass::InFrontOfPlayer);

        if (!controls.Draw()) { return 1; }

        if (!reportedFirstFrame) {
            GLCheckErrors("first frame");
            reportedFirstFrame = true;

            if (!screenshotPath.empty()) {
                std::printf("[weapon-render] shots=%zu live=%zu torsoMove=%d legsMove=%d\n",
                    effects.GetShotCount(), effects.GetBulletCount(),
                    character->weapon->brother.GetTorso().GetMoveIndex(),
                    character->weapon->brother.GetLegs().GetMoveIndex());
                if (!GB_SAVE_FRAME(window, screenshotPath)) {
                    return 1;
                }
                window.Present();
                break;
            }
        }

        window.Present();
    }

    std::printf("[equipment] done\n");
    return 0;
}

