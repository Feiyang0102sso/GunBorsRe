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

/**
 * Walk every pack's LEVEL section and report which levels scroll a tile layer.
 *
 * Tile layer scroll speed is not in any map or tile set -- it is set at run
 * time by a level script calling CLevel::FunctionResolver's function 42. This
 * finds those calls by their byte signature, without an interpreter, which is
 * enough to say whether the archives use the feature and with what numbers.
 */
int RunLevelSurvey(const std::string &bigDirectory);

#endif  // GUN_BROS_RE_MILESTONES_M1RESOURCES_H
