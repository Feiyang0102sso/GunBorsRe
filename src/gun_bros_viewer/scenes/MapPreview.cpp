#include "gun_bros_viewer/ViewerControls.h"
#include "gun_bros_viewer/ViewerSettings.h"
#include "gun_bros_viewer/scenes/MapPreview.h"
#include "gun_bros_viewer/scenes/MapTurretPreview.h"
#include "gun_bros_re/gameplay/MapWorldInternal.h"
#include <algorithm>
#include <charconv>
using namespace MapDetail;

namespace {
bool ReadPackNumber(const std::string &name, std::uint32_t &number) {
    if (name.compare(0, 4, "pack") != 0) { return false; }
    const char *end = name.data() + name.size();
    const auto parsed = std::from_chars(name.data() + 4, end, number);
    return parsed.ec == std::errc() && parsed.ptr == end;
}

bool ViewerMapComesBefore(const CatalogMap &first, const CatalogMap &second) {
    std::uint32_t firstNumber = 0, secondNumber = 0;
    const bool firstNumbered = ReadPackNumber(first.packName, firstNumber);
    const bool secondNumbered = ReadPackNumber(second.packName, secondNumber);
    if (firstNumbered != secondNumbered) { return firstNumbered; }
    if (firstNumbered && firstNumber != secondNumber) { return firstNumber < secondNumber; }
    if (first.packName != second.packName) { return first.packName < second.packName; }
    return first.mapIndex < second.mapIndex;
}

std::vector<CatalogMap> BuildViewerMapCatalog(CResTOCManager &tocManager) {
    // Sort only the presentation list; pack indices and resource ordinals stay intact.
    std::vector<CatalogMap> catalog = BuildCatalog(tocManager);
    std::stable_sort(catalog.begin(), catalog.end(), ViewerMapComesBefore);
    return catalog;
}
}

int RunMapList(const std::string &bigDirectory) {
    CResTOCManager tocManager;
    if (!tocManager.InitAuto(bigDirectory)) {
        return 1;
    }
    if (!tocManager.Bind()) {
        return 1;
    }

    std::printf("\n%-12s %6s %9s %8s\n", "pack", "maps", "tilesets", "levels");
    for (std::uint32_t i = 0; i < tocManager.GetPackCount(); ++i) {
        CResPackTOC *pack = tocManager.GetPack(static_cast<int>(i));

        CGameObjectPack objectPack;
        if (!objectPack.Init(*pack)) {
            continue;
        }

        const std::uint32_t maps = objectPack.GetObjectCount(GameSection::TileLayer);
        if (maps == 0) {
            continue;
        }
        std::printf("%-12s %6u %9u %8u\n", pack->GetShortName().c_str(), maps,
                    objectPack.GetObjectCount(GameSection::TileSet),
                    objectPack.GetObjectCount(GameSection::Level));
    }

    // The flat order the viewer's left and right keys walk, so a map can be
    // named by one number instead of a pack and an ordinal.
    const std::vector<CatalogMap> catalog = BuildViewerMapCatalog(tocManager);
    std::printf("\nviewer order (%zu maps):\n", catalog.size());
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        std::printf("  %2zu  %-8s map %u\n", i + 1, catalog[i].packName.c_str(),
                    catalog[i].mapIndex);
    }
    return 0;
}

int RunMapPreview(const std::string &bigDirectory, const std::string &packShortName,
             std::uint32_t mapIndex, const std::string &screenshotPath,
             std::uint32_t advanceMs, bool startWithSpawns,
             bool startWithCollisions, MapViewMode viewMode, std::uint32_t weaponIndex,
             bool firePreview) {
    const bool gameView = viewMode == MapViewMode::GameView;
    const char *modeName = "Preview";
    if (gameView) {
        modeName = "GameView";
    }
    std::printf("=== Map: %s ===\n\n", modeName);

    // --- resources, before any GL exists ---
    CResTOCManager tocManager;
    if (!tocManager.InitAuto(bigDirectory) || !tocManager.Bind()) {
        return 1;
    }

    std::vector<CatalogMap> catalog = BuildViewerMapCatalog(tocManager);
    if (catalog.empty()) {
        std::printf("[map] no pack contains any maps\n");
        return 1;
    }

    // --map only picks where to start now; the whole catalogue is reachable
    // from the keyboard.
    std::size_t slot = 0;
    if (!packShortName.empty()) {
        bool found = false;
        for (std::size_t index = 0; index < catalog.size(); ++index) {
            if (catalog[index].packName == packShortName && catalog[index].mapIndex == mapIndex) {
                slot = index;
                found = true;
                break;
            }
        }
        if (!found) { std::printf("[map] map not found: %s %u\n", packShortName.c_str(), mapIndex); return 1; }
    }

    std::printf("[map] %zu maps across the archives:", catalog.size());
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        if (i == 0 || catalog[i].packIndex != catalog[i - 1].packIndex) {
            std::printf(" %s(", catalog[i].packName.c_str());
        }
        std::printf("%u", catalog[i].mapIndex);
        const bool lastOfPack = (i + 1 == catalog.size()) ||
                                (catalog[i + 1].packIndex != catalog[i].packIndex);
        std::printf("%s", lastOfPack ? ")" : ",");
    }
    std::printf("\n");

    // --- window, then everything that needs a context ---
    CWindow window;
    if (!OpenViewerWindow(window, "Map")) {
        return 1;
    }
    ViewerControls controls(window, mapview::Bindings, !gameView);
    if (!controls.Init()) { return 1; }


    CShaderProgram program;
    if (!program.Load(kShaderDirectory, "ogles_vs_mvp_tex0", "ogles_ps_tex0")) {
        return 1;
    }

    CQuadBatch batch;
    // The constant-colour pair, for the spawn markers. Two programs, because
    // the shaders the engine ships are one job each.
    CShaderProgram markerProgram;
    if (!markerProgram.Load(kShaderDirectory, "ogles_vs_mvp_constcolor",
                            "ogles_ps_constcolor")) {
        return 1;
    }

    CMarkerBatch markers;
    if (!markers.Create(markerProgram)) {
        return 1;
    }

    if (!batch.Create(program)) {
        return 1;
    }
    CAudioPlayer audio;
    PackTables weaponTables(tocManager);
    std::vector<WeaponEntry> weapons;
    std::size_t weaponSlot = weaponIndex;
    std::unique_ptr<WeaponEffects> weaponEffects;
    if (gameView) {
        if (!LoadWeaponCatalog(tocManager, weaponTables, weapons)) { return 1; }
        if (weaponSlot >= weapons.size()) { weaponSlot = 0; }
        weaponEffects.reset(new WeaponEffects(tocManager, weaponTables, program));
    }

    int drawableWidth = 0;
    int drawableHeight = 0;
    controls.GetDrawableSize(drawableWidth, drawableHeight);

    std::printf("\n[map] --- %s map %u (%zu of %zu) ---\n",
                catalog[slot].packName.c_str(), catalog[slot].mapIndex, slot + 1,
                catalog.size());

    LoadedMap loaded;
    if (!LoadMap(tocManager, catalog[slot].packIndex, catalog[slot].mapIndex,
                 loaded)) {
        return 1;
    }
    LoadProps(tocManager, loaded);
    BuildCollisionScene(loaded);
    LoadPlacedEnemies(tocManager, program, loaded);
    LoadPlacedPlayers(tocManager, program, loaded);
    MapTurretPreview turrets;
    if (!gameView) { turrets.Bind(loaded); }
    if (gameView && !EquipControlledPlayer(weaponTables, loaded, program, weapons[weaponSlot])) { return 1; }
    ReportSpawns(loaded);
    WarmUp(loaded, advanceMs, weaponEffects.get(), firePreview);
    turrets.Update(static_cast<int>(advanceMs));

    // Either layer can be hidden, which is how "is that rock in the right\n// place or is the ground wrong?" gets answered without a debugger.
    bool showTiles = true;
    bool showProps = true;
    bool showSpawns = startWithSpawns;
    bool showCollisions = startWithCollisions;
    CoverState coverState = CoverState::Intact;
    std::uint8_t barrelState = 0;
    std::uint8_t spireState = 0;

    // The props animate, so the geometry is rebuilt every frame from here on.
    // This flag only decides whether a rebuild says anything about itself.
    bool reportGeometry = true;
    Camera camera = FitCamera(loaded, drawableWidth, drawableHeight);
    bool followPlayer = gameView && !loaded.players.empty();
    if (followPlayer) {
        camera.zoom = GameViewCameraZoom(drawableWidth, drawableHeight);
        FollowPlayerCamera(loaded, drawableWidth, drawableHeight, camera);
    }

    // Time, and the two ways of taking it apart when something looks wrong:
    // stop it, or move it on one bite at a time.
    std::uint64_t previousTicks = window.GetTicksMs();
    bool paused = false;
    bool singleStep = false;

    // The factors themselves are the batch's business now: glows and fire are
    // additive, everything else is straight alpha.
    glEnable(GL_BLEND);

    if (gameView) {
        std::printf("\n[gameview] WASD: move, arrows: change map, "
                    "1-7: weapon category, N/M: weapon, mouse: aim, left mouse: fire, "
                    "T: tiles, P: props, K: spawns, B: covers, E: barrels, "
                    "F: spires, C: collision, "
                    "space: pause, '.': one step, Esc: quit\n");
    }

    bool reportedFirstFrame = false;
    while (controls.PumpEvents()) {
        controls.GetDrawableSize(drawableWidth, drawableHeight);

        // --- switching maps and packs ---
        const std::size_t previousSlot = slot;
        bool reload = false;
        bool refit = false;
        for (KeyCode key = controls.TakeKeyPress(); key != KeyCode::None;
             key = controls.TakeKeyPress()) {
            if (gameView) {
                const std::size_t nextWeapon = SelectWeaponKey(weapons, weaponSlot, key);
                if (nextWeapon != weaponSlot && EquipControlledPlayer(weaponTables, loaded, program, weapons[nextWeapon])) {
                    weaponEffects->Clear();
                    weaponSlot = nextWeapon;
                    std::printf("[weapon] %s\n", WeaponSelectionLabel(weapons, weaponSlot).c_str());
                }
            }
            if (controls.IsPressed(key, ViewerAction::Tiles)) {
                showTiles = !showTiles;
                reportGeometry = true;
            } else if (controls.IsPressed(key, ViewerAction::Cover)) {
                coverState = NextCoverState(coverState);
                const std::uint32_t changed = SetCoverState(loaded, coverState);
                StartTransitionParticles(
                    tocManager, loaded, InteractivePropKind::Cover,
                    static_cast<std::uint8_t>(coverState));
                PlayTransitionSound(tocManager, loaded, audio,
                                    InteractivePropKind::Cover,
                                    static_cast<std::uint8_t>(coverState));
                BuildCollisionScene(loaded);
                reportGeometry = true;
                std::printf("[m4] %u covers: %s\n", changed,
                            CoverStateName(coverState));
            } else if (controls.IsPressed(key, ViewerAction::Barrel)) {
                barrelState = static_cast<std::uint8_t>((barrelState + 1) % 4);
                const std::uint32_t changed = SetInteractiveState(
                    loaded, InteractivePropKind::Barrel, barrelState);
                StartTransitionParticles(tocManager, loaded,
                                         InteractivePropKind::Barrel,
                                         barrelState);
                PlayTransitionSound(tocManager, loaded, audio,
                                    InteractivePropKind::Barrel, barrelState);
                BuildCollisionScene(loaded);
                reportGeometry = true;
                std::printf("[m4] %u barrels: %s\n", changed,
                            InteractiveStateName(InteractivePropKind::Barrel,
                                                 barrelState));
            } else if (!gameView && controls.IsPressed(key, ViewerAction::Turret)) {
                turrets.Cycle();
            } else if ((!gameView && controls.IsPressed(key, ViewerAction::Spire)) ||
                       (gameView && key == mapview::GameViewSpire)) {
                spireState = static_cast<std::uint8_t>((spireState + 1) % 3);
                const std::uint32_t changed = SetInteractiveState(
                    loaded, InteractivePropKind::Spire, spireState);
                StartTransitionParticles(tocManager, loaded,
                                         InteractivePropKind::Spire,
                                         spireState);
                PlayTransitionSound(tocManager, loaded, audio,
                                    InteractivePropKind::Spire, spireState);
                reportGeometry = true;
                std::printf("[m4] %u spires: %s\n", changed,
                            InteractiveStateName(InteractivePropKind::Spire,
                                                 spireState));
            } else if (controls.IsPressed(key, ViewerAction::Collisions)) {
                showCollisions = !showCollisions;
                std::printf("[m4] collision %s\n",
                            showCollisions ? "on" : "off");
            } else if (controls.IsPressed(key, ViewerAction::Spawns)) {
                showSpawns = !showSpawns;
                std::printf("[map] spawns %s\n", showSpawns ? "on" : "off");
            } else if (controls.IsPressed(key, ViewerAction::Props)) {
                showProps = !showProps;
                reportGeometry = true;
            } else if (controls.IsPressed(key, ViewerAction::Pause)) {
                paused = !paused;
                std::printf("[map] %s\n", paused ? "paused" : "running");
            } else if (controls.IsPressed(key, ViewerAction::Step)) {
                // Stepping implies pausing: otherwise the step is lost in the
                // real time that keeps flowing around it.
                paused = true;
                singleStep = true;
            } else if (controls.IsPressed(key, ViewerAction::Next)) {
                // One step through the flat list, so the last map of a pack is
                // followed by the first map of the next.
                slot = (slot + 1) % catalog.size();
                reload = true;
            } else if (controls.IsPressed(key, ViewerAction::Previous)) {
                slot = (slot + catalog.size() - 1) % catalog.size();
                reload = true;
            } else if (controls.IsPressed(key, ViewerAction::NextPage)) {
                slot = NextPackSlot(catalog, slot);
                reload = true;
            } else if (controls.IsPressed(key, ViewerAction::PreviousPage)) {
                slot = PreviousPackSlot(catalog, slot);
                reload = true;
            } else if (controls.IsPressed(key, ViewerAction::ResetView) && !gameView) {
                refit = true;
            }
        }

        if (reload) {
            std::printf("\n[map] --- %s map %u (%zu of %zu) ---\n",
                        catalog[slot].packName.c_str(), catalog[slot].mapIndex,
                        slot + 1, catalog.size());

            // Replace wholesale: the old textures, including every atlas page
            // the props point at, go with the old LoadedMap.
            LoadedMap replacement;
            if (LoadMap(tocManager, catalog[slot].packIndex, catalog[slot].mapIndex,
                        replacement)) {
                LoadProps(tocManager, replacement);
                coverState = CoverState::Intact;
                barrelState = 0;
                spireState = 0;
                BuildCollisionScene(replacement);
                LoadPlacedEnemies(tocManager, program, replacement);
                LoadPlacedPlayers(tocManager, program, replacement);
                if (gameView) {
                    if (!EquipControlledPlayer(weaponTables, replacement, program, weapons[weaponSlot])) { return 1; }
                    weaponEffects->Clear();
                }
                ReportSpawns(replacement);
                if (!gameView) { turrets.Bind(replacement); }
                WarmUp(replacement, advanceMs, weaponEffects.get(), firePreview);
                turrets.Update(static_cast<int>(advanceMs));
                loaded = std::move(replacement);
                reportGeometry = true;
                followPlayer = gameView && !loaded.players.empty();
                if (followPlayer) {
                    camera.zoom = GameViewCameraZoom(
                        drawableWidth, drawableHeight);
                    FollowPlayerCamera(loaded, drawableWidth, drawableHeight,
                                       camera);
                    refit = false;
                } else {
                    refit = true;
                }
            } else {
                // A map that will not load leaves the previous one on screen
                // rather than a blank window.
                std::printf("[map] staying on the previous map\n");
                slot = previousSlot;
            }
        }
        if (refit) {
            camera = FitCamera(loaded, drawableWidth, drawableHeight);
        }

        // Identify the displayed map, including after catalogue navigation.
        std::string titleDetails = catalog[slot].packName + " map " +
            std::to_string(catalog[slot].mapIndex) + " | " +
            std::to_string(slot + 1) + "/" + std::to_string(catalog.size());
        if (gameView) { titleDetails += " | " + WeaponSelectionLabel(weapons, weaponSlot); }
        if (!turrets.Empty()) { titleDetails += std::string(" | Turret: ") + turrets.StateName(); }
        window.SetTitle(ViewerWindowTitle("Map", titleDetails));

        // --- animation clock ---
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
            elapsedMs = kSingleStepMs;
            singleStep = false;
        }

        if (gameView) {
            UpdateControlledPlayer(loaded, window, elapsedMs);
            if (!loaded.players.empty()) {
                PlacedPlayer &player = loaded.players[0];
                float mouseX = 0, mouseY = 0;
                if (elapsedMs > 0 && window.GetMousePosition(mouseX, mouseY) && screenshotPath.empty()) {
                    const float aimX = camera.x + mouseX / camera.zoom - player.x;
                    const float aimY = camera.y + mouseY / camera.zoom - player.y;
                    if (aimX != 0 || aimY != 0) {
                        player.facingDegrees = std::atan2(aimY, aimX) * kRadiansToDegrees + 90.0f;
                    }
                }
                SetPlayerInput(*player.model, player.moving, firePreview || window.IsLeftMouseDown());
                weaponEffects->SetPaused(elapsedMs == 0);
            }
        }
        AdvanceProps(loaded.props, static_cast<std::uint16_t>(elapsedMs));
        AdvanceParticleEffects(loaded, static_cast<std::uint16_t>(elapsedMs));
        AdvanceEnemies(loaded, static_cast<std::int32_t>(elapsedMs));
        turrets.Update(static_cast<int>(elapsedMs));
        AdvancePlayers(loaded, static_cast<std::int32_t>(elapsedMs));
        if (gameView && !loaded.players.empty()) {
            PlacedPlayer &player = loaded.players[0];
            float identity[kMatrix4dElements], modelToWorld[kMatrix4dElements];
            Matrix4dIdentity(identity);
            const float scale = PlayerModelWorldScale(*player.model, loaded.playerTemplate->gameScale, kLevelCameraScale);
            BuildPlayerGameMatrix(identity, player.x, player.y, scale, player.facingDegrees, modelToWorld);
            weaponEffects->Update(*player.model, modelToWorld, player.facingDegrees,
                static_cast<int>(elapsedMs), &loaded.weaponCollision);
        }
        AdvanceTileLayers(loaded.map, static_cast<std::uint16_t>(elapsedMs));
        audio.Update();
        BuildGeometry(loaded, batch, showTiles, showProps, reportGeometry);
        reportGeometry = false;

        if (gameView) {
            // Window resizing changes only pixel scale, never the game field
            // of view. The camera remains entirely owned by GameView.
            camera.zoom = GameViewCameraZoom(drawableWidth,
                                             drawableHeight);
        }

        // --- zoom about the centre of the view ---
        const float wheel = controls.TakeWheelDelta();
        if (!gameView && wheel != 0.0f) {
            const float viewWidthBefore = static_cast<float>(drawableWidth) / camera.zoom;
            const float viewHeightBefore = static_cast<float>(drawableHeight) / camera.zoom;

            float factor = 1.0f;
            for (float notch = 0.0f; notch < wheel; notch += 1.0f) {
                factor *= kZoomPerNotch;
            }
            for (float notch = 0.0f; notch > wheel; notch -= 1.0f) {
                factor /= kZoomPerNotch;
            }
            camera.zoom *= factor;
            if (camera.zoom < kMinZoom) {
                camera.zoom = kMinZoom;
            }
            if (camera.zoom > kMaxZoom) {
                camera.zoom = kMaxZoom;
            }

            // Keep whatever was in the middle of the view in the middle.
            camera.x += (viewWidthBefore - static_cast<float>(drawableWidth) / camera.zoom) * 0.5f;
            camera.y += (viewHeightBefore - static_cast<float>(drawableHeight) / camera.zoom) * 0.5f;
        }

        if (followPlayer) {
            FollowPlayerCamera(loaded, drawableWidth, drawableHeight, camera);
        }

        // --- pan ---
        int dragX = 0;
        int dragY = 0;
        controls.TakeDragDelta(dragX, dragY);
        if (!gameView) {
            // Dragging right moves the view left, as if pulling the map along.
            // Divided by zoom so a drag tracks the cursor at any scale.
            camera.x -= static_cast<float>(dragX) / camera.zoom * kDragScale;
            camera.y -= static_cast<float>(dragY) / camera.zoom * kDragScale;
        }

        glViewport(0, 0, drawableWidth, drawableHeight);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Zoom is a wider or narrower slice of the world, so it goes into the
        // projection's extent rather than into a separate scale matrix.
        float mvp[kMatrix4dElements];
        Matrix4dOrthoTopLeft(static_cast<float>(drawableWidth) / camera.zoom,
                             static_cast<float>(drawableHeight) / camera.zoom,
                             kMapDepthRange, mvp);
        Matrix4dTranslate(mvp, -camera.x, -camera.y);

        int scissor[4];
        const bool clipping = VisibleBoundsScissor(
            loaded, camera, drawableWidth, drawableHeight, scissor);
        if (clipping) {
            // After the clear, so the background still fills the window.
            glEnable(GL_SCISSOR_TEST);
            glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        }

        batch.Draw(program, mvp);
        if (weaponEffects) {
            weaponEffects->Draw(mvp, nullptr, kLevelCameraScale, WeaponDrawPass::BehindPlayer);
        }
        DrawMapObjects(loaded, batch, program, mvp, showProps);
        if (weaponEffects) {
            weaponEffects->Draw(mvp, nullptr, kLevelCameraScale, WeaponDrawPass::InFrontOfPlayer);
        }

        if (showSpawns) {
            // On top of the terrain, and outside the prop batch, because a
            // marker is not part of the scene -- it is a note about it.
            BuildMarkers(loaded, markers, PlacedObjectType::Player);
            markers.Draw(markerProgram, mvp, 0.2f, 1.0f, 0.3f, 1.0f);

            BuildMarkers(loaded, markers, PlacedObjectType::Enemy);
            markers.Draw(markerProgram, mvp, 1.0f, 0.25f, 0.2f, 1.0f);
        }

        if (showCollisions) {
            DrawCollisionOverlay(markers, markerProgram, mvp, 1 / camera.zoom, &loaded, nullptr, nullptr, weaponEffects.get());
        }

        if (clipping) {
            glDisable(GL_SCISSOR_TEST);
        }

        if (!controls.Draw()) { return 1; }

        if (!reportedFirstFrame) {
            GLCheckErrors("first frame");
            reportedFirstFrame = true;

            if (!screenshotPath.empty()) {
                if (gameView) {
                    std::printf("[gameview-camera] zoom=%.4f world=%.1fx%.1f\n", camera.zoom,
                        drawableWidth / camera.zoom, drawableHeight / camera.zoom);
                }
                if (!GB_SAVE_FRAME(window, screenshotPath)) {
                    return 1;
                }
                window.Present();
                break;
            }
        }

        window.Present();
    }

    std::printf("[map] done\n");
    return 0;
}
