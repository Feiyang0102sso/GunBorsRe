#pragma once
/** @file SurvivalDevelopment.h
 * @brief Development-run configuration for a survival session.
 *
 * Pure configuration: which experiment to run, where to write evidence, how
 * far to advance. It carries no assertions; the test observer consumes it.
 * Production does not include or receive this record.
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
