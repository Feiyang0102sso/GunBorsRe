/**
 * @file M35Mesh.h
 * @brief M3.5 harness: the 3D models in Section 31.
 */

#ifndef GUN_BROS_RE_MILESTONES_M35MESH_H
#define GUN_BROS_RE_MILESTONES_M35MESH_H

#include <cstdint>
#include <string>

/**
 * Show one model, textured, on a turntable.
 *
 * @param startIndex Which model of the catalogue to open on. The catalogue is
 *        every distinct model any template names, in the order the archives
 *        hand them over.
 * @param spinDegrees Turn the model this far about its own up axis before
 *        the first frame, so a screenshot can be taken from a chosen side.
 * @param frameIndex Which of the model's key frames to pose it with.
 * @param screenshotPath When non-empty, save the first frame here and exit.
 */
int RunM35Mesh(const std::string &bigDirectory, std::uint32_t startIndex,
               float spinDegrees, std::uint32_t frameIndex,
               const std::string &screenshotPath);

/**
 * Parse every mesh in every pack and print what came out of each.
 *
 * The number that signs off the parser is the leftover byte count: a mesh read
 * correctly ends exactly at the end of its resource. Touches no GL.
 *
 * @return 0 when every mesh parsed and none had bytes left over.
 */
int RunMeshSurvey(const std::string &bigDirectory);

/**
 * Walk every move set and follow its mesh and atlas ordinals to real
 * resources.
 *
 * A mesh has no texture of its own; the pairing lives in CMoveSetMesh, so this
 * is what proves a model can actually be drawn. Player templates are parsed
 * whole and checked for leftover bytes; enemy templates are only read as far
 * as their move set. Touches no GL.
 *
 * @return 0 when every pair resolved and no player template had bytes left.
 */
int RunMoveSetSurvey(const std::string &bigDirectory);

#endif  // GUN_BROS_RE_MILESTONES_M35MESH_H
