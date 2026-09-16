#pragma once
#include <vector>
#include "engine/resources/CArrayInputStream.h"
#include <array>

/** Decoded keyframe representation shared by the original Movie object types.
 * Storage is a desktop representation; field order is defined by ui_movie.bt.
 */
struct ZMovieKeyFrame {
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

class CMovieObject {
public:
    bool Init(CArrayInputStream &stream);
    unsigned type = 0;
    std::vector<ZMovieKeyFrame> frames;
};
