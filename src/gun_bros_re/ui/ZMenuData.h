/** @file ZMenuData.h
 * @brief Original CMenuDataProvider static labels, icons, movies and actions.
 */
#ifndef GUN_BROS_RE_ZMENUDATA_H
#define GUN_BROS_RE_ZMENUDATA_H
#include <cstring>
struct ZMenuDataEntry {
    const char *table;
    unsigned index;
    const char *strings[4];
    unsigned sprites[4];
    const char *movies[2];
    unsigned action;
    unsigned parameter;
};
inline constexpr unsigned kNavigationBranches[] = {
#include "gun_bros_re/ui/CMenuSystemNavigationData.inc"
};
inline const ZMenuDataEntry *FindMenuData(const char *table, unsigned index) {
    static const ZMenuDataEntry entries[] = {
#include "gun_bros_re/ui/ZMenuData.inc"
    };
    for (const ZMenuDataEntry &entry : entries) {
        if (entry.index == index && std::strcmp(entry.table, table) == 0) { return &entry; }
    }
    return nullptr;
}
#endif
