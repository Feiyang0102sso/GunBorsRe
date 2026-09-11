/** @file OriginalTextLayout.h
 * @brief Shared original CTextBox font controls and word wrapping.
 * Source: CTextBox::paint :104153 and CMenuList::Bind :140481.
 */
#ifndef GUN_BROS_RE_ORIGINALTEXTLAYOUT_H
#define GUN_BROS_RE_ORIGINALTEXTLAYOUT_H
#include "engine/glu/movie/MovieRenderer.h"
#include <algorithm>
#include <array>
#include <cctype>
struct StoreTextRun {
    std::string text;
    unsigned font = 1;
    float x = 0, width = 0, height = 0;
};
struct StoreTextLine {
    std::vector<StoreTextRun> runs;
    float width = 0, height = 0;
};

inline std::vector<StoreTextLine> FormatStoreText(MovieRenderer &renderer, const std::string &text, float width,
    const std::array<unsigned, 5> &fonts = {1, 2, 4, 3, 0}) {
    std::vector<StoreTextLine> lines(1);
    unsigned font = fonts[0];
    float space = 0;
    for (std::size_t position = 0; position < text.size();) {
        if (text[position] == '^' && position + 2 < text.size() && text[position + 1] == 'f' &&
            text[position + 2] >= '0' && text[position + 2] <= '9') {
            font = fonts[std::min(4u, static_cast<unsigned>(text[position + 2] - '0'))];
            position += 3;
            continue;
        }
        if (text[position] == '\n') {
            lines.back().height = std::max(lines.back().height, renderer.TextHeight(font));
            lines.push_back({});
            space = 0;
            ++position;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(text[position]))) {
            space += renderer.TextWidth(" ", font);
            ++position;
            continue;
        }
        std::size_t end = position + 1;
        while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end])) && text[end] != '^') { ++end; }
        StoreTextRun run;
        run.text = text.substr(position, end - position);
        run.font = font;
        run.width = renderer.TextWidth(run.text, font);
        run.height = renderer.TextHeight(font);
        if (!lines.back().runs.empty() && lines.back().width + space + run.width > width) {
            lines.push_back({});
        }
        if (lines.back().runs.empty()) { space = 0; }
        run.x = lines.back().width + space;
        lines.back().width = run.x + run.width;
        lines.back().height = std::max(lines.back().height, run.height);
        lines.back().runs.push_back(run);
        space = 0;
        position = end;
    }
    return lines;
}

#endif
