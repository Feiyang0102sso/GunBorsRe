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
};
int RunSurvival(const SurvivalLaunch &launch);

