/**
 * @file CLayerObject.cpp
 * @brief One map layer's placed game objects: props, enemies, pickups.
 */

#include "gun_bros/CLayerObject.h"

#include <cstdio>

namespace {

/**
 * Read the per-object extra bytes.
 *
 * InitializeObjects allocates an extra buffer whose stride depends on the
 * object type, but only three types actually read anything into it. The other
 * types set hasExtra and write nothing, so their objects are the plain 11
 * bytes -- see the switch at _IDA_OUT/gunbros_3.6.0_IOS.c:126540.
 */
void ReadObjectExtra(CArrayInputStream &stream, PlacedObject &object) {
    const std::uint8_t objectType = object.objectType;
    if (objectType == static_cast<std::uint8_t>(PlacedObjectType::Player)) {
        // :126603. The runtime buffer is 12 bytes; the disk holds this one
        // uint16 only, and it is the spawn angle in degrees.
        object.playerSpawnFacing = stream.ReadUInt16();
        object.hasPlayerSpawnFacing = true;
    } else if (objectType == static_cast<std::uint8_t>(PlacedObjectType::Enemy)) {
        object.pathLayer = stream.ReadUInt8();
        object.facing = stream.ReadInt16();
    } else if (objectType == static_cast<std::uint8_t>(PlacedObjectType::Platform)) {
        object.platformPath = stream.ReadUInt8();
    }
}

}  // namespace

CLayerObject::CLayerObject() : m_declaredCount(0) {}

bool CLayerObject::Init(CArrayInputStream &stream) {
    m_objects.clear();

    m_declaredCount = stream.ReadUInt16();
    const std::uint8_t groupCount = stream.ReadUInt8();

    for (std::uint8_t group = 0; group < groupCount; ++group) {
        const std::uint8_t objectType = stream.ReadUInt8();
        const std::uint16_t count = stream.ReadUInt16();
        stream.ReadUInt16();  // extra-data allocation count

        for (std::uint16_t index = 0; index < count; ++index) {
            PlacedObject object;
            object.objectType = objectType;
            object.packHash = stream.ReadUInt32();
            object.localIndex = stream.ReadUInt8();

            const std::uint8_t hasExtra = stream.ReadUInt8();

            object.x = stream.ReadInt16();
            object.y = stream.ReadInt16();
            object.spawnTag = stream.ReadUInt8();

            if (hasExtra != 0) {
                ReadObjectExtra(stream, object);
            }

            m_objects.push_back(object);
        }
    }

    if (stream.Overran()) {
        std::printf("[layerobject] truncated after %zu objects\n", m_objects.size());
        m_objects.clear();
        return false;
    }

    if (m_objects.size() != m_declaredCount) {
        // Not fatal -- the groups are what the engine spawns from -- but it
        // means one of the two counts is being read wrong, so say so.
        std::printf("[layerobject] header says %u objects, groups hold %zu\n",
                    m_declaredCount, m_objects.size());
    }

    return true;
}
