#include "gun_bros_re/data/store/CStoreItemOverride.h"
#include "gun_bros_re/data/profile/CProfileManager.h"
int CStoreItemOverride::GetDisplayOrder(const CStoreItem &item, const CProfileManager &profile) {
    // CStoreItemOverride::OverrideItem :233074 restores owned hidden entries
    // at order 10000. This is a native algorithm constant, not a resource value.
    if (item.displayOrder >= 0 || item.value242 != 0 || item.singlePurchase != 0) { return item.displayOrder; }
    for (const GameObjectTypeRef &object : item.objects) {
        if (profile.Owns(object.type, object.object)) { return 10000; }
        if (object.type == 17 && profile.GetPowerupCount(object.object) != 0) { return 10000; }
    }
    return item.displayOrder;
}
