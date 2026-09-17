/**
 * @file ZPlayerModel.h
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

#ifndef GUN_BROS_RE_MILESTONES_ZPLAYERMODEL_H
#define GUN_BROS_RE_MILESTONES_ZPLAYERMODEL_H

#include "engine/graphics/ZMeshBuffer.h"
#include "gun_bros_re/gameplay/CArmor.h"
#include "engine/graphics/ZShaderProgram.h"
#include "engine/graphics/ZTexture.h"
#include "engine/graphics/CMesh.h"
#include "engine/graphics/CMoveSetMesh.h"
#include "engine/graphics/CMoveSetMeshController.h"
#include "gun_bros_re/data/ZPackTables.h"
#include "gun_bros_re/gameplay/brother/CBrother.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>

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
bool LoadMeshAndAtlas(ZPackTables &tables, const char *label,
                      std::uint32_t meshPackHash, std::uint32_t meshOrdinal,
                      std::uint32_t imagePackHash, std::uint32_t imageOrdinal,
                      CMesh &mesh, ZTexture &texture, const CMoveSetMesh *moveSet = nullptr);

/** One drawable piece of an assembled player. */
struct ZPlayerPart {
    std::string name;

    CMesh mesh;
    ZTexture texture;
    ZMeshBuffer buffer;

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

    ZPlayerPart() : moveSlot(0), attached(false), boneIndex(0) {}
};

/** Equipped state owns the script inputs so every runtime pointer stays valid. */
struct ZPlayerWeaponState {
    CScript playerScript;
    CGun::Template data;
    std::vector<std::unique_ptr<ZPlayerPart>> configs;
    ZPlayerPart gunPart;
    CGun gun;
    CBrother brother;
};

/** Each equipment slot owns its template, script state and optional attachments. */
struct ZPlayerArmorState {
    CArmor::Template data;
    CArmor armor;
    ZTexture images[kArmorVariantCount];
    std::unique_ptr<ZPlayerPart> parts[kArmorVariantCount];
};

/**
 * A player and his parts.
 *
 * Held by pointer per part because a CMeshBuffer owns a GL name and cannot be
 * copied or moved, and because `configMeshes` points back at its own `mesh`.
 */
struct ZPlayerModel {
    std::uint32_t powerupChoice = 0; // Desktop Bot input stream; never effect data.
    unsigned friendCount = 0;
    // Owned here so the controllers can point at it.
    CMoveSetMesh moveSet;
    std::vector<std::unique_ptr<ZPlayerPart>> parts;
    std::unique_ptr<ZPlayerWeaponState> weapon;
    // CBrother owns two guns in the original. UI keeps both mesh banks alive
    // while the old torso finishes raising after native 3 switches the gun.
    std::unique_ptr<ZPlayerWeaponState> uiOtherWeapon;
    ZPlayerWeaponState *uiActiveWeapon = nullptr;
    std::unique_ptr<ZPlayerArmorState> armor[kArmorSlotCount];
    ZPlayerVitals *vitals = nullptr;
    CBrother::PowerupState powerups;
    bool human = true;
    bool cooperative = false;
    bool deathmatch = false;
    // Stable gun/mesh banks keep torso sequences valid across PvP swaps.
    std::map<std::uint64_t, std::unique_ptr<ZPlayerWeaponState>> matchWeapons;
    GameObjectRef gunResource;
    unsigned gunSlot = 0; // Retained on projectiles after the player switches guns.
    unsigned masteryExperience = 0;
    std::map<std::uint64_t, unsigned> masteryByWeapon;
    unsigned brotherIndex = 0;
    /** Both menu and combat retain the outgoing torso until its move ends. */
    ZPlayerWeaponState &ActiveWeapon() const {
        if (uiActiveWeapon != nullptr) { return *uiActiveWeapon; }
        return *weapon;
    }
};

/**
 * The one player template in the archives, with the scale it draws at.
 *
 * `gameScale` is template offset 112: CBrother::Bind (:135608) copies it to
 * this[495] and CBrother::Draw multiplies it into the draw scale, exactly
 * where an enemy's template word 66 goes.
 */
struct ZPlayerTemplateData {
    std::uint32_t packHash;
    std::uint32_t ordinal;
    std::string owner;

    CMoveSetMesh moveSet;
    float gameScale;
    CScript script;

    ZPlayerTemplateData();
};

/**
 * Find the player template, whichever pack it lives in.
 *
 * There is exactly one in the whole library, so the first hit is the answer.
 *
 * @return false when no pack carries a readable one.
 */
bool FindPlayerTemplate(CResTOCManager &tocManager, ZPackTables &tables,
                        ZPlayerTemplateData &out);

/**
 * Build the torso and the legs. No weapon: that is a separate call.
 *
 * @return false when the set has fewer than the two configs a player needs.
 */
bool BuildPlayerBody(ZPackTables &tables, const CMoveSetMesh &moveSet,
                     ZPlayerModel &out);

/** Equip the actual template, including its player move overrides and scripts. */
bool EquipPlayerWeapon(ZPackTables &tables, const CScript &playerScript,
    const CGun::Template &weapon, const std::string &owner, ZPlayerModel &out);
/** Load the second UI gun from BIG; the primary brother remains the sole host. */
bool PreparePlayerUIWeapon(ZPackTables &tables, const CGun::Template &weapon,
    const std::string &owner, ZPlayerModel &out);
void SelectPlayerUIWeapon(ZPlayerModel &model, bool primary);
/** Replace only the template's own armour slot; other equipment stays equipped. */
bool EquipPlayerArmor(ZPackTables &tables, const CArmor::Template &data,
    const ZShaderProgram &program, ZPlayerModel &out);
void ClearPlayerArmor(ZPlayerModel &model);
float PlayerArmorMultiplier(const ZPlayerModel &model, std::uint32_t attribute);
void SetPlayerInput(ZPlayerModel &model, bool moving, bool shooting);
/** Compose a muzzle in the same raw coordinate space as DrawPlayer. */
bool GetPlayerMuzzle(ZPlayerModel &model, int hand, int node, ZMeshBoneTransform &out);

/**
 * Hang a weapon model off the torso's gun bone.
 *
 * The bone index is the one hardwired number in the whole assembly:
 * CBrother::Draw picks bone 4 off the torso mesh for a one-handed weapon, and
 * the torso's bone list names bone 4 "gun".
 */
bool AttachPlayerGun(ZPackTables &tables, const std::string &owner,
                     std::uint32_t meshPackHash, std::uint32_t meshOrdinal,
                     std::uint32_t imagePackHash, std::uint32_t imageOrdinal,
                     ZPlayerModel &out);

/** Create the GL buffers for every part. */
bool CreatePlayerBuffers(ZPlayerModel &model, const ZShaderProgram &program);

/** Move every animated part's clock on, and rewrite its vertices. */
void AdvancePlayer(ZPlayerModel &model, std::int32_t deltaMs);

/** Put every part's current pose in its buffer. Still parts show frame 0. */
void PosePlayer(ZPlayerModel &model);

/**
 * The box the whole player occupies.
 *
 * Only the parts that hang off nothing count: an attached part sits inside the
 * body anyway, and letting a long rifle drive the framing would make the
 * character shrink every time the gun changed.
 */
ZMeshBounds PlayerBounds(const ZPlayerModel &model);

/**
 * Draw every part against one base matrix.
 *
 * @param base Row-major, and it must already carry the scale: the vertices go
 *        in raw.
 */
void DrawPlayer(ZPlayerModel &model, const ZShaderProgram &program,
                const float *base);
/** Resolve the mesh used by the actual torso draw and pose passes. */
ZPlayerPart *FindPlayerTorsoPart(ZPlayerModel &model);

/** CBrother::DrawUI + CMeshCamera::OrientForUI, in full-menu pixel coordinates.
 * The region height scales the active torso's raw Z extent; weapons do not
 * change framing. Returns false when the active torso cannot be resolved. */
bool BuildPlayerUIMatrix(const ZPlayerModel &model, float centerX, float top, float height,
    float facingRadians, float screenWidth, float screenHeight, float *out);

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
void SelectPlayerMoveSlot(ZPlayerModel &model, std::size_t slot, bool report);

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
float PlayerModelWorldScale(const ZPlayerModel &model, float gameScale,
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
