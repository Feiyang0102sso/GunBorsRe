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
 * @param out Receives 16 floats.
 */
void Matrix4dOrthoTopLeft(float width, float height, float *out);

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

/** Models are authored z-up, so this is the one they spin about. */
void Matrix4dRotationZ(float radians, float *out);
void Matrix4dScale(float scale, float *out);
void Matrix4dTranslation(float x, float y, float z, float *out);

#endif  // GUN_BROS_RE_ENGINE_CMATRIX4D_H
