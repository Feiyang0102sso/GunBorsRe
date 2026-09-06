/**
 * @file M3Map.h
 * @brief M3 milestone harness: a whole level, terrain and scenery, on screen.
 */

#ifndef GUN_BROS_RE_MILESTONES_M3MAP_H
#define GUN_BROS_RE_MILESTONES_M3MAP_H

#include <cstdint>
#include <string>

/**
 * Assemble one map out of its tile and object layers and draw it, pannable
 * with the mouse.
 *
 * @param bigDirectory   Directory holding the .big files.
 * @param packShortName  Pack the map lives in, e.g. "pack2".
 * @param mapIndex       Ordinal within the pack's TILELAYER section.
 * @param screenshotPath When non-empty, save the first frame and exit.
 * @param advanceMs      Milliseconds to run the animations on before that
 *                       first frame, so a screenshot can be taken of a map
 *                       part-way through its animation rather than at rest.
 * @return 0 when the map was displayed.
 */
int RunM3Map(const std::string &bigDirectory, const std::string &packShortName,
             std::uint32_t mapIndex, const std::string &screenshotPath,
             std::uint32_t advanceMs);

/** List every pack that holds maps, and how many. */
int RunMapList(const std::string &bigDirectory);

#endif  // GUN_BROS_RE_MILESTONES_M3MAP_H
