/** @file ZTextLayout.h
 * @brief Shared original CTextBox font controls and word wrapping.
 * Source: CTextBox::paint :104153 and CMenuList::Bind :140481.
 */
#ifndef GUN_BROS_RE_ZTEXTLAYOUT_H
#define GUN_BROS_RE_ZTEXTLAYOUT_H
#include "engine/glu/movie/ZMovieRenderer.h"
#include <algorithm>
#include <array>
#include <cctype>
struct ZStoreTextRun {
    std::string text;
    unsigned font = 1;
    float x = 0, width = 0, height = 0;
};
struct ZStoreTextLine {
    std::vector<ZStoreTextRun> runs;
    float width = 0, height = 0;
};

inline std::vector<ZStoreTextLine> FormatStoreText(ZMovieRenderer &renderer, const std::string &text, float width,
    const std::array<unsigned, 5> &fonts = {1, 2, 4, 3, 0}) {
    std::vector<ZStoreTextLine> lines(1);
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
        ZStoreTextRun run;
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

/** Shared font layout; each original menu supplies its own native label binding. */
inline bool DrawPopupText(ZMovieRenderer &movies, const ZMovieRegion &region,
    const char *name, bool heading, bool centerHeading) {
    if (!name) { return true; }
    const std::string text = movies.NamedString(name);
    if (text.empty()) { return false; }
    if (heading) {
        float y = region.y;
        if (centerHeading) { y += region.height / 2 - movies.TextHeight(6) / 2; }
        return movies.Text(text, region.x + region.width / 2 - movies.TextWidth(text, 6) / 2,
            y, 6, 1, 0, region.alpha);
    }
    const auto lines = FormatStoreText(movies, text, region.width, {0, 0, 0, 0, 0});
    float height = 0;
    for (const auto &line : lines) { height += line.height; }
    float y = region.y + (region.height - height) / 2;
    for (const auto &line : lines) {
        for (const auto &run : line.runs) {
            movies.Text(run.text, region.x + (region.width - line.width) / 2 + run.x,
                y, run.font, 1, 0, region.alpha);
        }
        y += line.height;
    }
    return true;
}

// Draw authored regions with the native menu's animation/action bindings.
inline bool DrawPopupControls(ZMovieRenderer &movies, unsigned ordinal, unsigned time,
    unsigned iconsRegion, unsigned closeRegion, const std::array<unsigned, 2> &animations,
    const std::array<unsigned, 2> &actions, std::vector<std::pair<ZMovieRegion, unsigned>> &hits) {
    ZMovieRegion icons, close;
    if (!movies.Region(ordinal, iconsRegion, time, icons) ||
        !movies.Region(ordinal, closeRegion, time, close)) { return false; }
    // Both original classes divide their authored icon strip into equal cells.
    for (unsigned cell = 0; cell < 2; ++cell) {
        unsigned animation = animations[0], action = actions[0];
        if (cell == 1) { animation = animations[1]; action = actions[1]; }
        const float x = icons.x + icons.width * (cell * 2 + 1) / 4;
        const float y = icons.y + icons.height / 2;
        ZMovieRegion bounds;
        if (!movies.SpriteBounds(29, animation, bounds) ||
            !movies.DrawSprite(29, animation, time, x, y)) { return false; }
        bounds.x += x; bounds.y += y;
        hits.push_back({bounds, action});
    }
    if (!movies.DrawSpriteFitted(0, 99, time, close.x, close.y, close.width, close.height)) { return false; }
    hits.push_back({close, 45});
    return true;
}
#endif
