#include "engine/core/Paths.h"
/** @file MovieRenderer.cpp
 * @brief Original 3x3 anchors, 16.16 transforms, layered sprites and tiled panels.
 */
#define NOMINMAX
#include "engine/glu/movie/MovieRenderer.h"
#include "engine/core/CMatrix4d.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
float AnchorX(unsigned anchor, float width) {
    if (anchor > 8) { return 0; }
    return (anchor % 3) * width * 0.5f;
}
float AnchorY(unsigned anchor, float height) {
    if (anchor > 8) { return 0; }
    return (anchor / 3) * height * 0.5f;
}
}

bool MovieRenderer::Region(unsigned ordinal, unsigned index, unsigned time, MovieRegion &region) {
    // CMovie::GetUserRegion is a metrics query; invisible touch boxes still exist.
    for (const MovieRegion &candidate : Regions(ordinal, time, 512, 384, true)) {
        if (candidate.index != index) { continue; }
        region = candidate;
        return true;
    }
    return false;
}

std::vector<MovieRegion> MovieRenderer::Regions(unsigned ordinal, unsigned time, float x, float y, bool includeInvisible) {
    std::vector<MovieRegion> regions;
    CMovie *movie = GetMovie(ordinal);
    if (movie == nullptr) { return regions; }
    unsigned regionIndex = 0;
    for (unsigned index = 0; index < movie->objects.size(); ++index) {
        const MovieObject &object = movie->objects[index];
        if (object.type != 6) { continue; }
        const MovieKeyFrame frame = AtTime(object, time);
        const Metrics metrics = GetMetrics(*movie, index, time, 1024, 768);
        if (frame.visible || includeInvisible) { regions.push_back({regionIndex, frame.region, x + metrics.x, y + metrics.y, metrics.width, metrics.height, frame.alpha}); }
        ++regionIndex;
    }
    return regions;
}