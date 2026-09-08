/** @file CStoreItem.h
 * @brief Complete store-item wire format, named after the original class.
 */
#ifndef GUN_BROS_RE_CSTOREITEM_H
#define GUN_BROS_RE_CSTOREITEM_H
#include "gun_bros/CGameAssetRef.h"
#include <array>
#include <vector>

struct GameObjectTypeRef {
    std::uint8_t type = 0;
    GameObjectRef object;
};

class CStoreItem {
public:
    bool Init(CArrayInputStream &stream);
    std::uint8_t type = 0;
    std::uint8_t flags = 0;
    std::uint32_t value8 = 0;
    std::vector<GameObjectTypeRef> objects;
    // CStoreAggregator::AcquireItem (:158076) checks these exact fields.
    std::uint16_t requiredLevel = 0;
    std::uint32_t commonPrice = 0;
    std::uint32_t rarePrice = 0;
    std::uint8_t value32 = 0;
    CGameAssetRef assets[6]; // [1] icon, [2] name, [5] loadout description.
    std::array<std::vector<std::int32_t>, 8> statGroups;
    std::int16_t value240 = 0;
    std::uint8_t value242 = 0;
    std::uint8_t value243 = 0;
    std::uint8_t value244 = 0;
};
#endif
