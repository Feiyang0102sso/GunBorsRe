#include "engine/core/ZPaths.h"
/** @file ZMoviePlayback.cpp
 * @brief Original 3x3 anchors, 16.16 transforms, layered sprites and tiled panels.
 */
#define NOMINMAX
#include "engine/glu/movie/ZMovieRenderer.h"
#include "engine/core/ZMatrix4d.h"
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

bool ZMovieRenderer::Region(unsigned ordinal, unsigned index, unsigned time, ZMovieRegion &region) {
    // CMovie::GetUserRegion is a metrics query; invisible touch boxes still exist.
    for (const ZMovieRegion &candidate : Regions(ordinal, time, 512, 384, true)) {
        if (candidate.index != index) { continue; }
        region = candidate;
        return true;
    }
    return false;
}

std::vector<ZMovieRegion> ZMovieRenderer::Regions(unsigned ordinal, unsigned time, float x, float y, bool includeInvisible) {
    std::vector<ZMovieRegion> regions;
    CMovie *movie = GetMovie(ordinal);
    if (movie == nullptr) { return regions; }
    unsigned regionIndex = 0;
    for (unsigned index = 0; index < movie->objects.size(); ++index) {
        const CMovieObject &object = movie->objects[index];
        if (object.type != 6) { continue; }
        const ZMovieKeyFrame frame = AtTime(object, time);
        const Metrics metrics = GetMetrics(*movie, index, time, 1024, 768);
        if (frame.visible || includeInvisible) { regions.push_back({regionIndex, frame.region, x + metrics.x, y + metrics.y, metrics.width, metrics.height, frame.alpha}); }
        ++regionIndex;
    }
    return regions;
}
