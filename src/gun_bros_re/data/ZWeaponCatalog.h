/** @file ZWeaponCatalog.h
 * @brief Shared weapon catalogue; categories never select a holding pose.
 */
#ifndef GUN_BROS_RE_ZWEAPONCATALOG_H
#define GUN_BROS_RE_ZWEAPONCATALOG_H

#include "gun_bros_re/data/ZPackTables.h"
#include "gun_bros_re/gameplay/CGun.h"
#include "engine/platform/ZWindow.h"
#include <string>
#include <vector>

constexpr int kWeaponCategoryCount = 7;
const char *WeaponCategoryName(int category);

struct ZWeaponEntry {
    std::uint32_t packHash = 0;
    std::uint32_t ordinal = 0;
    CGun::Template data;
    std::string name;
    std::string owner;
    int category = -1;
    bool visualOnly = false;
    bool hasStoreEntry = false; // Missing metadata does not prove an unused asset.
};

/** Reads each GUN once, then joins STORE_ITEM references for display names. */
bool LoadWeaponCatalog(CResTOCManager &toc, ZPackTables &tables,
                       std::vector<ZWeaponEntry> &weapons);
std::string WeaponSelectionLabel(const std::vector<ZWeaponEntry> &weapons,
                                 std::size_t current);

#endif
