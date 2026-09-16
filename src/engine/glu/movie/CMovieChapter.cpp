#include "engine/glu/movie/CMovieChapter.h"

bool CMovieChapter::GetRange(unsigned duration, unsigned chapter, unsigned &start, unsigned &end) const {
    if (chapter >= starts.size()) { return false; }
    start = starts[chapter];
    end = duration;
    if (chapter + 1 < starts.size()) { end = starts[chapter + 1]; }
    if (end < start || end > duration) { return false; }
    if (end != start && end != duration) { --end; }
    return true;
}
