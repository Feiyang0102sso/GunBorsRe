#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/ui/content/CStoreAggregator.h"
namespace MenuDetail {

bool CStoreAggregator::MatchesEquipmentSlot(const ZStoreEntry &entry, unsigned slot,
    const std::vector<ZWeaponEntry> &weapons, const std::vector<ZArmorEntry> &armors) {
    if (slot == 5) {
        if (entry.data.objects.empty() || entry.data.type >= 14) { return false; }
        // Zero-price consumable records are reward payloads (e.g. Bro-op
        // grenade prize), not a repeatable store purchase.
        // Correction: price is not an availability flag. InitFilteredList
        // :159210 applies the authored hidden flag and display order instead.
        for (const GameObjectTypeRef &object : entry.data.objects) {
            if (object.type != 17 || object.object.IsNull()) { return false; }
        }
        return true;
    }
    if (entry.data.objects.size() != 1) { return false; }
    const GameObjectTypeRef &ref = entry.data.objects[0];
    if (slot < 2) {
        if (ref.type != 6) { return false; }
        for (const ZWeaponEntry &weapon : weapons) {
            if (weapon.packHash == ref.object.packHash && weapon.ordinal == ref.object.localIndex) {
                return !weapon.visualOnly && weapon.hasStoreEntry;
            }
        }
    } else {
        if (ref.type != 2) { return false; }
        for (const ZArmorEntry &armor : armors) {
            if (armor.packHash == ref.object.packHash && armor.ordinal == ref.object.localIndex) {
                return armor.data.GetSlot() == kArmorSlots[slot];
            }
        }
    }
    return false;
}

GameObjectRef &CStoreAggregator::Equipped(CProfileManager &profile, unsigned slot) {
    if (slot < 2) { return profile.configuration.guns[slot]; }
    return profile.configuration.armor[kArmorSlots[slot]];
}

/** Shared compact/expanded store status, distinct from the preview slot. */
bool CStoreAggregator::IsStoreObjectEquipped(CProfileManager &profile, unsigned slot, const GameObjectRef &object) {
    // CStoreAggregator::GetItemStatus :155316 requests IsGunEquipped(..., -1).
    if (slot < 2) { return profile.configuration.IsGunEquipped(object) >= 0; }
    return slot < 5 && SameObject(CStoreAggregator::Equipped(profile, slot), object);
}

/** A bundle is owned once every object it hands over is. Only the records with
 * the single-purchase flag are treated this way. */
bool CStoreAggregator::OwnsBundle(const CProfileManager &profile, const CStoreItem &item) {
    // The historical inventory approximation below does not apply to a
    // single-purchase package: CPackageOfferMgr keeps an independent key.
    if (item.singlePurchase != 0) { return profile.IsPackagePurchased(item.resource); }
    for (const GameObjectTypeRef &object : item.objects) {
        if (!profile.Owns(object.type, object.object)) { return false; }
    }
    return !item.objects.empty();
}

const ZWeaponEntry *CStoreAggregator::FindWeaponEntry(const std::vector<ZWeaponEntry> &weapons, const GameObjectRef &ref) {
    for (const ZWeaponEntry &weapon : weapons) {
        if (weapon.packHash == ref.packHash && weapon.ordinal == ref.localIndex) { return &weapon; }
    }
    return nullptr;
}

/** CStoreAggregator::EquipItem :156082 and SetGun/SetArmor :171658.
 * Granting inventory and equipping it are separate original menu actions.
 */
bool CStoreAggregator::EquipStoreItem(CProfileManager &profile, const CStoreItem &item, const std::vector<ZArmorEntry> &armors) {
    CPlayerConfiguration configuration = profile.configuration;
    unsigned gunCount = 0;
    for (const GameObjectTypeRef &object : item.objects) {
        if (object.type == 6 && gunCount < 2) {
            const unsigned slot = (profile.activeWeaponSlot + gunCount) & 1;
            ++gunCount;
            configuration.SetGun(slot, object.object);
        } else if (object.type == 2) {
            bool alreadyEquipped = false;
            for (const GameObjectRef &part : configuration.armor) {
                if (SameObject(part, object.object)) { alreadyEquipped = true; }
            }
            if (alreadyEquipped) { continue; }
            const ZArmorEntry *part = nullptr;
            for (const ZArmorEntry &entry : armors) {
                if (entry.packHash == object.object.packHash && entry.ordinal == object.object.localIndex) { part = &entry; break; }
            }
            if (part == nullptr || part->data.GetSlot() >= configuration.armor.size()) {
                std::printf("[store] Cannot equip armor pack=%u ordinal=%u\n", object.object.packHash, object.object.localIndex);
                return false;
            }
            configuration.armor[part->data.GetSlot()] = object.object;
        }
    }
    profile.configuration = configuration;
    return true;
}
namespace {
// GetItemStatus :155261..155433. Only ownership states affect filtering;
// level, acquisition and runtime promotion states retain category selection.
int OwnershipStatus(const CStoreItem &item, const CProfileManager &profile) {
    if (item.singlePurchase != 0) {
        if (profile.IsPackagePurchased(item.resource)) { return 3; }
        return -1;
    }
    if (item.objects.size() == 1) {
        const auto &object = item.objects.front();
        if (object.type == 6 && profile.configuration.IsGunEquipped(object.object) >= 0) { return 4; }
        if (object.type == 2) {
            for (const auto &armor : profile.configuration.armor) {
                if (SameObject(armor, object.object)) { return 4; }
            }
        }
        if (object.type != 17 && profile.Owns(object.type, object.object)) { return 3; }
    } else {
        unsigned nonConsumables = 0;
        bool allOwned = true;
        for (const auto &object : item.objects) {
            if (object.type != 6 && object.type != 2) { continue; }
            ++nonConsumables;
            bool owned = profile.Owns(object.type, object.object);
            if (object.type == 6 && profile.configuration.IsGunEquipped(object.object) >= 0) { owned = true; }
            if (object.type == 2) {
                for (const auto &armor : profile.configuration.armor) {
                    if (SameObject(armor, object.object)) { owned = true; }
                }
            }
            if (!owned) { allOwned = false; }
        }
        if (nonConsumables >= 2 && allOwned) { return 3; }
    }
    return -1;
}

// Native ctor :158975 and InitFilteredList :159129..159217.
constexpr unsigned kRootMasks[] = {0x7F, 0x380, 0x3C00, 0x1C000};
constexpr unsigned kEquippedFilterBit = 1u << 19;
bool MatchesFilter(const CStoreItem &item, const CProfileManager &profile,
    unsigned root, unsigned filter) {
    if (item.type >= 32 || (root & (1u << item.type)) == 0) { return false; }
    const bool category = (filter & (1u << item.type)) != 0;
    const unsigned categories = filter & root;
    const bool partial = categories != 0 && categories != root;
    const bool ownedEnabled = (filter & kOwnedFilterBit) != 0;
    const bool equippedEnabled = (filter & kEquippedFilterBit) != 0;
    const int status = OwnershipStatus(item, profile);
    bool include = category;
    if (ownedEnabled) {
        if (category || !partial) {
            bool hasCount = false;
            if (item.type >= 10 && item.type <= 14 && !item.objects.empty()) {
                hasCount = profile.GetPowerupCount(item.objects.front().object) != 0;
            }
            if (status == 3 || status == 4 || hasCount) { include = true; }
        }
    } else if (status == 3) {
        include = false;
    }
    if (status == 4) {
        if (equippedEnabled) {
            if (!partial) { include = true; }
        } else if (!ownedEnabled) {
            include = false;
        }
    }
    return include;
}
}
void CStoreAggregator::InitFilteredList(const std::vector<ZStoreEntry> &store,
    const CProfileManager &profile, const std::vector<ZWeaponEntry> &weapons,
    const std::vector<ZArmorEntry> &armors, unsigned shopCategory, unsigned shopGunSlot,
    unsigned shopFilter, bool filterAll, unsigned shopExclusionFilter, std::vector<unsigned> &items,
    std::vector<unsigned> &itemSlots) {
    items.clear();
    itemSlots.clear();
    // The first column links to the local friend and currency flows, on every
    // category page. The starter bundle follows as the store's own first row.
    if (filterAll && shopFilter == 0) {
        items.push_back(static_cast<unsigned>(store.size())); itemSlots.push_back(6);
        items.push_back(static_cast<unsigned>(store.size() + 1)); itemSlots.push_back(6);
        for (unsigned index = 0; index < store.size(); ++index) {
            if (shopCategory != 0 || store[index].data.singlePurchase == 0 ||
                profile.IsPackageHidden(store[index].ref)) { continue; }
            items.push_back(index);
            itemSlots.push_back(6);
            break;
        }
    }
    // CStoreItem's trailing int16 is the store's own row order; a negative value
    // keeps the record out of the list entirely.
    // Correction: OverrideItem :233074 makes owned negative-order gear visible.
    std::vector<std::pair<int, unsigned>> ordered;
    for (unsigned index = 0; index < store.size(); ++index) {
        const int order = GetStoreDisplayOrder(store[index].data, profile);
        if (order < 0 || store[index].data.value242 == 1 || store[index].data.singlePurchase != 0) { continue; }
        ordered.push_back({order, index});
    }
    std::sort(ordered.begin(), ordered.end());
    for (const std::pair<int, unsigned> &row : ordered) {
        const unsigned index = row.second;
        unsigned slot = shopGunSlot;
        bool matches = false;
        if (shopCategory == 0) {
            matches = CStoreAggregator::MatchesEquipmentSlot(store[index], slot, weapons, armors);
        } else if (shopCategory == 1) {
            for (slot = 2; slot < 5; ++slot) {
                if (CStoreAggregator::MatchesEquipmentSlot(store[index], slot, weapons, armors)) { matches = true; break; }
            }
        } else if (shopCategory == 3) {
            // CStoreAggregator ctor :158975 gives bank mask 0x1C000.
            slot = 7;
            matches = store[index].data.type >= 14 && store[index].data.type <= 16;
        } else {
            slot = 5;
            matches = CStoreAggregator::MatchesEquipmentSlot(store[index], slot, weapons, armors);
        }
        if (!matches) { continue; }
        // InitFilteredList :159135 uses STORE.type, never the model category.
        const unsigned category = store[index].data.type;
        if ((shopExclusionFilter & store[index].data.excludedGameModes) != 0) { continue; }
        unsigned filter = shopFilter;
        if (filterAll && filter == 0) { filter = kRootMasks[shopCategory] | kOwnedFilterBit | kEquippedFilterBit; }
        if (!MatchesFilter(store[index].data, profile, kRootMasks[shopCategory], filter)) { continue; }
        items.push_back(index);
        itemSlots.push_back(slot);
    }
}
}
