/**
 * @file CGameAssetRef.cpp
 * @brief The reference types object templates use to point at other things.
 */

#include "gun_bros/CGameAssetRef.h"

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

    for (std::uint32_t group = 0; group < groupCount; ++group) {
        stream.ReadUInt8();  // object type; nothing preloads yet
        const std::uint8_t entries = stream.ReadUInt8();

        for (std::uint8_t entry = 0; entry < entries; ++entry) {
            stream.ReadUInt32();  // pack hash
            stream.ReadUInt8();   // local index, 255 meaning "skip"
            entryCount++;
        }
    }
}
