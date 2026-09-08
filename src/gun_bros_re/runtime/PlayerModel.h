/**
 * @file PlayerModel.h
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

#ifndef GUN_BROS_RE_MILESTONES_PLAYERMODEL_H
#define GUN_BROS_RE_MILESTONES_PLAYERMODEL_H

#include "engine/CMeshBuffer.h"
#include "engine/CShaderProgram.h"
#include "engine/CTexture.h"
#include "gun_bros/CMesh.h"
#include "gun_bros/CMoveSetMesh.h"
#include "gun_bros/CMoveSetMeshController.h"
#include "runtime/PackTables.h"
#include "gun_bros/CBrother.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// The player's move set names its models in this order, and CBrother::Draw
// draws them as parts 0 and 1 with no attachment between them.
constexpr std::uint8_t kPlayerTorsoConfigIndex = 0;
constexpr std::uint8_t kPlayerLegsConfigIndex = 1;

/**
 * Fetch one model and the atlas it wears, and report what came out.
 *
 * The one place a mesh ordinal and an image ordinal turn into something
 * drawable.
 *
 * @param label Printed with the result, so a failure names its owner.
 */
bool LoadMeshAndAtlas(PackTables &tables, const char *label,
                      std::uint32_t meshPackHash, std::uint32_t meshOrdinal,
                      std::uint32_t imagePackHash, std::uint32_t imageOrdinal,
                      CMesh &mesh, CTexture &texture);

/** One drawable piece of an assembled player. */
struct PlayerPart {
    std::string name;

    CMesh mesh;
    CTexture texture;
    CMeshBuffer buffer;

    // Empty `moves` for a part with no animation of its own -- a gun model is
    // a single frame, and its pose comes entirely from the hand it hangs off.
    CMoveSetMeshController controller;
    std::vector<const CMesh *> configMeshes;
    std::vector<std::int32_t> moves;
    std::size_t moveSlot;
    std::vector<float> pose;

    // Which bone of the TORSO mesh this hangs off, when `attached`.
    bool attached;
    std::size_t boneIndex;

    PlayerPart() : moveSlot(0), attached(false), boneIndex(0) {}
};

/** Equipped state owns the script inputs so every runtime pointer stays valid. */
struct PlayerWeaponState {
    CScript playerScript;
    CGun::Template data;
    std::vector<std::unique_ptr<PlayerPart>> configs;
    PlayerPart gunPart;
    CGun gun;
    CBrother brother;
};

/**
 * A player and his parts.
 *
 * Held by pointer per part because a CMeshBuffer owns a GL name and cannot be
 * copied or moved, and because `configMeshes` points back at its own `mesh`.
 */
struct PlayerModel {
    // Owned here so the controllers can point at it.
    CMoveSetMesh moveSet;
    std::vector<std::unique_ptr<PlayerPart>> parts;
    std::unique_ptr<PlayerWeaponState> weapon;
};

/**
 * The one player template in the archives, with the scale it draws at.
 *
 * `gameScale` is template offset 112: CBrother::Bind (:135608) copies it to
 * this[495] and CBrother::Draw multiplies it into the draw scale, exactly
 * where an enemy's template word 66 goes.
 */
struct PlayerTemplateData {
    std::uint32_t packHash;
    std::uint32_t ordinal;
    std::string owner;

    CMoveSetMesh moveSet;
    float gameScale;
    CScript script;

    PlayerTemplateData();
};

/**
 * Find the player template, whichever pack it lives in.
 *
 * There is exactly one in the whole library, so the first hit is the answer.
 *
 * @return false when no pack carries a readable one.
 */
bool FindPlayerTemplate(CResTOCManager &tocManager, PackTables &tables,
                        PlayerTemplateData &out);

/**
 * Build the torso and the legs. No weapon: that is a separate call.
 *
 * @return false when the set has fewer than the two configs a player needs.
 */
bool BuildPlayerBody(PackTables &tables, const CMoveSetMesh &moveSet,
                     PlayerModel &out);

/** Equip the actual template, including its player move overrides and scripts. */
bool EquipPlayerWeapon(PackTables &tables, const CScript &playerScript,
    const CGun::Template &weapon, const std::string &owner, PlayerModel &out);
void SetPlayerInput(PlayerModel &model, bool moving, bool shooting);
/** Compose a muzzle in the same raw coordinate space as DrawPlayer. */
bool GetPlayerMuzzle(PlayerModel &model, int hand, int node, MeshBoneTransform &out);

/**
 * Hang a weapon model off the torso's gun bone.
 *
 * The bone index is the one hardwired number in the whole assembly:
 * CBrother::Draw picks bone 4 off the torso mesh for a one-handed weapon, and
 * the torso's bone list names bone 4 "gun".
 */
bool AttachPlayerGun(PackTables &tables, const std::string &owner,
                     std::uint32_t meshPackHash, std::uint32_t meshOrdinal,
                     std::uint32_t imagePackHash, std::uint32_t imageOrdinal,
                     PlayerModel &out);

/** Create the GL buffers for every part. */
bool CreatePlayerBuffers(PlayerModel &model, const CShaderProgram &program);

/** Move every animated part's clock on, and rewrite its vertices. */
void AdvancePlayer(PlayerModel &model, std::int32_t deltaMs);

/** Put every part's current pose in its buffer. Still parts show frame 0. */
void PosePlayer(PlayerModel &model);

/**
 * The box the whole player occupies.
 *
 * Only the parts that hang off nothing count: an attached part sits inside the
 * body anyway, and letting a long rifle drive the framing would make the
 * character shrink every time the gun changed.
 */
MeshBounds PlayerBounds(const PlayerModel &model);

/**
 * Draw every part against one base matrix.
 *
 * @param base Row-major, and it must already carry the scale: the vertices go
 *        in raw.
 */
void DrawPlayer(PlayerModel &model, const CShaderProgram &program,
                const float *base);

/**
 * Point every animated part at the same slot of its own move list.
 *
 * A viewer convention, not something the data says: the torso's moves and the
 * legs' moves are separate lists, and the game picks one of each independently
 * -- aim with the arms, walk with the feet. Stepping them together is just the
 * cheapest way to see a whole character move.
 *
 * @param report Print what each part landed on.
 */
void SelectPlayerMoveSlot(PlayerModel &model, std::size_t slot, bool report);

/**
 * How much to scale a player's RAW vertices by to put him in the world.
 *
 * `torso.inverseExtent * runtimeScale * gameScale * cameraScale`, read off
 * CBrother::Draw (:134960): the inverse extent comes from the mesh at
 * this[454] -- the TORSO, not the combined body -- and the runtime factor
 * this[494] is 1 from CBrother::Bind onwards. Same shape as an enemy's, and
 * the same trap: the product applies to raw vertices, because the inverse
 * extent in it is what normalises them.
 *
 * @return 0 when there is no torso to measure.
 */
float PlayerModelWorldScale(const PlayerModel &model, float gameScale,
                            float cameraScale);

/**
 * The matrix CBrother::Draw stands a player up with, via OrientForGame.
 *
 * The same term-for-term chain as an enemy's, except for the pivot:
 * CEnemy::Draw passes its own mesh centre, CBrother::Draw passes null and lets
 * the camera's default stand in. Null is taken here to mean the origin.
 *
 * @param out Row-major, 16 floats. Must not alias `base`.
 */
void BuildPlayerGameMatrix(const float *base, float x, float y, float scale,
                           float facingDegrees, float *out);

#endif  // GUN_BROS_RE_MILESTONES_PLAYERMODEL_H
