/**
 * @file M1Resources.h
 * @brief M1 milestone harness: cross-pack resource addressing.
 */

#ifndef GUN_BROS_RE_MILESTONES_M1RESOURCES_H
#define GUN_BROS_RE_MILESTONES_M1RESOURCES_H

#include <string>

/**
 * Bind every pack, survey how each handle resolves, sample-decompress, and
 * check one known CGameAssetRef end to end.
 *
 * @return 0 when the acceptance target matched.
 */
int RunM1Resources(const std::string &bigDirectory);

/**
 * List one pack's resource table: index, logical ID, group, block size and
 * compression flag. This is the listing PLAN.md asks M1 to produce.
 *
 * @param packShortName Art-set-free name, e.g. "pack0_core" or "pack1".
 */
int RunPackDump(const std::string &bigDirectory, const std::string &packShortName);

#endif  // GUN_BROS_RE_MILESTONES_M1RESOURCES_H
