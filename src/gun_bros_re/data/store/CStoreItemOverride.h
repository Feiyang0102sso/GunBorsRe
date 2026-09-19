#pragma once
#include "gun_bros_re/data/store/CStoreItem.h"
class CProfileManager;
/** Restored local ownership branch; remote overrides remain unimplemented. */
class CStoreItemOverride {
public:
    static int GetDisplayOrder(const CStoreItem &item, const CProfileManager &profile);
};
