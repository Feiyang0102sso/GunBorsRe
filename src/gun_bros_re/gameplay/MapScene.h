/** @file MapScene.h
 * @brief Shared Windows map presentation and playable survival entry points.
 * Research viewers call this runtime; gameplay does not call milestone code.
 */
#pragma once
#include <string>
class CWindow;
enum class MapViewMode { Preview, GameView };
struct SurvivalGameContext;
struct MissionEntry;
struct DebugMapSelection;
class LocalBotFriend;
// Development-only check configuration, defined in tests/. Production never
// sets it, so an incomplete type is all this header needs.
struct SurvivalDevelopment;
class ISurvivalScenario;

/** Production session configuration: map, equipment, progress, and an existing window, without validation modes. */
struct SurvivalLaunch {
    std::string bigDirectory;
    std::string packShortName;
    unsigned mapIndex = 0;
    unsigned weaponIndex = 0;
    int armorIndex = -1;
    unsigned startWave = 0;
    SurvivalGameContext *gameContext = nullptr;
    bool withBrother = false;
    const MissionEntry *archiveMission = nullptr;
    CWindow *window = nullptr;
    DebugMapSelection *debugSelection = nullptr;
    const DebugMapSelection *debugMap = nullptr;
    bool localLive = false;
    bool localBot = false; // Selected from the persistent Windows BROS roster.
    LocalBotFriend *botFriend = nullptr;
    bool deathmatch = false;
    unsigned matchIndex = 0;
    unsigned loadout[2]{0, 1};
    // Always null on the production path. Keep this last so the existing
    // aggregate initialisations in tests/ stay valid.
    const SurvivalDevelopment *development = nullptr;
    // Development hook set; production leaves it null and the loop skips every hook.
    ISurvivalScenario *scenario = nullptr;
};
int RunSurvival(const SurvivalLaunch &launch);

