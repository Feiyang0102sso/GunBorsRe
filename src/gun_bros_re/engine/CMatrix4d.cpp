/**
 * @file CMatrix4d.cpp
 * @brief The 4x4 matrix operations the renderer needs.
 */

#include "engine/CMatrix4d.h"

#include <cmath>

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

void Matrix4dIdentity(float *out) {
    for (int i = 0; i < kMatrix4dElements; ++i) {
        out[i] = 0.0f;
    }
    out[0] = 1.0f;
    out[5] = 1.0f;
    out[10] = 1.0f;
    out[15] = 1.0f;
}

void Matrix4dMultiply(const float *left, const float *right, float *out) {
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += left[row * 4 + k] * right[k * 4 + column];
            }
            out[row * 4 + column] = sum;
        }
    }
}

void Matrix4dOrthoCentred(float width, float height, float depth, float *out) {
    Matrix4dIdentity(out);
    out[0] = 2.0f / width;

    // Negative for the same reason Matrix4dOrthoTopLeft is: the engine's
    // screen space has y growing downward, and the mesh camera's rotations
    // are written for that space. Flip this and every model stands on its
    // head.
    out[5] = -2.0f / height;

    // Negative so that +z comes toward the viewer: GL keeps the smaller depth
    // value, and this sends a larger z to a smaller one.
    out[10] = -2.0f / depth;
}

void Matrix4dRotationX(float radians, float *out) {
    Matrix4dIdentity(out);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    out[5] = c;
    out[6] = -s;
    out[9] = s;
    out[10] = c;
}

void Matrix4dRotationY(float radians, float *out) {
    Matrix4dIdentity(out);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    out[0] = c;
    out[2] = s;
    out[8] = -s;
    out[10] = c;
}

void Matrix4dRotationAxis(float radians, float axisX, float axisY, float axisZ,
                          float *out) {
    Matrix4dIdentity(out);

    const float length =
        std::sqrt(axisX * axisX + axisY * axisY + axisZ * axisZ);
    if (length == 0.0f) {
        return;
    }

    const float x = axisX / length;
    const float y = axisY / length;
    const float z = axisZ / length;
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    const float t = 1.0f - c;

    out[0] = t * x * x + c;
    out[1] = t * x * y - s * z;
    out[2] = t * x * z + s * y;

    out[4] = t * x * y + s * z;
    out[5] = t * y * y + c;
    out[6] = t * y * z - s * x;

    out[8] = t * x * z - s * y;
    out[9] = t * y * z + s * x;
    out[10] = t * z * z + c;
}

void Matrix4dFromQuaternion(float x, float y, float z, float w, float *out) {
    Matrix4dIdentity(out);

    // Every off-diagonal term has the sign OPPOSITE to the textbook formula,
    // which makes this the transpose of it -- see the header for why that is
    // the right matrix and not a mistake.
    out[0] = 1.0f - 2.0f * (y * y + z * z);
    out[1] = 2.0f * (x * y + z * w);
    out[2] = 2.0f * (x * z - y * w);

    out[4] = 2.0f * (x * y - z * w);
    out[5] = 1.0f - 2.0f * (x * x + z * z);
    out[6] = 2.0f * (y * z + x * w);

    out[8] = 2.0f * (x * z + y * w);
    out[9] = 2.0f * (y * z - x * w);
    out[10] = 1.0f - 2.0f * (x * x + y * y);
}

void Matrix4dRotationZ(float radians, float *out) {
    Matrix4dIdentity(out);
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    out[0] = c;
    out[1] = -s;
    out[4] = s;
    out[5] = c;
}

void Matrix4dScale(float scale, float *out) {
    Matrix4dIdentity(out);
    out[0] = scale;
    out[5] = scale;
    out[10] = scale;
}

void Matrix4dTranslation(float x, float y, float z, float *out) {
    Matrix4dIdentity(out);
    out[3] = x;
    out[7] = y;
    out[11] = z;
}
