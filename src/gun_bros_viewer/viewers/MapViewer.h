#pragma once
#include <cstdint>
#include <string>
#include "gun_bros_re/gameplay/MapScene.h"
class CWindow;
struct SurvivalGameContext;
struct MissionEntry;
int RunMapPreview(const std::string &bigDirectory, const std::string &packShortName,
    std::uint32_t mapIndex, const std::string &screenshotPath, std::uint32_t advanceMs,
    bool showSpawns, bool showCollisions, MapViewMode viewMode, std::uint32_t weaponIndex, bool firePreview);
int RunMapList(const std::string &bigDirectory);
