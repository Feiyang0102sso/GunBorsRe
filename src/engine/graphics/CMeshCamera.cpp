/**
 * @file CMeshCamera.cpp
 * @brief Where each part of an assembled character sits.
 */

#include "engine/graphics/CMeshCamera.h"

#include "engine/core/ZMatrix4d.h"

namespace {

// The engine's Rotate takes degrees; CMatrix4d works in radians.
constexpr float kDegreesToRadians = 3.14159265f / 180.0f;

}  // namespace

ZMeshPart::ZMeshPart()
    : attachment(),
      extraAngleDegrees(0.0f),
      extraAxisX(0.0f),
      extraAxisY(0.0f),
      extraAxisZ(0.0f) {}

void MeshCameraBuildPartMatrix(const ZMeshPart &part, const float *base,
                               float *out) {
    float translation[kMatrix4dElements];
    Matrix4dTranslation(part.attachment.posX, part.attachment.posY,
                        part.attachment.posZ, translation);

    float rotation[kMatrix4dElements];
    Matrix4dFromQuaternion(part.attachment.rotX, part.attachment.rotY,
                           part.attachment.rotZ, part.attachment.rotW, rotation);

    float placed[kMatrix4dElements];
    Matrix4dMultiply(translation, rotation, placed);

    if (part.extraAngleDegrees != 0.0f) {
        float extra[kMatrix4dElements];
        Matrix4dRotationAxis(part.extraAngleDegrees * kDegreesToRadians,
                             part.extraAxisX, part.extraAxisY, part.extraAxisZ,
                             extra);

        float turned[kMatrix4dElements];
        Matrix4dMultiply(placed, extra, turned);
        Matrix4dMultiply(base, turned, out);
        return;
    }

    Matrix4dMultiply(base, placed, out);
}

namespace {
// The lean CBrother::Draw asks OrientForGame for, same as an enemy's.
// Reference: :98959.
constexpr float kGameTiltDegrees = 30.0f;

}

void MeshCameraBuildGameMatrix(const float *base, float x, float y, float scale,
                           float facingDegrees, float *out) {
    float step[kMatrix4dElements];
    float accumulated[kMatrix4dElements];
    float next[kMatrix4dElements];

    // base * translate(x, y)
    Matrix4dTranslation(x, y, 0.0f, step);
    Matrix4dMultiply(base, step, accumulated);

    // * scale
    Matrix4dScale(scale, step);
    Matrix4dMultiply(accumulated, step, next);

    // * rotateX(30), about the origin because CBrother::Draw passes no pivot
    Matrix4dRotationX(kGameTiltDegrees * kDegreesToRadians, step);
    Matrix4dMultiply(next, step, accumulated);

    // * rotateZ(facing)
    Matrix4dRotationZ(facingDegrees * kDegreesToRadians, step);
    Matrix4dMultiply(accumulated, step, out);
}

void MeshCameraBuildUIMatrix(float centerX, float originY, float scale, float facingRadians,
    float screenWidth, float screenHeight, float *out) {
    float projection[16], translation[16], scaling[16], tilt[16], facing[16], first[16], second[16];
    // CGraphics2d_OGLES::SetWidthAndHeightMappedOrthoProjection :378895:
    // original near/far = 0/32767; OrientForUI :98920 places the mesh at -500.
    Matrix4dOrthoTopLeft(screenWidth, screenHeight, 32767, projection);
    projection[11] = -1;
    Matrix4dTranslation(static_cast<float>(static_cast<int>(centerX)), originY, -500, translation);
    Matrix4dScale(scale, scaling);
    Matrix4dRotationX(3.14159265f * 0.5f, tilt);
    Matrix4dRotationZ(3.14159265f + facingRadians, facing);
    Matrix4dMultiply(projection, translation, first);
    Matrix4dMultiply(first, scaling, second);
    Matrix4dMultiply(second, tilt, first);
    Matrix4dMultiply(first, facing, out);
}
