/** @file MapScene.h
 * @brief Shared Windows map presentation and playable survival entry points.
 * Research viewers call this runtime; gameplay does not call milestone code.
 */
#ifndef GUN_BROS_RE_MAPSCENE_H
#define GUN_BROS_RE_MAPSCENE_H
#include <cstdint>
#include <string>
enum class MapViewMode { Preview, GameView };
struct SurvivalGameContext;
struct MissionEntry;
int RunMapPreview(const std::string &bigDirectory, const std::string &packShortName,
    std::uint32_t mapIndex, const std::string &screenshotPath, std::uint32_t advanceMs,
    bool showSpawns, bool showCollisions, MapViewMode viewMode, std::uint32_t weaponIndex, bool firePreview);
int RunMapList(const std::string &bigDirectory);
int RunSurvival(const std::string &bigDirectory, const std::string &packShortName,
    unsigned mapIndex, unsigned weaponIndex, int armorIndex, const std::string &screenshotPath,
    unsigned advanceMs, bool firePreview, bool showCollisions, bool check = false, unsigned checkWaves = 2, unsigned startWave = 0,
    SurvivalGameContext *gameContext = nullptr, bool withBrother = false, bool powerupStudy = false,
    const MissionEntry *archiveMission = nullptr, bool performanceStudy = false);
#endif
