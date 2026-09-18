/** @file CEnemyNodes.cpp
 * Original: src/gunbros/enemy.cpp GetNodeLocationChunk :70263 / ARM 0x3d424.
 * Mesh node layout: mesh.bt / CMesh::GetNodeAt. Positions use the selected
 * part's bounds and game scale, without the viewport or damage-circle scale.
 */
#include "gun_bros_re/gameplay/enemy/CEnemy.h"
#include <cmath>

namespace {
constexpr float kHalfDegreesToRadians = 3.14159265f / 360.0f;
constexpr float kTiltCosine = 0.8660254f;
constexpr float kTiltSine = 0.5f;
}

bool CEnemy::GetNodeLocationChunk(int index, int node, float &x, float &y, float &z) const {
    if (data == nullptr || index < 0 || index >= static_cast<int>(m_partCount)) { return false; }
    const Part &part = m_parts[index];
    const auto &animation = part.controller.GetAnimation();
    const CMesh *mesh = animation.GetMesh();
    if (mesh == nullptr) { return false; }
    const auto &bounds = mesh->GetBounds();
    ZMeshBoneTransform position{};
    if (node != -1 && !animation.GetNodeAt(node, position)) { return false; }
    float localX = position.posX - bounds.centerX;
    float localY = position.posY - bounds.centerY;
    float localZ = position.posZ - bounds.centerZ;

    // Original tests the sum of the extra angle and follow-facing byte. The
    // extra visual spin itself is not applied by this node-location function.
    float halfAngle = 0;
    if (part.extraAngleDegrees + static_cast<float>(part.followsFacing) != 0) {
        halfAngle = combat.facing * kHalfDegreesToRadians;
    }
    const float cosine = std::cos(halfAngle);
    const float sine = std::sin(halfAngle);
    float qx = 0, qy = 0, qz = sine, qw = cosine;
    if (part.boneIndex != -1) {
        ZMeshBoneTransform attachment{};
        if (!m_parts[0].controller.GetAnimation().GetNodeAt(part.boneIndex, attachment)) { return false; }
        localX += attachment.posX;
        localY += attachment.posY;
        localZ += attachment.posZ;
        qx = cosine * attachment.rotX - sine * attachment.rotY;
        qy = cosine * attachment.rotY + sine * attachment.rotX;
        qz = cosine * attachment.rotZ + sine * attachment.rotW;
        qw = cosine * attachment.rotW - sine * attachment.rotZ;
    }
    // q * position * conjugate(q), retaining the original interpolated quaternion.
    const float scalar = -qx * localX - qy * localY - qz * localZ;
    const float vectorX = qw * localX + qy * localZ - qz * localY;
    const float vectorY = qw * localY - qx * localZ + qz * localX;
    const float vectorZ = qw * localZ + qx * localY - qy * localX;
    const float turnedX = vectorX * qw - scalar * qx - vectorY * qz + vectorZ * qy + bounds.centerX;
    const float turnedY = vectorY * qw - scalar * qy - vectorZ * qx + vectorX * qz + bounds.centerY;
    const float turnedZ = vectorZ * qw - scalar * qz - vectorX * qy + vectorY * qx + bounds.centerZ;
    const float scale = data->gameScale * bounds.inverseExtent;
    x = combat.x + (turnedX - bounds.centerX) * scale;
    y = combat.y + (turnedY * kTiltCosine - turnedZ * kTiltSine - bounds.centerY) * scale;
    z = (turnedZ * kTiltCosine + turnedY * kTiltSine - bounds.centerZ) * scale;
    return true;
}
