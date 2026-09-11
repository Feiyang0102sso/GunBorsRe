/**
 * @file CMeshCamera.h
 * @brief Where each part of an assembled character sits.
 *
 * Port of the inner loop of CMeshCamera::DrawHeirarchy (src/gunbros/mesh.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:99070 (DrawHeirarchy),
 *            :134780 (CBrother::Draw builds the part table),
 *            :98863 (OrientForUI), :98959 (OrientForGame)
 *
 * **"Hierarchy" is a misnomer.** DrawHeirarchy takes a FLAT array of parts,
 * seventeen dwords each, and draws them one after another against a shared
 * base matrix. Nothing is nested: every part hangs off the same parent.
 *
 * That parent is always PART 0 -- its mesh and its clock. CBrother::Draw
 * (:134780) reads them straight out of part 0's animation controller and
 * passes the same pair to every GetNodeAt it makes, so a character's gun
 * follows the torso's animation, never its own.
 *
 * Three facts about how a player is put together, read off the same function
 * and worth writing down because none of them is guessable:
 *
 * - **The torso and the legs do not attach to anything.** Their part records
 *   carry only a controller and a texture; the attachment is left all zero,
 *   and an all-zero quaternion comes out of the matrix below as the identity.
 *   They line up because they are authored in one space, not because anything
 *   joins them.
 * - **The gun's bone index is hardwired**, not data. The mesh's bone list is
 *   `brother, head, gunleft, shoulder_left01, gun, shoulder_left, back`, and
 *   the code picks index 4 (`gun`) or index 2 (`gunleft`) off the gun's
 *   handedness byte, both hands at once for a two-handed weapon.
 * - **Armour bone indices ARE data**, held as bytes on the character. Those
 *   arrive with the armour system; only the two above are needed to stand a
 *   character up.
 *
 * What is NOT here yet is the base matrix. OrientForUI and OrientForGame both
 * work in screen pixels, on top of SetWidthAndHeightMappedOrthoProjection, so
 * they need the game's camera to mean anything -- that is M4b. Until then the
 * viewer builds its own base and only the part transform below is a port.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CMESHCAMERA_H
#define GUN_BROS_RE_GUN_BROS_CMESHCAMERA_H

#include "engine/graphics/CMesh.h"

// The two bones a gun hangs off, by index into the character mesh's bone list.
// Hardwired in CBrother::Draw (:134780), not read from any template.
constexpr std::size_t kGunBoneIndex = 4;      // "gun", the right hand
constexpr std::size_t kGunLeftBoneIndex = 2;  // "gunleft", the left hand

/**
 * One part of an assembled model.
 *
 * The part the original draws also carries an animation controller, a texture
 * and a tint; those stay with the caller, which is what keeps this header free
 * of both GL and resource loading. What is left is the transform.
 */
struct MeshPart {
    /**
     * Where the parent's bone puts this part.
     *
     * All zero for a part that hangs off nothing -- the zero quaternion is the
     * identity here, which is exactly how the torso and the legs are drawn.
     */
    MeshBoneTransform attachment;

    /**
     * An extra turn on top of the bone's own, in DEGREES about an arbitrary
     * axis. The engine's Rotate takes degrees; this is the swing an aimed gun
     * gets. An angle of zero means no extra turn and the axis is ignored.
     */
    float extraAngleDegrees;
    float extraAxisX, extraAxisY, extraAxisZ;

    MeshPart();
};

/**
 * The matrix one part is drawn with: `base * translate * rotate * extra`.
 *
 * The order is DrawHeirarchy's: Translate, then post-multiply the bone's
 * rotation, then post-multiply the extra turn if there is one.
 *
 * @param base Row-major, shared by every part of the model.
 * @param out  Row-major, 16 floats. Must not alias `base`.
 */
void MeshCameraBuildPartMatrix(const MeshPart &part, const float *base,
                               float *out);

#endif  // GUN_BROS_RE_GUN_BROS_CMESHCAMERA_H
