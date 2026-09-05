/**
 * @file M3Map.h
 * @brief M3 milestone harness: a whole level's terrain, on screen.
 */

#ifndef GUN_BROS_RE_MILESTONES_M3MAP_H
#define GUN_BROS_RE_MILESTONES_M3MAP_H

#include <cstdint>
#include <string>

/**
 * Assemble one map out of its tile layers and draw it, pannable with the mouse.
 *
 * @param bigDirectory   Directory holding the .big files.
 * @param packShortName  Pack the map lives in, e.g. "pack2".
 * @param mapIndex       Ordinal within the pack's TILELAYER section.
 * @param screenshotPath When non-empty, save the first frame and exit.
 * @return 0 when the map was displayed.
 */
int RunM3Map(const std::string &bigDirectory, const std::string &packShortName,
             std::uint32_t mapIndex, const std::string &screenshotPath);

/** List every pack that holds maps, and how many. */
int RunMapList(const std::string &bigDirectory);

#endif  // GUN_BROS_RE_MILESTONES_M3MAP_H
