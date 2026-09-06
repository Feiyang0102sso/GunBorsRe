/**
 * @file CMeshCamera.cpp
 * @brief Where each part of an assembled character sits.
 */

#include "gun_bros/CMeshCamera.h"

#include "engine/CMatrix4d.h"

namespace {

// The engine's Rotate takes degrees; CMatrix4d works in radians.
constexpr float kDegreesToRadians = 3.14159265f / 180.0f;

}  // namespace

MeshPart::MeshPart()
    : attachment(),
      extraAngleDegrees(0.0f),
      extraAxisX(0.0f),
      extraAxisY(0.0f),
      extraAxisZ(0.0f) {}

void MeshCameraBuildPartMatrix(const MeshPart &part, const float *base,
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
