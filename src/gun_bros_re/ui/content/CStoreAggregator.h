#pragma once
#include "gun_bros_re/data/ZStoreCatalog.h"
#include "gun_bros_re/data/ZWeaponCatalog.h"
#include "gun_bros_re/data/ZArmorCatalog.h"
#include "gun_bros_re/data/CProfileManager.h"

namespace MenuDetail {
constexpr unsigned kOwnedFilterBit = 1u << 18; // CStoreAggregator native filter criterion.

/** CStoreAggregator filters STORE entries and applies equipment policy.
 * Native InitFilteredList :158999 and EquipItem :156082. */
class CStoreAggregator {
public:
    /** CStoreAggregator::EquipItem :156082 and SetGun/SetArmor :171658.
     * Granting inventory and equipping it are separate original menu actions.
     */
    static bool EquipStoreItem(CProfileManager &profile, const CStoreItem &item, const std::vector<ZArmorEntry> &armors);
    static const ZWeaponEntry *FindWeaponEntry(const std::vector<ZWeaponEntry> &weapons, const GameObjectRef &ref);
    /** A bundle is owned once every object it hands over is. Only the records with
     * the single-purchase flag are treated this way. */
    // Correction: repeatable multi-object bundles also use inventory ownership.
    static bool OwnsBundle(const CProfileManager &profile, const CStoreItem &item);
    static bool IsStoreObjectEquipped(CProfileManager &profile, unsigned slot, const GameObjectRef &object);
    static GameObjectRef &Equipped(CProfileManager &profile, unsigned slot);
    static bool MatchesEquipmentSlot(const ZStoreEntry &entry, unsigned slot,
        const std::vector<ZWeaponEntry> &weapons, const std::vector<ZArmorEntry> &armors);
    static void InitFilteredList(const std::vector<ZStoreEntry> &store,
        const CProfileManager &profile, const std::vector<ZWeaponEntry> &weapons,
        const std::vector<ZArmorEntry> &armors, unsigned shopCategory, unsigned shopGunSlot,
        unsigned shopFilter, bool filterAll, unsigned shopExclusionFilter, std::vector<unsigned> &items,
        std::vector<unsigned> &itemSlots);
};
}
