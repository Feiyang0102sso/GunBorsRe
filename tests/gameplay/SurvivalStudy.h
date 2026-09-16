#pragma once
#include "gun_bros_re/gameplay/ZMapScene.h"
#include "gun_bros_re/debug/SurvivalDevelopment.h"
#include "gun_bros_re/debug/FlockMetrics.h"
int RunLocalLiveCheck(const std::string &bigDirectory);
int RunSurvivalStudy(const std::string &bigDirectory, const std::string &packShortName,
    unsigned mapIndex, unsigned weaponIndex, int armorIndex, const std::string &screenshotPath,
    unsigned advanceMs, bool firePreview, bool showCollisions, bool check = false, unsigned checkWaves = 2, unsigned startWave = 0,
    ZSurvivalGameContext *gameContext = nullptr, bool withBrother = false, bool powerupStudy = false,
    const ZMissionEntry *archiveMission = nullptr, bool performanceStudy = false, ZWindow *sharedWindow = nullptr, bool feedbackStudy = false,
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
class ZCombatWorld;
int RunFlockCheck(const std::string &bigDirectory);
int CheckFlockMovement(ZCombatWorld &scene);
int RunFlockPerformanceCheck(const std::string &bigDirectory);
