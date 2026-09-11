/** @file Arena.h
 * @brief Interactive single-template combat laboratory and repeatable checks.
 */
#ifndef GUN_BROS_VIEWER_ARENAPREVIEW_H
#define GUN_BROS_VIEWER_ARENAPREVIEW_H
#include <string>
#include <cstdint>
int RunArena(const std::string &bigDirectory, std::uint32_t enemyIndex,
    std::uint32_t weaponIndex, const std::string &screenshot, std::uint32_t advanceMs,
    bool fire, bool check, bool showCollisions = false, int armorIndex = -1);
#endif
