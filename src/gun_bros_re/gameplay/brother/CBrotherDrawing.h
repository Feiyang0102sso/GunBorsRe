/**
 * @file CBrotherDrawing.h
 * @brief The player, assembled out of the models a player actually is.
 *
 * A Gun Bro is not one model. CBrother::Draw (:134780) builds a flat part
 * table -- torso, legs, and whatever is in his hands -- and hands it to
 * CMeshCamera::DrawHeirarchy. The torso is the parent: every attachment is a
 * bone of the TORSO mesh read at the TORSO's animation time, so the gun
 * follows the arms and never its own clock.
 *
 * Shared by the two places a player appears: the M3.7 viewer, which puts one
 * on a turntable with a weapon in his hand, and the M3 map viewer, which
 * stands him on the spawn point the object layer names. Same file for the same
 * reason EnemyModel.h is one file -- the assembly is the interesting part and
 * only the base matrix differs.
 *
 * **Torso and legs attach to nothing.** Their part records leave the
 * attachment all zero, and an all-zero quaternion comes out of the part matrix
 * as the identity. They line up because they were authored in one space.
 */

#pragma once
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "engine/graphics/ZMeshBuffer.h"
#include "engine/graphics/ZTexture.h"

// The player's move set names its models in this order, and CBrother::Draw
// draws them as parts 0 and 1 with no attachment between them.
constexpr std::uint8_t kPlayerTorsoConfigIndex = 0;
constexpr std::uint8_t kPlayerLegsConfigIndex = 1;

/** Actor-owned body meshes; animation controllers stay on CBrother. */
struct CBrother::Drawing {
    struct BodyMesh {
        std::shared_ptr<const CMesh> mesh;
        std::shared_ptr<ZTexture> texture;
        ZMeshBuffer buffer;
        std::vector<float> pose;
    };
    std::vector<std::unique_ptr<BodyMesh>> parts;
};

/** A temporary view of the actual torso; it never owns or extends its lifetime. */
struct CBrother::TorsoDrawing {
    ZTexture *texture = nullptr;
    ZMeshBuffer *buffer = nullptr;
    std::vector<float> *pose = nullptr;
};
