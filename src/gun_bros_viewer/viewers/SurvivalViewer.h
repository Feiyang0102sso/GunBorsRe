#pragma once
#include "gun_bros_re/gameplay/MapScene.h"
int RunViewerSurvival(const std::string &bigDirectory, const std::string &packShortName,
    unsigned mapIndex, unsigned weaponIndex, int armorIndex, const std::string &screenshotPath,
    unsigned advanceMs, bool firePreview, bool showCollisions, bool check = false, unsigned checkWaves = 2, unsigned startWave = 0,
    SurvivalGameContext *gameContext = nullptr, bool withBrother = false, bool powerupStudy = false,
    const MissionEntry *archiveMission = nullptr, bool performanceStudy = false, CWindow *sharedWindow = nullptr, bool feedbackStudy = false,
    bool bossStudy = false, bool deathStudy = false);
