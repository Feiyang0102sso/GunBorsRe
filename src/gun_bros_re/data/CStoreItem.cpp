/** @file CStoreItem.cpp
 * @brief CStoreItem::Init (:159834), retaining unknown fields without guessing.
 */
#include "gun_bros_re/data/CStoreItem.h"

bool CStoreItem::Init(CArrayInputStream &stream) {
    type = stream.ReadUInt8();
    flags = stream.ReadUInt8();
    value8 = stream.ReadUInt32();
    objects.resize(stream.ReadUInt8());
    for (GameObjectTypeRef &reference : objects) {
        reference.type = stream.ReadUInt8();
        reference.object.Init(stream);
    }
    requiredLevel = stream.ReadUInt16();
    commonPrice = stream.ReadUInt32();
    rarePrice = stream.ReadUInt32();
    value32 = stream.ReadUInt8();
    for (CGameAssetRef &asset : assets) { asset.Init(stream); }
    for (auto &group : statGroups) {
        group.resize(stream.ReadUInt16());
        for (std::int32_t &value : group) { value = stream.ReadInt32(); }
    }
    displayOrder = stream.ReadInt16();
    value242 = stream.ReadUInt8();
    singlePurchase = stream.ReadUInt8();
    value244 = stream.ReadUInt8();
    return !stream.Overran();
}
