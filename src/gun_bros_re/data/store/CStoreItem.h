/** @file CStoreItem.h
 * @brief Complete store-item wire format, named after the original class.
 */
#ifndef GUN_BROS_RE_CSTOREITEM_H
#define GUN_BROS_RE_CSTOREITEM_H
#include "gun_bros_re/application/CGunBros.h"
#include <string>
#include "gun_bros_re/data/objects/CGameAssetRef.h"
#include <array>
#include <vector>

struct GameObjectTypeRef {
    std::uint8_t type = 0;
    GameObjectRef object;
};

class CStoreItem {
public:
    /** Host listing snapshot; parsed object ownership remains in the pack. */
    struct Entry;
    static bool LoadEntries(CResTOCManager &toc, CGunBros &tables, std::vector<Entry> &entries);
    static const CStoreItem *Load(CResTOCManager &toc, CGunBros &tables, const GameObjectRef &ref);
    bool Init(CArrayInputStream &stream);
    /** STORE mode indices: single-player 0, multiplayer 1, deathmatch 2.
     * CStoreAggregator::IsItemExcludedFromGameType :156283; store_entry.bt.
     */
    bool IsExcludedFromGameType(unsigned gameType) const {
        return (excludedGameModes & (1u << gameType)) != 0;
    }
    // Loader identity, corresponding to original cached pack/index at +356/+358.
    // This is not an extra field in the serialized STORE payload.
    GameObjectRef resource;
    std::uint8_t type = 0;
    std::uint8_t flags = 0;
    // Original member +8; serialized uint32 at file offset 2.
    std::uint32_t excludedGameModes = 0;
    std::vector<GameObjectTypeRef> objects;
    // CStoreAggregator::AcquireItem (:158076) checks these exact fields.
    std::uint16_t requiredLevel = 0;
    std::uint32_t commonPrice = 0;
    std::uint32_t rarePrice = 0;
    std::uint8_t value32 = 0;
    // [1] icon, [2] name, [3] description, [4] the expanded card's stat
    // template and [5] the folded card's power template. The templates use
    // `^fN` font switches and `#KEY` value slots.
    CGameAssetRef assets[6];
    std::array<std::vector<std::int32_t>, 8> statGroups;
    // Store display order; CStoreAggregator sorts on it and -1 hides the row.
    std::int16_t displayOrder = 0;
    std::uint8_t value242 = 0;
    // Set on the starter bundle alone: the record may only be bought once.
    std::uint8_t singlePurchase = 0;
    std::uint8_t value244 = 0;
};
struct CStoreItem::Entry {
    GameObjectRef ref;
    std::string owner;
    std::string name;
    // CStoreItem::GetIapName: resolved from BIG asset[0], not a host SKU table.
    std::string productId;
    CStoreItem data;
};
#endif
