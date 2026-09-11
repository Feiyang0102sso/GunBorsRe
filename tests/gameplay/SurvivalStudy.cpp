#include "gameplay/SurvivalStudy.h"
#include "gun_bros_re/gameplay/SurvivalRuntime.h"
int RunSurvivalStudy(const std::string &bigDirectory, const std::string &packShortName,
    unsigned mapIndex, unsigned weaponIndex, int armorIndex, const std::string &screenshotPath,
    unsigned advanceMs, bool firePreview, bool showCollisions, bool check, unsigned checkWaves, unsigned startWave,
    SurvivalGameContext *gameContext, bool withBrother, bool powerupStudy,
    const MissionEntry *archiveMission, bool performanceStudy, CWindow *sharedWindow, bool feedbackStudy,
    bool bossStudy, bool deathStudy) {

    SurvivalLaunch launch{bigDirectory, packShortName, mapIndex, weaponIndex, armorIndex,
        startWave, gameContext, withBrother, archiveMission, sharedWindow};
    SurvivalDevelopment development{screenshotPath, advanceMs, firePreview, showCollisions,
        check, checkWaves, powerupStudy, performanceStudy, feedbackStudy, bossStudy, deathStudy};
    return RunSurvivalSession(launch, &development);
}
