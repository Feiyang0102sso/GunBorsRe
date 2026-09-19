#pragma once
/** @file FlockMetrics.h
 * @brief Crowd-spacing measurements for performance reports.
 *
 * Pure measurement with no assertions and no gameplay effect; the thresholds
 * here are diagnostic only. The test-owned performance session writes
 * these numbers into its own performance CSV.
 */
class CLevel;

struct FlockMetrics {
    float nearestMean = 0;
    float minimum = 0;
    unsigned closePairs = 0;
};

/** Mean and minimum nearest-neighbour distance over the live actors. */
FlockMetrics MeasureFlock(const CLevel &scene);
