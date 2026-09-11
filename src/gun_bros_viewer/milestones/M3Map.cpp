#include "gun_bros_viewer/viewers/MapViewer.h"
/** @file M3Map.cpp
 * @brief Permanent M3 research entry, backed by the shared map renderer.
 * Original implementation and its research comments moved to runtime/MapScene.cpp.
 */
#include "gun_bros_viewer/milestones/M3Map.h"
int RunM3Map(const std::string &bigDirectory, const std::string &packShortName,
    std::uint32_t mapIndex, const std::string &screenshotPath, std::uint32_t advanceMs,
    bool showSpawns, bool showCollisions, MapViewMode viewMode, std::uint32_t weaponIndex, bool firePreview) {
    return RunMapPreview(bigDirectory, packShortName, mapIndex, screenshotPath, advanceMs,
        showSpawns, showCollisions, viewMode, weaponIndex, firePreview);
}
