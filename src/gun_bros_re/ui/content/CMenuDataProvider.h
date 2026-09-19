#pragma once
/** Original CMenuDataProvider static bindings (:148357..154133).
 * Native statics remain verbatim in the retained generated inc files. */
#include <cstring>
class CMenuDataProvider {
public:
struct Entry {
    const char *table;
    unsigned index;
    const char *strings[4];
    unsigned sprites[4];
    const char *movies[2];
    unsigned action;
    unsigned parameter;
};
    static const Entry *Find(const char *table, unsigned index);
};
inline constexpr unsigned kNavigationBranches[] = {
#include "gun_bros_re/ui/CMenuSystemNavigationData.inc"
};
