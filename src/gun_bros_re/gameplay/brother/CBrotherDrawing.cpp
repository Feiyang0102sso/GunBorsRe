/**
 * @file CBrotherDrawing.cpp
 * @brief Pose and draw the assembled player, including torso-driven attachments.
 */

#include "gun_bros_re/gameplay/brother/CBrotherDrawing.h"

#include "engine/core/CMatrix4d.h"
#include "gun_bros_re/gameplay/weapon/CGunDrawing.h"
#include "gun_bros_re/gameplay/armor/CArmorDrawing.h"
#include "engine/graphics/CMeshCamera.h"

#include <cstdio>
#include <cmath>

namespace {
// CBrother::Bind writes 1.0 into this[494] and nothing ever writes it again,
// so the runtime factor in the draw scale is a constant until something in a
// later milestone moves it.
constexpr float kPlayerRuntimeScale = 1.0f;

}

bool CBrother::CreateBuffers(const ZShaderProgram &program) {
    for (std::size_t i = 0; i < m_drawing->parts.size(); ++i) {
        auto &part = *m_drawing->parts[i];
        if (!part.buffer.Create(program) || !part.buffer.SetMesh(*part.mesh)) {
            return false;
        }
    }
    CGun *weaponBanks[] = {weapon.get(), uiOtherWeapon.get()};
    for (CGun *weapon : weaponBanks) {
        if (weapon == nullptr) { continue; }
        if (!weapon->CreateBuffers(program)) { return false; }
    }
    // SetMesh uploads frame zero, which can differ from the script's idle
    // range. Pose now so load/restart/weapon changes never expose that frame.
    UploadPose();
    return true;
}

/** Resolve the actual controller mesh, including an outgoing gun's torso. */
CBrother::TorsoDrawing CBrother::ResolveTorsoDrawing() const {
    const CMesh *mesh = GetTorso().GetAnimation().GetMesh();
    for (auto &part : m_drawing->parts) {
        if (TorsoUsesWeapon()) { break; }
        if (part->mesh.get() == mesh) { return {part->texture.get(), &part->buffer, &part->pose}; }
    }
    CGun *banks[] = {weapon.get(), uiOtherWeapon.get()};
    for (CGun *bank : banks) {
        if (bank == nullptr) { continue; }
        // Identical equipped templates share CPU meshes, but their pose buffers
        // are distinct. Select the controller's bank before matching the model.
        if (GetTorso().GetMoveSet() != &bank->GetTemplate()->GetMoveSet()) { continue; }
        for (auto &part : bank->m_drawing->configs) {
            if (part->mesh.get() == mesh) { return {part->texture.get(), &part->buffer, &part->pose}; }
        }
    }
    for (auto &entry : m_cachedWeapons) {
        // UploadPose can still evaluate the outgoing weapon after a PvP swap.
        // Completed uploads enter the cache atomically; no empty bank is published.
        if (GetTorso().GetMoveSet() != &entry.second->GetTemplate()->GetMoveSet()) { continue; }
        for (auto &part : entry.second->m_drawing->configs) {
            if (part->mesh.get() == mesh) { return {part->texture.get(), &part->buffer, &part->pose}; }
        }
    }
    return {};
}

void CBrother::UploadPose() {
    if (weapon) {
        CMoveSetMeshController &torso = GetTorso();
        const auto part = ResolveTorsoDrawing();
        if (part.buffer != nullptr) {
            if (torso.GetAnimation().Evaluate(*part.pose)) { part.buffer->SetVertices(*part.pose); }
        }
        CMoveSetMeshController &legs = GetLegs();
        const int legsIndex = legs.GetMeshConfigIndex();
        if (legsIndex >= 0) {
            auto &part = *m_drawing->parts[legsIndex];
            if (legs.GetAnimation().Evaluate(part.pose)) { part.buffer.SetVertices(part.pose); }
        }
        CGun *active = &ActiveWeapon();
        auto &gun = active->m_drawing->gunPart;
        if (active->GetAnimation().Evaluate(gun.pose)) { gun.buffer.SetVertices(gun.pose); }
        return;
    }
    CMoveSetMeshController *controllers[] = {&m_torso, &m_legs};
    for (std::size_t i = 0; i < m_drawing->parts.size(); ++i) {
        auto &part = *m_drawing->parts[i];
        if (controllers[i]->GetAnimation().Evaluate(part.pose)) {
            part.buffer.SetVertices(part.pose);
        } else {
            part.buffer.SetFrame(*part.mesh, 0);
        }
    }
}

ZMeshBounds CBrother::GetBounds() const {
    ZMeshBounds combined = ZMeshBounds();
    for (std::size_t i = 0; i < m_drawing->parts.size(); ++i) {
        const auto &part = *m_drawing->parts[i];

        const ZMeshBounds &bounds = part.mesh->GetBounds();
        if (bounds.minX < combined.minX) {
            combined.minX = bounds.minX;
        }
        if (bounds.minY < combined.minY) {
            combined.minY = bounds.minY;
        }
        if (bounds.minZ < combined.minZ) {
            combined.minZ = bounds.minZ;
        }
        if (bounds.maxX > combined.maxX) {
            combined.maxX = bounds.maxX;
        }
        if (bounds.maxY > combined.maxY) {
            combined.maxY = bounds.maxY;
        }
        if (bounds.maxZ > combined.maxZ) {
            combined.maxZ = bounds.maxZ;
        }
    }

    combined.centerX = 0.5f * (combined.minX + combined.maxX);
    combined.centerY = 0.5f * (combined.minY + combined.maxY);
    combined.centerZ = 0.5f * (combined.minZ + combined.maxZ);

    float extent = combined.maxX - combined.minX;
    if (combined.maxY - combined.minY > extent) {
        extent = combined.maxY - combined.minY;
    }
    if (combined.maxZ - combined.minZ > extent) {
        extent = combined.maxZ - combined.minZ;
    }
    if (extent > 0.0f) {
        combined.inverseExtent = 1.0f / extent;
    }
    return combined;
}

bool CBrother::BuildUIMatrix(float centerX, float top, float height,
    float facingRadians, float screenWidth, float screenHeight, float *out) const {
    if (!weapon) { return false; }
    const CMesh *torso = GetTorso().GetAnimation().GetMesh();
    if (torso == nullptr) { return false; }
    const ZMeshBounds &bounds = torso->GetBounds();
    const float torsoHeight = std::abs(bounds.maxZ - bounds.minZ);
    if (torsoHeight <= 0 || height <= 0) { return false; }

    // CBrother::DrawUI :136337-136357 uses mesh mem+68/+80/+92, not
    // combined player bounds or the weapon's extent. These are derived from BIG.
    const float scale = height / torsoHeight;
    const float originY = static_cast<float>(static_cast<int>(top - bounds.centerZ * scale + height + height * 0.5f));
    MeshCameraBuildUIMatrix(centerX, originY, scale, facingRadians, screenWidth, screenHeight, out);
    return true;
}

void CBrother::Draw(const ZShaderProgram &program,
                const float *base) {
    if (weapon && (!IsVisible() || IsImmunityHidden())) { return; }
    UploadPose();
    float flash = 0;
    if (GetVitals() != nullptr) { flash = GetVitals()->flash; }
    if (m_drawing->parts.empty()) {
        return;
    }
    if (weapon) {
        const int legsIndex = GetLegs().GetMeshConfigIndex();
        const auto part = ResolveTorsoDrawing();
        if (part.buffer != nullptr) {
            const ZTexture *texture = part.texture;
            if (armor[1] && armor[1]->m_drawing->images[brotherIndex] != nullptr) {
                texture = armor[1]->m_drawing->images[brotherIndex].get();
            }
            part.buffer->Draw(program, base, *texture, flash);
        }
        if (legsIndex >= 0) {
            auto &part = *m_drawing->parts[legsIndex];
            const ZTexture *texture = part.texture.get();
            // CBrother::Draw :134795 always uses the first legs image.
            if (armor[0] && armor[0]->m_drawing->images[0] != nullptr) {
                texture = armor[0]->m_drawing->images[0].get();
            }
            part.buffer.Draw(program, base, *texture, flash);
        }
        CGun *active = &ActiveWeapon();
        const int handedness = active->GetTemplate()->GetHandedness();
        int count = 1;
        if (handedness == 2) { count = 2; }
        for (int i = 0; i < count; ++i) {
            std::size_t bone = kGunBoneIndex;
            if (handedness == 1 || i == 1) { bone = kGunLeftBoneIndex; }
            ZMeshPart placement;
            if (!GetTorso().GetAnimation().GetNodeAt(bone, placement.attachment)) { continue; }
            float mvp[kMatrix4dElements];
            MeshCameraBuildPartMatrix(placement, base, mvp);
            active->m_drawing->gunPart.buffer.Draw(program, mvp, *active->m_drawing->gunPart.texture, active->GetHeatIntensity());
        }
        // Original order after weapons: head, then both torso attachments.
        const std::uint32_t slots[] = {2, 1};
        for (std::uint32_t slot : slots) {
            if (!armor[slot]) {
                continue;
            }
            CArmor &equippedArmor = *armor[slot];
            std::uint32_t partCount = kArmorVariantCount;
            if (slot == 2) {
                partCount = 1;
            }
            for (std::uint32_t index = 0; index < partCount; ++index) {
                if (!equippedArmor.m_drawing->parts[index]) {
                    continue;
                }
                auto &part = *equippedArmor.m_drawing->parts[index];
                ZMeshPart placement;
                if (!GetTorso().GetAnimation().GetNodeAt(part.boneIndex, placement.attachment)) {
                    continue;
                }
                float mvp[kMatrix4dElements];
                MeshCameraBuildPartMatrix(placement, base, mvp);
                unsigned imageIndex = brotherIndex;
                if (slot == 2) { imageIndex = 0; }
                part.buffer.Draw(program, mvp, *equippedArmor.m_drawing->images[imageIndex], flash);
            }
        }
        return;
    }

    for (auto &part : m_drawing->parts) {
        part->buffer.Draw(program, base, *part->texture);
    }
}

float CBrother::GetWorldScale(float gameScale,
                            float cameraScale) const {
    if (m_drawing->parts.empty()) {
        return 0.0f;
    }

    const float inverseExtent = m_drawing->parts[0]->mesh->GetBounds().inverseExtent;
    if (weapon) {
        const CMesh *torso = GetTorso().GetAnimation().GetMesh();
        if (torso != nullptr) {
            return torso->GetBounds().inverseExtent * kPlayerRuntimeScale * gameScale * cameraScale;
        }
    }
    return inverseExtent * kPlayerRuntimeScale * gameScale * cameraScale;
}

bool CBrother::GetMuzzle(int hand, int node, ZMeshBoneTransform &out) {
    if (!weapon) { return false; }
    ZMeshPart placement;
    std::size_t bone = kGunBoneIndex;
    if (hand == 1) { bone = kGunLeftBoneIndex; }
    if (!GetTorso().GetAnimation().GetNodeAt(bone, placement.attachment)) { return false; }
    ZMeshBoneTransform muzzle{};
    // CGun::FireBullet zero-initializes the node and retains that origin when
    // a model has no named muzzle (for example pack5 gun 60).
    ActiveWeapon().GetAnimation().GetNodeAt(node, muzzle);
    float identity[kMatrix4dElements];
    Matrix4dIdentity(identity);
    float transform[kMatrix4dElements];
    MeshCameraBuildPartMatrix(placement, identity, transform);
    out = muzzle;
    out.posX = transform[0] * muzzle.posX + transform[1] * muzzle.posY + transform[2] * muzzle.posZ + transform[3];
    out.posY = transform[4] * muzzle.posX + transform[5] * muzzle.posY + transform[6] * muzzle.posZ + transform[7];
    out.posZ = transform[8] * muzzle.posX + transform[9] * muzzle.posY + transform[10] * muzzle.posZ + transform[11];
    return true;
}


const std::vector<float> *CBrother::GetTorsoPose() const {
    return ResolveTorsoDrawing().pose;
}
