/**
 * @file CGameAssetRef.h
 * @brief The reference types object templates use to point at other things.
 *
 * Port of CGameAssetRef and IGameObject::GameObjectRef (src/gunbros/gameAssetRef.cpp).
 * Reference: _IDA_OUT/gunbros_3.6.0_IOS.c:191877 (CGameAssetRef::Init),
 *            :191892 (GameObjectRef::Init), :191802 (RequirementList::Add)
 *
 * Both carry a pack hash plus an index, and in both cases the index is a
 * LOCAL ORDINAL WITHIN A SECTION, not a resource id and not a handle. The
 * section is decided by the call site, never by the data: TileSet::Load treats
 * its refs as images, so they index the PNG section. See CGameObjectPack.
 */

#ifndef GUN_BROS_RE_GUN_BROS_CGAMEASSETREF_H
#define GUN_BROS_RE_GUN_BROS_CGAMEASSETREF_H

#include "engine/CArrayInputStream.h"

#include <cstdint>

// A null reference: no pack, nothing to resolve.
constexpr std::uint32_t kNullPackHash = 0;

// GameObjectRef uses this local index to mean "no object".
constexpr std::uint8_t kNoLocalIndex = 255;

/**
 * Reference to a media asset -- an image, a sound, a mesh.
 * Wire format: uint32 packHash, int32 assetId.
 */
struct CGameAssetRef {
    std::uint32_t packHash;
    std::int32_t assetId;  // ordinal within whichever section the caller means

    CGameAssetRef() : packHash(kNullPackHash), assetId(0) {}

    bool IsNull() const { return packHash == kNullPackHash; }

    void Init(CArrayInputStream &stream);
};

/**
 * Reference to a game object template.
 *
 * Wire format: uint32 packHash, then a uint8 local index -- but the index byte
 * is only present when the hash is non-zero, so a null reference is four bytes
 * and a live one is five. Getting that wrong desynchronises the whole stream.
 */
struct GameObjectRef {
    std::uint32_t packHash;
    std::uint8_t localIndex;

    GameObjectRef() : packHash(kNullPackHash), localIndex(kNoLocalIndex) {}

    bool IsNull() const { return packHash == kNullPackHash; }

    void Init(CArrayInputStream &stream);
};

/**
 * The list of objects a map wants preloaded.
 *
 * Nothing here needs it yet -- the map renderer only has to step over it to
 * reach the layer count -- so this parses the structure and keeps the count.
 *
 * Wire format:
 *   uint8 groupCount
 *   groups: uint8 objectType, uint8 entryCount,
 *           entries: uint32 packHash, uint8 localIndex
 */
struct RequirementList {
    std::uint32_t groupCount;
    std::uint32_t entryCount;

    RequirementList() : groupCount(0), entryCount(0) {}

    void Init(CArrayInputStream &stream);
};

#endif  // GUN_BROS_RE_GUN_BROS_CGAMEASSETREF_H
