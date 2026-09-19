#include "gun_bros_re/ui/content/CMenuDataProvider.h"
const CMenuDataProvider::Entry *CMenuDataProvider::Find(const char *table, unsigned index) {
    static const Entry entries[] = {
#include "gun_bros_re/ui/ZMenuData.inc"
    };
    for (const Entry &entry : entries) {
        if (entry.index == index && std::strcmp(entry.table, table) == 0) { return &entry; }
    }
    return nullptr;
}
