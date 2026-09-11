/**
 * @file CMatrix4d.h
 * @brief The 4x4 matrix operations the renderer needs.
 *
 * Named after platform/shared/math/src/CMatrix4d.cpp, but not a port of it --
 * that class carries a full 3D transform stack this project has no caller for
 * yet. Only what the milestones actually use lives here.
 *
 * Matrices are ROW-MAJOR: element [row * 4 + column]. That reads like the
 * matrix on paper, and glUniformMatrix4fv is told to transpose on upload.
 */

#ifndef GUN_BROS_RE_ENGINE_CMATRIX4D_H
#define GUN_BROS_RE_ENGINE_CMATRIX4D_H

// A 4x4 matrix is this many floats. Callers pass plain arrays.
constexpr int kMatrix4dElements = 16;

/**
 * Orthographic projection with the origin at the top left and y growing
 * downward, which is how the game's 2D coordinates run.
 *
 * @param depth Extent kept around z = 0. Sprites sit at z = 0, but models
 *              placed in the same scene are as deep as they are tall.
 * @param out Receives 16 floats.
 */
void Matrix4dOrthoTopLeft(float width, float height, float depth, float *out);

/**
 * Post-multiply a translation onto a matrix built by the above -- moves the
 * world, so a positive x scrolls the view left.
 */
void Matrix4dTranslate(float *matrix, float x, float y);

// ---------------------------------------------------------------------------
// The 3D half, added for the mesh viewer
// ---------------------------------------------------------------------------

void Matrix4dIdentity(float *out);

/**
 * out = left * right, applying right first to a column vector.
 *
 * `out` must not alias either input.
 */
void Matrix4dMultiply(const float *left, const float *right, float *out);

/**
 * Orthographic projection centred on the origin, y DOWN.
 *
 * The mesh path in the original is orthographic too -- DrawHeirarchy (:99070)
 * builds its matrix with SetWidthAndHeightMappedOrthoProjection -- so there is
 * no perspective divide to copy. y grows downward because that is the space
 * CMeshCamera's rotations are written for; see the note in the .cpp.
 *
 * Depth maps [-depth/2, +depth/2] to clip space with **+z toward the viewer**,
 * so a larger z wins the depth test.
 */
void Matrix4dOrthoCentred(float width, float height, float depth, float *out);

void Matrix4dRotationX(float radians, float *out);
void Matrix4dRotationY(float radians, float *out);

/**
 * Rotation about an arbitrary axis, which need not be a unit vector.
 *
 * The engine's CMatrix4dh::Rotate takes an axis as three floats; the parts of
 * an assembled model carry one, for the swing a gun gets when it is aimed.
 * A zero-length axis leaves the identity, since there is no rotation to make.
 */
void Matrix4dRotationAxis(float radians, float axisX, float axisY, float axisZ,
                          float *out);

/**
 * The rotation a quaternion describes, in the engine's sense of it.
 *
 * **This is the TRANSPOSE of the textbook quaternion matrix, on purpose.**
 * DrawHeirarchy (:99070) builds its own inline, sixteen values written in a
 * row, and those values only mean what the engine means by them once you know
 * its matrices are COLUMN-major -- which `math::operator*` (:370909) settles:
 * its first output term is `L[0]*R[0] + L[4]*R[1] + L[8]*R[2] + L[12]*R[3]`,
 * and a row-major multiply would read `L[1]*R[4]` there instead.
 *
 * Copying those sixteen values into this project's ROW-major layout therefore
 * transposes them, and for a rotation a transpose is an inverse -- which is
 * exactly wrong. So the terms below carry the opposite signs, and the two
 * transposes cancel.
 *
 * Symptom when this is wrong, since it is not obvious: parts hang off their
 * bones at the wrong angle, and the further a part reaches from its bone the
 * more it looks like it has been MOVED rather than turned. Parts whose bone
 * sits near zero or near half a turn look right either way, because a half
 * turn is its own inverse.
 *
 * The quaternion is NOT normalised first -- neither the evaluator that
 * produced it nor the original does that, so a blend between two key frames
 * shows up as a slight scale on the attached part.
 */
void Matrix4dFromQuaternion(float x, float y, float z, float w, float *out);

/** Models are authored z-up, so this is the one they spin about. */
void Matrix4dRotationZ(float radians, float *out);
void Matrix4dScale(float scale, float *out);
void Matrix4dTranslation(float x, float y, float z, float *out);

#endif  // GUN_BROS_RE_ENGINE_CMATRIX4D_H
