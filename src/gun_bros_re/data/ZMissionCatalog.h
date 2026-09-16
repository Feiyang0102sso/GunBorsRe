/** @file ZMissionCatalog.h
 * @brief Permanent archive inspection, separate from retail survival selection.
 */
#ifndef GUN_BROS_RE_ZMISSIONCATALOG_H
#define GUN_BROS_RE_ZMISSIONCATALOG_H
#include "gun_bros_re/data/Mission.h"
#include "gun_bros_re/data/ZPackTables.h"
#include <string>

struct ZMissionEntry {
    GameObjectRef resource;
    Mission data;
    std::string owner;
    std::string title;
};
bool LoadMissionCatalog(CResTOCManager &toc, ZPackTables &tables, std::vector<ZMissionEntry> &catalog);
int RunMissionPlay(const std::string &bigDirectory, const std::string &packName, int missionIndex,
    unsigned weaponIndex, int armorIndex, const std::string &screenshot, unsigned advanceMs, bool fire, bool check = false);
#endif
