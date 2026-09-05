/**
 * @file M2Texture.h
 * @brief M2 milestone harness: a PNG out of a .big, on screen.
 */

#ifndef GUN_BROS_RE_MILESTONES_M2TEXTURE_H
#define GUN_BROS_RE_MILESTONES_M2TEXTURE_H

#include <cstdint>
#include <string>

/**
 * Open a window, pull one PNG resource out of a pack, decode it, upload it and
 * draw it at its native size.
 *
 * @param bigDirectory   Directory holding the .big files.
 * @param packShortName  Pack to take the image from, e.g. "pack0_core".
 * @param resourceId     Logical resource ID of a PNG in that pack.
 * @param screenshotPath When non-empty, save the first rendered frame here and
 *                       exit instead of waiting for the user. Lets the
 *                       milestone be checked without a human at the keyboard.
 * @return 0 when the image was displayed.
 */
int RunM2Texture(const std::string &bigDirectory, const std::string &packShortName,
                 std::uint32_t resourceId, const std::string &screenshotPath);

#endif  // GUN_BROS_RE_MILESTONES_M2TEXTURE_H
