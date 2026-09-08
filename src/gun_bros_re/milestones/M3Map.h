/**
 * @file M3Map.h
 * @brief M3 milestone harness: a whole level, terrain and scenery, on screen.
 */

#ifndef GUN_BROS_RE_MILESTONES_M3MAP_H
#define GUN_BROS_RE_MILESTONES_M3MAP_H

#include <cstdint>
#include <string>

/** The two intentionally different ways a map can be opened. */
enum class MapViewMode {
    Preview,
    GameView,
};

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
int RunM3Map(const std::string &bigDirectory, const std::string &packShortName,
             std::uint32_t mapIndex, const std::string &screenshotPath,
             std::uint32_t advanceMs, bool showSpawns, bool showCollisions,
             MapViewMode viewMode, std::uint32_t weaponIndex = 0,
             bool firePreview = false);

/** List every pack that holds maps, and how many. */
int RunMapList(const std::string &bigDirectory);

/** Retail survival on the same terrain renderer; research viewers stay separate. */
struct SurvivalGameContext;
struct MissionEntry;
int RunSurvival(const std::string &bigDirectory, const std::string &packShortName,
    unsigned mapIndex, unsigned weaponIndex, int armorIndex, const std::string &screenshotPath,
    unsigned advanceMs, bool firePreview, bool showCollisions, bool check = false, unsigned checkWaves = 2, unsigned startWave = 0,
    SurvivalGameContext *gameContext = nullptr, bool withBrother = false, bool powerupStudy = false,
    const MissionEntry *archiveMission = nullptr, bool performanceStudy = false);

#endif  // GUN_BROS_RE_MILESTONES_M3MAP_H
