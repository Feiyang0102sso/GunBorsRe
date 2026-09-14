#pragma once
#include "gun_bros_re/gameplay/MapScene.h"
/** Development driver input and experiment selection, compiled only in Debug. */
struct SurvivalDevelopment {
    std::string screenshotPath;
    unsigned advanceMs = 0;
    bool firePreview = false;
    bool showCollisions = false;
    bool check = false;
    unsigned checkWaves = 2;
    bool powerupStudy = false;
    bool performanceStudy = false;
    bool feedbackStudy = false;
    bool bossStudy = false;
    bool deathStudy = false;
    bool campaignDoorCheck = false;
    bool debugMapProfileCheck = false;
    bool campaignTargetCheck = false;
    bool campaignProgressionCheck = false;
    bool campaignRescueCheck = false;
    bool campaignPortalCheck = false;
    bool campaignCacheCheck = false;
    bool performanceSpawnStudy = false;
    bool performanceRealtimeStudy = false;
    bool performanceUncachedPaths = false;
    bool flockCheck = false;
    bool performanceFlockStudy = false;
    bool localLiveCheck = false;
};
int RunLocalLiveCheck(const std::string &bigDirectory);
int RunSurvivalStudy(const std::string &bigDirectory, const std::string &packShortName,
    unsigned mapIndex, unsigned weaponIndex, int armorIndex, const std::string &screenshotPath,
    unsigned advanceMs, bool firePreview, bool showCollisions, bool check = false, unsigned checkWaves = 2, unsigned startWave = 0,
    SurvivalGameContext *gameContext = nullptr, bool withBrother = false, bool powerupStudy = false,
    const MissionEntry *archiveMission = nullptr, bool performanceStudy = false, CWindow *sharedWindow = nullptr, bool feedbackStudy = false,
    bool bossStudy = false, bool deathStudy = false);
/** Fatal-hit and SDL suicide regression on all four retail survival maps. */
int RunPlayerDeathCheck(const std::string &bigDirectory);
/** Four retail LEVEL scripts, their Boss camera and real grenade collisions. */
int RunBossCheck(const std::string &bigDirectory);
/** Real BIG scenery/player pixel checks above and below an obstacle. */
int RunMapOcclusionCheck(const std::string &bigDirectory);
/** Wave 50 at the reported slow position, with both brothers alive. */
int RunSpawnPerformanceCheck(const std::string &bigDirectory, bool realtime = false, bool uncachedPaths = false);
int RunPathCacheCheck();
class CombatScene;
int RunFlockCheck(const std::string &bigDirectory);
int CheckFlockMovement(CombatScene &scene);
int RunFlockPerformanceCheck(const std::string &bigDirectory);
struct FlockMetrics {
    float nearestMean = 0;
    float minimum = 0;
    unsigned closePairs = 0;
};
FlockMetrics MeasureFlock(const CombatScene &scene);

