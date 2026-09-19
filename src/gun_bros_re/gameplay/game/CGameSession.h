/** @file CGameSession.h
 * @brief Shared Windows map presentation and playable survival entry points.
 * Research viewers call this runtime; gameplay does not call milestone code.
 */
#pragma once
#include <string>
#include "gun_bros_re/gameplay/game/CGame.h"
class ZWindow;
struct CGameFlow;
struct ZMissionEntry;
struct DebugMapSelection;
class ZLocalBotFriend;
class ZGameObserver;

/** Production session configuration: map, equipment, progress, and an existing window, without validation modes. */
struct CGame::Launch {
    std::string bigDirectory;
    std::string packShortName;
    unsigned mapIndex = 0;
    unsigned weaponIndex = 0;
    int armorIndex = -1;
    unsigned startWave = 0;
    CGameFlow *gameContext = nullptr;
    bool withBrother = false;
    const ZMissionEntry *archiveMission = nullptr;
    ZWindow *window = nullptr;
    DebugMapSelection *debugSelection = nullptr;
    const DebugMapSelection *debugMap = nullptr;
    bool localLive = false;
    bool localBot = false; // Selected from the persistent Windows BROS roster.
    ZLocalBotFriend *botFriend = nullptr;
    bool deathmatch = false;
    unsigned matchIndex = 0;
    unsigned loadout[2]{0, 1};
    // Optional caller adapter. Production uses ordinary platform input.
    ZGameObserver *observer = nullptr;
};
