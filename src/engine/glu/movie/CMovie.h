/** @file CMovie.h
 * @brief Original Glu movie timeline and its typed keyframes.
 */
#ifndef GUN_BROS_RE_CMOVIE_H
#define GUN_BROS_RE_CMOVIE_H
#include "engine/resources/CArrayInputStream.h"
#include <array>

#include "engine/glu/movie/CMovieObject.h"
#include "engine/glu/movie/CMovieChapter.h"

class CMovie {
public:
    /** iOS CMovie::InitResource :109263; exact serialized order, no padding. */
    bool Init(CArrayInputStream &stream);
    /** Inclusive playback bounds, from CMovieChapter::GetChapterLengthMS :109576.
     * Intermediate chapters stop one millisecond before the next chapter;
     * the final chapter may reach the movie duration. Missing chapters fail. */
    bool GetChapterRange(unsigned chapter, unsigned &start, unsigned &end) const;
    unsigned width = 0, height = 0;
    std::uint32_t duration = 0;
    std::vector<CMovieObject> objects;
    CMovieChapter chapter;
};
#endif
