/**
 * @file MapPreview.h
 * @brief Map presentation backed by the game terrain and object renderer.
 */

#ifndef GUN_BROS_VIEWER_MAPPREVIEW_H
#define GUN_BROS_VIEWER_MAPPREVIEW_H

#include <cstdint>
#include <string>
#include "gun_bros_re/gameplay/MapScene.h"

/** The two intentionally different ways a map can be opened. */
// MapViewMode now belongs to runtime/MapScene.h, shared by game and tools.

/**
 * Assemble one map out of its tile and object layers and draw it.
 *
 * @param bigDirectory   Directory holding the .big files.
 * @param packShortName  Pack the map lives in, e.g. "pack2".
 * @param mapIndex       Ordinal within the pack's TILELAYER section.
 * @param screenshotPath When non-empty, save the first frame and exit.
 * @param advanceMs      Milliseconds to run the animations on before that
 *                       first frame, so a screenshot can be taken of a map
 *                       part-way through its animation rather than at rest.
 * @param showSpawns     Start with the spawn-point overlay on. `K` toggles it
 *                       either way; this is so a screenshot can carry it.
 * @param showCollisions Start with collision edges visible. `C` toggles them.
 * @param viewMode        Preview is a freely navigable whole-map canvas;
 *                        GameView is the fixed, playable camera.
 * @return 0 when the map was displayed.
 */
int RunMapPreview(const std::string &bigDirectory, const std::string &packShortName,
             std::uint32_t mapIndex, const std::string &screenshotPath,
             std::uint32_t advanceMs, bool showSpawns, bool showCollisions,
             MapViewMode viewMode, std::uint32_t weaponIndex = 0,
             bool firePreview = false);

/** List every pack that holds maps, and how many. */
int RunMapList(const std::string &bigDirectory);

/** Retail survival on the same terrain renderer; research viewers stay separate. */
struct SurvivalGameContext;
struct MissionEntry;
// RunSurvival is declared in runtime/MapScene.h; no game caller needs this harness.

#endif  // GUN_BROS_VIEWER_MAPPREVIEW_H
