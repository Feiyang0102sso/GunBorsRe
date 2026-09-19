/** @file CTextBox.h
 * @brief Shared original CTextBox font controls and word wrapping.
 * Source: CTextBox::paint :104153 and CMenuList::Bind :140481.
 */
#pragma once
#include "engine/glu/movie/ZMovieRenderer.h"
#include <array>
/** Native font-switch tokens and word layout; popup menus supply label binding. */
class CTextBox {
public:
    struct Run {
        std::string text;
        unsigned font = 1;
        float x = 0, width = 0, height = 0;
    };
    struct Line {
        std::vector<Run> runs;
        float width = 0, height = 0;
    };
    static std::vector<Line> Format(ZMovieRenderer &renderer, const std::string &text, float width,
        const std::array<unsigned, 5> &fonts = {1, 2, 4, 3, 0});
    static bool PaintLabel(ZMovieRenderer &movies, const ZMovieRegion &region,
        const char *name, bool heading, bool centerHeading);
};
