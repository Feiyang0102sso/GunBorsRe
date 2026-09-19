#include "gun_bros_viewer/scenes/ZMapViewer.h"
#include "gun_bros_viewer/scenes/BrotherPreview.h"
/** Bare mesh exploration belongs to the viewer, outside the actor lifecycle. */
#include "gun_bros_re/graphics/ZMeshAssets.h"
#include "engine/core/ZMatrix4d.h"
#include "engine/graphics/CMeshCamera.h"
#include <cstdio>
#include "gun_bros_re/gameplay/map/CMapInternal.h"

bool ZBrotherPreview::AttachGun(CGunBros &tables, const std::string &owner,
                     std::uint32_t meshPackHash, std::uint32_t meshOrdinal,
                     std::uint32_t imagePackHash, std::uint32_t imageOrdinal) {
    const CMesh *torsoMesh = body.GetTorso().GetAnimation().GetMesh();
    if (torsoMesh == nullptr) {
        std::printf("[player] no torso to hang %s off\n", owner.c_str());
        return false;
    }

    auto gun = std::make_unique<Gun>();
    if (!LoadMeshAndAtlas(tables, owner.c_str(), meshPackHash, meshOrdinal,
                          imagePackHash, imageOrdinal, gun->mesh, gun->texture)) {
        return false;
    }

    const CMesh &torso = *torsoMesh;
    gun->attached = true;
    gun->boneIndex = kGunBoneIndex;
    if (kGunBoneIndex < torso.GetBoneNames().size()) {
        std::printf("[player] gun hangs off bone %zu, named \"%s\"\n",
                    kGunBoneIndex, torso.GetBoneNames()[kGunBoneIndex].c_str());
    } else {
        std::printf("[player] torso mesh has no bone %zu; the gun will sit at "
                    "the origin\n",
                    kGunBoneIndex);
        gun->attached = false;
    }

    m_gun = std::move(gun);
    return true;
}

/** Which moves of the set drive one config, by index into the set. */
static std::vector<std::int32_t> MovesForConfig(const CMoveSetMesh &moveSet, unsigned configIndex) {
    std::vector<std::int32_t> moves;
    for (std::size_t i = 0; i < moveSet.GetMoves().size(); ++i) {
        if (moveSet.GetMoves()[i].meshConfigIndex == configIndex) {
            moves.push_back(static_cast<std::int32_t>(i));
        }
    }
    return moves;
}

void SelectPlayerMoveSlot(CBrother &model, std::size_t slot, bool report) {
    // Equipped actors are driven by the original player script, independently
    // for torso and legs. The old manual slot browser remains for bare bodies.
    if (model.weapon) { return; }
    CMoveSetMeshController *controllers[] = {&model.GetTorso(), &model.GetLegs()};
    const char *names[] = {"torso", "legs"};
    for (unsigned config = 0; config < 2; ++config) {
        // The controller gets the whole config array, as the original's does.
        // Moves naming other configs are filtered out rather than left to fail.
        const auto moves = MovesForConfig(model.moveSet, config);
        if (moves.empty()) { continue; }
        const std::size_t moveSlot = slot % moves.size();
        auto &controller = *controllers[config];
        if (!controller.SetMove(moves[moveSlot])) {
            auto &animation = controller.GetAnimation();
            animation.SetTimeMs(animation.GetRangeStartMs());
        }
        if (!report) { continue; }
        const auto &move = model.moveSet.GetMoves()[moves[moveSlot]];
        std::printf("[player] %s: move %d (%zu of %zu) -- frames %u..%u, %d ms\n",
            names[config], moves[moveSlot], moveSlot + 1, moves.size(),
            move.firstFrame, move.lastFrame, controller.GetAnimation().GetRangeDurationMs());
    }
}

void AdvanceBrotherPreview(CBrother &model, std::int32_t deltaMs) {
    if (model.weapon) {
        model.Update(deltaMs);
        return;
    }
    model.GetTorso().Update(deltaMs);
    model.GetLegs().Update(deltaMs);
}

bool ZBrotherPreview::CreateBuffers(const ZShaderProgram &program) {
    if (!body.CreateBuffers(program)) { return false; }
    if (!m_gun) { return true; }
    return m_gun->buffer.Create(program) && m_gun->buffer.SetMesh(m_gun->mesh);
}

void ZBrotherPreview::Update(std::int32_t deltaMs) {
    AdvanceBrotherPreview(body, deltaMs);
}

void ZBrotherPreview::Draw(const ZShaderProgram &program, const float *base) {
    body.Draw(program, base);
    if (!m_gun) { return; }
    ZMeshPart placement;
    if (m_gun->attached) {
        body.GetTorso().GetAnimation().GetNodeAt(m_gun->boneIndex, placement.attachment);
    }
    float matrix[kMatrix4dElements];
    MeshCameraBuildPartMatrix(placement, base, matrix);
    m_gun->buffer.Draw(program, matrix, m_gun->texture);
}

namespace MapDetail {
/** Move every placed player's animation on. */
void AdvancePlayers(CMap &loaded, std::int32_t deltaMs) {
    for (std::size_t i = 0; i < loaded.GetResources().players.size(); ++i) {
        AdvanceBrotherPreview(*loaded.GetResources().players[i].model, deltaMs);
    }
}
/**
 * Run the animation clock forward, in the bites playback would use.
 *
 * What makes a still screenshot able to prove anything about animation: shoot
 * the same map at two different times and diff them. Deterministic, because
 * the bite size is fixed rather than taken from the wall clock.
 */
void WarmUp(CMap &loaded, std::uint32_t totalMs,
            CLevel *effects , bool firing ) {
    if (effects && !loaded.GetResources().players.empty()) { loaded.GetResources().players[0].model->SetInput(false, firing); }
    for (std::uint32_t elapsed = 0; elapsed < totalMs; elapsed += kWarmUpFrameMs) {
        AdvanceProps(loaded.GetResources().props, kWarmUpFrameMs);
        loaded.UpdateLayers(kWarmUpFrameMs);
        AdvanceEnemies(loaded, kWarmUpFrameMs);
        AdvancePlayers(loaded, kWarmUpFrameMs);
        if (effects && !loaded.GetResources().players.empty()) {
            CMap::Resources::Player &player = loaded.GetResources().players[0];
            float identity[kMatrix4dElements], modelToWorld[kMatrix4dElements];
            Matrix4dIdentity(identity);
            const float scale = player.model->GetWorldScale(loaded.GetResources().playerTemplate->GetGameScale(), kLevelCameraScale);
            MeshCameraBuildGameMatrix(identity, player.x, player.y, scale, player.facingDegrees, modelToWorld);
            effects->Update(*player.model, modelToWorld, player.facingDegrees, kWarmUpFrameMs, &loaded.GetResources().weaponCollision);
        }
    }
}


bool UpdateControlledPlayer(CMap &loaded, const ZWindow &window,
                            std::uint64_t elapsedMs) {
    if (loaded.GetResources().players.empty()) {
        return false;
    }

    float directionX = 0.0f;
    float directionY = 0.0f;
    if (window.IsKeyDown(ZKeyCode::A)) {
        directionX -= 1.0f;
    }
    if (window.IsKeyDown(ZKeyCode::D)) {
        directionX += 1.0f;
    }
    if (window.IsKeyDown(ZKeyCode::W)) {
        directionY -= 1.0f;
    }
    if (window.IsKeyDown(ZKeyCode::S)) {
        directionY += 1.0f;
    }

    const bool moving = directionX != 0.0f || directionY != 0.0f;
    CMap::Resources::Player &player = loaded.GetResources().players[0];
    if (moving != player.moving) {
        // The player data interleaves torso and leg moves. Slot zero is the
        // spawn/idle pair and slot one is the first locomotion pair.
        const std::size_t moveSlot = moving ? 1 : 0;
        SelectPlayerMoveSlot(*player.model, moveSlot, false);
        player.moving = moving;
    }

    if (!moving || elapsedMs == 0) {
        return moving;
    }

    const float directionLength = std::sqrt(directionX * directionX +
                                            directionY * directionY);
    directionX /= directionLength;
    directionY /= directionLength;
    player.facingDegrees = std::atan2(directionY, directionX) * kRadiansToDegrees + 90.0f;

    const float elapsedSeconds = static_cast<float>(elapsedMs) * 0.001f;
    ZCollisionPoint movement(directionX * kPlayerMovementUnitsPerSecond *
                                elapsedSeconds,
                            directionY * kPlayerMovementUnitsPerSecond *
                                elapsedSeconds);
    ZCollisionPoint resolved = loaded.GetResources().collisionScene.ResolveCircleMovement(
        ZCollisionPoint(player.x, player.y), movement, kPlayerCollisionRadius);

    // CPlayer::Move clamps the body to the active camera bounds before it
    // resolves collision. Keep the whole circle inside the same rectangle.
    const CLayerCamera::Rectangle bounds = loaded.GetVisibleBounds();
    if (!bounds.IsEmpty()) {
        const float minimumX = static_cast<float>(bounds.x) +
                               kPlayerCollisionRadius;
        const float maximumX = static_cast<float>(bounds.x + bounds.width) -
                               kPlayerCollisionRadius;
        const float minimumY = static_cast<float>(bounds.y) +
                               kPlayerCollisionRadius;
        const float maximumY = static_cast<float>(bounds.y + bounds.height) -
                               kPlayerCollisionRadius;
        if (resolved.x < minimumX) {
            resolved.x = minimumX;
        }
        if (resolved.x > maximumX) {
            resolved.x = maximumX;
        }
        if (resolved.y < minimumY) {
            resolved.y = minimumY;
        }
        if (resolved.y > maximumY) {
            resolved.y = maximumY;
        }
    }

    player.x = resolved.x;
    player.y = resolved.y;
    return true;
}


}
