/** @file COffscreenSpawnLocationFilter.h
 * @brief Original enemySpawner.cpp AcceptSpawnLocation :147156.
 * Stores the same camera rectangle as values rather than original ARM pointers.
 */
#pragma once
/** Desktop value form of the original offscreen spawn filter. */
struct COffscreenSpawnLocationFilter {
    float left, top, right, bottom;
    bool enabled;
    bool AcceptSpawnLocation(float x, float y) const {
        if (!enabled) { return true; }
        return x < left || x > right || y < top || y > bottom;
    }
};
