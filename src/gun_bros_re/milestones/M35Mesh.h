/**
 * @file M35Mesh.h
 * @brief M3.5 and M3.7 harnesses: the 3D models in Section 31.
 *
 * The character viewer lives alongside the model viewer because they share the
 * walk that answers "which atlas does this model wear" -- see the .cpp. The
 * assembly the character viewer shows is in PlayerModel.h; what is left here is
 * the weapon catalogue it pages through.
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
 * @param frameIndex Which of the model's key frames to pose it with. Only
 *        used by the models that have no move set; the rest play a move.
 * @param screenshotPath When non-empty, save the first frame here and exit.
 * @param advanceMs Run the animation on this far before the first frame, so a
 *        screenshot can be taken part way through a move.
 */
int RunM35Mesh(const std::string &bigDirectory, std::uint32_t startIndex,
               float spinDegrees, std::uint32_t frameIndex,
               const std::string &screenshotPath, std::uint32_t advanceMs);

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
/** Print the archive weapon catalogue for inspection. */
int RunWeaponSurvey(const std::string &bigDirectory);
/** Exercise every weapon through equip, movement, firing and release with real assets. */
int RunWeaponCheck(const std::string &bigDirectory);

/**
 * M3.7: stand a whole player up -- torso, legs and a gun in his hand.
 *
 * The three are separate models. The torso is the parent: the gun's placement
 * comes from a bone of the TORSO mesh evaluated at the TORSO's animation time,
 * which is what CBrother::Draw (:134780) does.
 *
 * @param gunIndex Which weapon model to put in his hand, out of every one the
 *        gun templates name. The arrow keys reach the rest.
 * @param spinDegrees Turn the character this far before the first frame.
 * @param screenshotPath When non-empty, save the first frame here and exit.
 * @param advanceMs Run the animation on this far before that first frame.
 */
int RunM37Character(const std::string &bigDirectory, std::uint32_t gunIndex,
                    float spinDegrees, const std::string &screenshotPath,
                    std::uint32_t advanceMs, bool firePreview = false,
                    int armorIndex = -1);

#endif  // GUN_BROS_RE_MILESTONES_M35MESH_H
