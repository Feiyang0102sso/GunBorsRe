#include "tests/research/SurvivalStudyHost.h"
#include "gameplay/SurvivalStudy.h"
int RunViewerSurvival(const std::string &bigDirectory, const std::string &packShortName,
    unsigned mapIndex, unsigned weaponIndex, int armorIndex, const std::string &screenshotPath,
    unsigned advanceMs, bool firePreview, bool showCollisions, bool check, unsigned checkWaves, unsigned startWave,
    CGameFlow *gameContext, bool withBrother, bool powerupStudy,
    const ZMissionEntry *archiveMission, bool performanceStudy, ZWindow *sharedWindow, bool feedbackStudy,
    bool bossStudy, bool deathStudy) {
    return RunSurvivalStudy(bigDirectory, packShortName, mapIndex, weaponIndex, armorIndex,
        screenshotPath, advanceMs, firePreview, showCollisions, check, checkWaves, startWave,
        gameContext, withBrother, powerupStudy, archiveMission, performanceStudy, sharedWindow,
        feedbackStudy, bossStudy, deathStudy);
}
