#include "tests/research/SurvivalStudyHost.h"
#if GB_ENABLE_TESTS
#include "gameplay/SurvivalStudy.h"
#endif
int RunViewerSurvival(const std::string &bigDirectory, const std::string &packShortName,
    unsigned mapIndex, unsigned weaponIndex, int armorIndex, const std::string &screenshotPath,
    unsigned advanceMs, bool firePreview, bool showCollisions, bool check, unsigned checkWaves, unsigned startWave,
    SurvivalGameContext *gameContext, bool withBrother, bool powerupStudy,
    const MissionEntry *archiveMission, bool performanceStudy, CWindow *sharedWindow, bool feedbackStudy,
    bool bossStudy, bool deathStudy) {
#if GB_ENABLE_TESTS
    return RunSurvivalStudy(bigDirectory, packShortName, mapIndex, weaponIndex, armorIndex,
        screenshotPath, advanceMs, firePreview, showCollisions, check, checkWaves, startWave,
        gameContext, withBrother, powerupStudy, archiveMission, performanceStudy, sharedWindow,
        feedbackStudy, bossStudy, deathStudy);
#else
    SurvivalLaunch launch{bigDirectory, packShortName, mapIndex, weaponIndex, armorIndex,
        startWave, gameContext, withBrother, archiveMission, sharedWindow};
    return RunSurvival(launch);
#endif
}
