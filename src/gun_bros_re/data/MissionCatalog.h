/** @file MissionCatalog.h
 * @brief Permanent archive inspection, separate from retail survival selection.
 */
#ifndef GUN_BROS_RE_MISSIONCATALOG_H
#define GUN_BROS_RE_MISSIONCATALOG_H
#include "gun_bros_re/data/Mission.h"
#include "gun_bros_re/data/PackTables.h"
#include <string>

struct MissionEntry {
    GameObjectRef resource;
    Mission data;
    std::string owner;
    std::string title;
};
bool LoadMissionCatalog(CResTOCManager &toc, PackTables &tables, std::vector<MissionEntry> &catalog);
int RunMissionPlay(const std::string &bigDirectory, const std::string &packName, int missionIndex,
    unsigned weaponIndex, int armorIndex, const std::string &screenshot, unsigned advanceMs, bool fire, bool check = false);
#endif
