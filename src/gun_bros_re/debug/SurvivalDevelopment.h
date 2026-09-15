#pragma once
/** @file SurvivalDevelopment.h
 * @brief Development-run configuration for a survival session.
 *
 * Pure configuration: which experiment to run, where to write evidence, how
 * far to advance. It carries no assertions, so the session loop can read it
 * without reaching into tests/. Production leaves SurvivalLaunch::development
 * null and every field below stays at its default.
 */
#include <string>

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
    bool deathmatchCheck = false;
    bool deathmatchFeedbackCheck = false;
    // Absolute directory for evidence files the session writes itself.
    // Keep this last: tests/ still initialise the leading fields positionally.
    std::string outputDirectory;
};
