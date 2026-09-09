/** @file CMovie.h
 * @brief Original Glu movie timeline and its typed keyframes.
 */
#ifndef GUN_BROS_RE_CMOVIE_H
#define GUN_BROS_RE_CMOVIE_H
#include "engine/CArrayInputStream.h"
#include <array>

struct MovieKeyFrame {
    std::uint32_t time = 0;
    std::int16_t x = 0, y = 0;
    float alpha = 1, scaleX = 1, scaleY = 1, rotation = 0;
    std::uint8_t layer = 10;
    std::uint8_t selfAnchor = 0, parentAnchor = 0, parent = 255;
    std::int16_t width = 0, height = 0;
    std::array<std::uint8_t, 4> content{};
    std::array<std::uint8_t, 4> tiledSprite{};
    bool visible = true;
    std::uint16_t font = 0, text = 0, region = 0;
    std::int32_t tileX = 0, tileY = 0;
    std::array<std::uint8_t, 6> colors{};
};

struct MovieObject {
    unsigned type = 0;
    std::vector<MovieKeyFrame> frames;
};

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
    std::vector<MovieObject> objects;
    std::vector<std::uint32_t> chapters;
};
#endif
