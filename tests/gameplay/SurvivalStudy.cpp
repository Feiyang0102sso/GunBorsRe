#include "gameplay/SurvivalCheckScenario.h"
#include "gameplay/SurvivalStudy.h"
#include "gun_bros_re/gameplay/game/CGameSession.h"
int RunFlockPerformanceCheck(const std::string &bigDirectory) {
    CGame::Launch launch;
    launch.bigDirectory = bigDirectory;
    launch.packShortName = "pack7";
    launch.mapIndex = 6;
    launch.startWave = 49;
    SurvivalDevelopment development;
    DevelopmentBinding binding(launch, development);
    development.performanceStudy = true;
    development.performanceSpawnStudy = true;
    development.performanceFlockStudy = true;
    return CGame::Run(launch);
}
int RunFlockCheck(const std::string &bigDirectory) {
    CGame::Launch launch;
    launch.bigDirectory = bigDirectory;
    launch.packShortName = "pack7";
    launch.mapIndex = 6;
    SurvivalDevelopment development;
    DevelopmentBinding binding(launch, development);
    development.flockCheck = true;
    return CGame::Run(launch);
}
int RunSpawnPerformanceCheck(const std::string &bigDirectory, bool realtime, bool uncachedPaths) {
    CGame::Launch launch;
    launch.bigDirectory = bigDirectory;
    launch.packShortName = "pack2";
    launch.mapIndex = 7;
    launch.startWave = 49; // The public wave number is one-based.
    launch.withBrother = true;
    SurvivalDevelopment development;
    DevelopmentBinding binding(launch, development);
    development.performanceStudy = true;
    development.performanceSpawnStudy = true;
    development.performanceRealtimeStudy = realtime;
    development.performanceUncachedPaths = uncachedPaths;
    return CGame::Run(launch);
}
int RunSurvivalStudy(const std::string &bigDirectory, const std::string &packShortName,
    unsigned mapIndex, unsigned weaponIndex, int armorIndex, const std::string &screenshotPath,
    unsigned advanceMs, bool firePreview, bool showCollisions, bool check, unsigned checkWaves, unsigned startWave,
    CGameFlow *gameContext, bool withBrother, bool powerupStudy,
    const Mission::Entry *archiveMission, bool performanceStudy, ZWindow *sharedWindow, bool feedbackStudy,
    bool bossStudy, bool deathStudy, ZGameObserver *frameDriver) {

    CGame::Launch launch{bigDirectory, packShortName, mapIndex, weaponIndex, armorIndex,
        startWave, gameContext, withBrother, archiveMission, sharedWindow};
    SurvivalDevelopment development{screenshotPath, advanceMs, firePreview, showCollisions,
        check, checkWaves, powerupStudy, performanceStudy, feedbackStudy, bossStudy, deathStudy};
    DevelopmentBinding binding(launch, development, frameDriver);
    return CGame::Run(launch);
}
