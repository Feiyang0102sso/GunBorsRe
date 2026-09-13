#pragma once
/** @file DebugMaps.h
 * @brief Desktop-only BIG map selection; no retail progress is written by previews.
 */
#include "gun_bros_re/data/MissionCatalog.h"
class CWindow;
class CProfileManager;
struct SurvivalLaunch;
struct SurvivalGameContext;
struct DebugMapSelection {
    std::string pack;
    unsigned map = 0;
    GameObjectRef level;
    MissionEntry mission;
    bool hasMission = false;
    bool ready = false;
};
inline constexpr int kDebugMapMenuChoice = -4;
inline constexpr int kDebugMapSessionChoice = 2;
inline constexpr int kDebugMapSessionComplete = 3;
bool ShowDebugMapPicker(CResTOCManager &toc, PackTables &tables, CWindow &window, DebugMapSelection &selection,
    const std::string &message = "");
SurvivalLaunch MakeDebugMapLaunch(const std::string &bigDirectory, const DebugMapSelection &selection,
    SurvivalGameContext &context);
void RunDebugMaps(const std::string &bigDirectory, CWindow &window, DebugMapSelection &selection,
    const CProfileManager &profile);
