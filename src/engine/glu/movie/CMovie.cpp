/** @file CMovie.cpp
 * @brief Parse all eight movie object types present in the iOS implementation.
 */
#include "engine/glu/movie/CMovie.h"
#include <cstdio>

bool CMovie::GetChapterRange(unsigned index, unsigned &start, unsigned &end) const {
    return chapter.GetRange(duration, index, start, end);
}

bool CMovie::Init(CArrayInputStream &stream) {
    objects.clear();
    chapter.starts.clear();
    width = stream.ReadUInt16();
    height = stream.ReadUInt16();
    duration = stream.ReadUInt32();
    const unsigned objectCount = stream.ReadUInt16();
    if (width == 0 || height == 0 || width > 4096 || height > 4096 || objectCount > 255) { return false; }
    for (unsigned index = 0; index < objectCount; ++index) {
        CMovieObject object;
        if (!object.Init(stream)) {
            std::printf("[movie] invalid object index=%u type=%u\n", index, object.type);
            return false;
        }
        if (object.type == 5) {
            chapter.starts.push_back(0);
            for (const auto &frame : object.frames) { chapter.starts.push_back(frame.time); }
        }
        objects.push_back(std::move(object));
    }
    return !stream.Overran() && stream.Available() == 0;
}
