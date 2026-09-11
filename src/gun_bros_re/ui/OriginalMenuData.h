/** @file OriginalMenuData.h
 * @brief Original CMenuDataProvider static labels, icons, movies and actions.
 */
#ifndef GUN_BROS_RE_ORIGINALMENUDATA_H
#define GUN_BROS_RE_ORIGINALMENUDATA_H
#include <cstring>
struct OriginalMenuEntry {
    const char *table;
    unsigned index;
    const char *strings[4];
    unsigned sprites[4];
    const char *movies[2];
    unsigned action;
    unsigned parameter;
};
inline constexpr unsigned kOriginalNavigationBranches[] = {
#include "gun_bros_re/ui/OriginalNavigationData.inc"
};
inline const OriginalMenuEntry *OriginalMenuData(const char *table, unsigned index) {
    static const OriginalMenuEntry entries[] = {
#include "gun_bros_re/ui/OriginalMenuData.inc"
    };
    for (const OriginalMenuEntry &entry : entries) {
        if (entry.index == index && std::strcmp(entry.table, table) == 0) { return &entry; }
    }
    return nullptr;
}
#endif
