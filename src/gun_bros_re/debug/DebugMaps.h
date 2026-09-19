#pragma once
/** @file DebugMaps.h
 * @brief Desktop-only BIG map selection; no retail progress is written by previews.
 */
#include "gun_bros_re/data/ZMissionCatalog.h"
class ZWindow;
class CProfileManager;
#include "gun_bros_re/gameplay/game/CGameSession.h"
struct CGameFlow;
struct DebugMapSelection {
    std::string pack;
    unsigned map = 0;
    GameObjectRef level;
    ZMissionEntry mission;
    bool hasMission = false;
    bool ready = false;
};
inline constexpr int kDebugMapMenuChoice = -4;
inline constexpr int kDebugMapSessionChoice = 2;
inline constexpr int kDebugMapSessionComplete = 3;
bool ShowDebugMapPicker(CResTOCManager &toc, ZPackTables &tables, ZWindow &window, DebugMapSelection &selection,
    const std::string &message = "");
CGame::Launch MakeDebugMapLaunch(const std::string &bigDirectory, const DebugMapSelection &selection,
    CGameFlow &context);
void RunDebugMaps(const std::string &bigDirectory, ZWindow &window, DebugMapSelection &selection,
    const CProfileManager &profile);
