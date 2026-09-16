/** @file ZMapScene.h
 * @brief Shared Windows map presentation and playable survival entry points.
 * Research viewers call this runtime; gameplay does not call milestone code.
 */
#pragma once
#include <string>
class ZWindow;
enum class ZMapViewMode { Preview, GameView };
struct ZSurvivalGameContext;
struct ZMissionEntry;
struct DebugMapSelection;
class ZLocalBotFriend;
// Development-only check configuration, defined in tests/. Production never
// sets it, so an incomplete type is all this header needs.
struct SurvivalDevelopment;
class ZSurvivalScenario;

/** Production session configuration: map, equipment, progress, and an existing window, without validation modes. */
struct ZSurvivalLaunch {
    std::string bigDirectory;
    std::string packShortName;
    unsigned mapIndex = 0;
    unsigned weaponIndex = 0;
    int armorIndex = -1;
    unsigned startWave = 0;
    ZSurvivalGameContext *gameContext = nullptr;
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
    // Always null on the production path. Keep this last so the existing
    // aggregate initialisations in tests/ stay valid.
    const SurvivalDevelopment *development = nullptr;
    // Development hook set; production leaves it null and the loop skips every hook.
    ZSurvivalScenario *scenario = nullptr;
};
int RunSurvival(const ZSurvivalLaunch &launch);
