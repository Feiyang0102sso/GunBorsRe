/**
 * @file CLayerObject.h
 * @brief One map layer's placed game objects: props, enemies, pickups.
 *
 * Port of CLayerObject (src/gunbros/layerObject.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:126460 (InitializeObjects),
 *            :126287 (SpawnInstance)
 * Cross-checked against _Big_tool/binary template/big_assets/maps/map.bt.
 *
 * Wire format:
 *   uint16 totalObjects        -- only sizes an array; the groups are the truth
 *   uint8  groupCount
 *   groups: uint8 objectType, uint16 count, uint16 extraAllocCount
 *           objects: uint32 packHash, uint8 localIndex, uint8 hasExtra,
 *                    int16 x, int16 y, uint8 spawnTag,
 *                    plus extra bytes decided by objectType when hasExtra
 *
 * The extra bytes are the trap here: only three object types write any, and
 * getting that wrong desynchronises the rest of the map.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CLAYEROBJECT_H
#define GUN_BROS_RE_GUN_BROS_CLAYEROBJECT_H

#include "engine/resources/CArrayInputStream.h"

#include <cstdint>
#include <vector>

/**
 * Object types, as switched on in CLayerObject::SpawnInstance.
 *
 * A type id is the section number minus one, so type 19 addresses section 20
 * (PROP). Only the ones this port names are listed; map.bt has the full 28.
 */
enum class PlacedObjectType : std::uint8_t {
    Enemy = 5,
    ParticleEffect = 11,
    Pickup = 12,
    Platform = 14,
    Player = 15,
    Prop = 19,
};

// A spawn tag of 255 means the object is not part of a script-driven group.
constexpr std::uint8_t kUntaggedSpawn = 255;

/** One object placed on the map. */
struct PlacedObject {
    std::uint8_t objectType;
    std::uint32_t packHash;   // which pack holds the template
    std::uint8_t localIndex;  // ordinal within that pack's section
    std::int16_t x;
    std::int16_t y;
    std::uint8_t spawnTag;
    // CEnemy::Spawn :73292 consumes the authored path and initial facing.
    std::uint8_t pathLayer = 255;
    std::int16_t facing = 0;
    // The PLAYER extra uint16. InitializeObjects :126603 stores it in the
    // object layer's member +76; at level start :120846/:121005 hand
    // {+76,+80,+84} to CBrother::Spawn :135887, which keeps it in the same
    // member +1984 that SetAngle writes -- the spawn facing, in degrees.
    // Checked against 20 original maps: mostly 180, one 0, the rest omit the
    // whole extra field (see below).
    std::uint16_t playerSpawnFacing = 0;
    // With hasExtra 0 the original never writes member +76, so its spawn angle
    // is uninitialised memory. This port keeps 0 instead.
    bool hasPlayerSpawnFacing = false;
    std::uint8_t platformPath = 255;
};

/**
 * An object layer.
 *
 * The original keeps the objects grouped by type because spawning walks them
 * that way; nothing here needs that, so the groups are flattened and the type
 * kept on each object.
 */
class CLayerObject {
public:
    CLayerObject();

    bool Init(CArrayInputStream &stream);

    const std::vector<PlacedObject> &GetObjects() const { return m_objects; }
    void SetLayerIndex(unsigned index) { m_layerIndex = index; }
    unsigned GetLayerIndex() const { return m_layerIndex; }

    /** What the header claimed, for comparison against the groups. */
    std::uint16_t GetDeclaredCount() const { return m_declaredCount; }

private:
    std::uint16_t m_declaredCount;
    unsigned m_layerIndex = 0;
    std::vector<PlacedObject> m_objects;
};

#endif  // GUN_BROS_RE_GUN_BROS_CLAYEROBJECT_H
