/**
 * @file CGameAssetRef.cpp
 * @brief The reference types object templates use to point at other things.
 */

#include "gun_bros_re/data/CGameAssetRef.h"

void CGameAssetRef::Init(CArrayInputStream &stream) {
    packHash = stream.ReadUInt32();
    assetId = stream.ReadInt32();
}

void GameObjectRef::Init(CArrayInputStream &stream) {
    packHash = stream.ReadUInt32();

    // The index byte is only written when there is something to point at.
    if (packHash == kNullPackHash) {
        localIndex = kNoLocalIndex;
        return;
    }
    localIndex = stream.ReadUInt8();
}

void RequirementList::Init(CArrayInputStream &stream) {
    groupCount = stream.ReadUInt8();
    entryCount = 0;
    objects.clear();

    for (std::uint32_t group = 0; group < groupCount; ++group) {
        const auto objectType = stream.ReadUInt8();  // object type; nothing preloads yet
        // The old consumer discarded this field; loading now retains it.
        const std::uint8_t entries = stream.ReadUInt8();

        for (std::uint8_t entry = 0; entry < entries; ++entry) {
            Entry requirement;
            requirement.objectType = objectType;
            requirement.object.packHash = stream.ReadUInt32();  // pack hash
            requirement.object.localIndex = stream.ReadUInt8();   // local index, 255 meaning "skip"
            objects.push_back(requirement);
            entryCount++;
        }
    }
}

CGameSpriteGluRef::CGameSpriteGluRef()
    : packHash(kNullPackHash), archetype(255), action(255), animation(255) {}

void CGameSpriteGluRef::Init(CArrayInputStream &stream) {
    // All four fields are always present, unlike GameObjectRef, whose index
    // byte disappears when its hash is zero.
    packHash = stream.ReadUInt32();
    archetype = stream.ReadUInt8();
    action = stream.ReadUInt8();
    animation = stream.ReadUInt8();
}
