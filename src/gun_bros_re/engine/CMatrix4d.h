/**
 * @file CMatrix4d.h
 * @brief The 4x4 matrix operations the 2D renderer needs.
 *
 * Named after platform/shared/math/src/CMatrix4d.cpp, but not a port of it --
 * that class carries a full 3D transform stack this project has no caller for
 * yet. Only what M2 and M3 actually use lives here; it grows when the mesh
 * work arrives.
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

#endif  // GUN_BROS_RE_ENGINE_CMATRIX4D_H
