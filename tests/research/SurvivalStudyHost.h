#pragma once
#include "gun_bros_re/gameplay/CGameSession.h"
int RunViewerSurvival(const std::string &bigDirectory, const std::string &packShortName,
    unsigned mapIndex, unsigned weaponIndex, int armorIndex, const std::string &screenshotPath,
    unsigned advanceMs, bool firePreview, bool showCollisions, bool check = false, unsigned checkWaves = 2, unsigned startWave = 0,
    ZSurvivalGameContext *gameContext = nullptr, bool withBrother = false, bool powerupStudy = false,
    const ZMissionEntry *archiveMission = nullptr, bool performanceStudy = false, ZWindow *sharedWindow = nullptr, bool feedbackStudy = false,
    bool bossStudy = false, bool deathStudy = false);
