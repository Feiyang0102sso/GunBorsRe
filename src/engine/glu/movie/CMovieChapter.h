#pragma once
#include <cstdint>
#include <vector>

/** Native chapter track (CMovieChapter::Init :109532, GetChapterLengthMS :109576). */
class CMovieChapter {
public:
    bool GetRange(unsigned duration, unsigned chapter, unsigned &start, unsigned &end) const;
    std::vector<std::uint32_t> starts;
};
