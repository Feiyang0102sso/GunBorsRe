/** @file WeaponCatalog.h
 * @brief Shared weapon catalogue; categories never select a holding pose.
 */
#ifndef GUN_BROS_RE_WEAPONCATALOG_H
#define GUN_BROS_RE_WEAPONCATALOG_H

#include "runtime/PackTables.h"
#include "gun_bros/CGun.h"
#include "engine/platform/CWindow.h"
#include <string>
#include <vector>

constexpr int kWeaponCategoryCount = 7;
const char *WeaponCategoryName(int category);

struct WeaponEntry {
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
bool LoadWeaponCatalog(CResTOCManager &toc, PackTables &tables,
                       std::vector<WeaponEntry> &weapons);
/** Number keys select categories, N/M wrap within the selected category. */
std::size_t SelectWeaponKey(const std::vector<WeaponEntry> &weapons,
                            std::size_t current, KeyCode key);
std::string WeaponSelectionLabel(const std::vector<WeaponEntry> &weapons,
                                 std::size_t current);

#endif
