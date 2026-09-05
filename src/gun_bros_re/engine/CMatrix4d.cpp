/**
 * @file CMatrix4d.cpp
 * @brief The 4x4 matrix operations the 2D renderer needs.
 */

#include "engine/CMatrix4d.h"

void Matrix4dOrthoTopLeft(float width, float height, float *out) {
    for (int i = 0; i < kMatrix4dElements; ++i) {
        out[i] = 0.0f;
    }

    // Map x from [0, width] to [-1, 1].
    out[0] = 2.0f / width;
    out[3] = -1.0f;

    // Map y from [0, height] to [1, -1]: negative so y grows downward.
    out[5] = -2.0f / height;
    out[7] = 1.0f;

    out[10] = -1.0f;
    out[15] = 1.0f;
}

void Matrix4dTranslate(float *matrix, float x, float y) {
    // Row-major, so the translation column is elements 3 and 7. Each row's
    // existing scale applies to the offset being folded in.
    matrix[3] += matrix[0] * x;
    matrix[7] += matrix[5] * y;
}
