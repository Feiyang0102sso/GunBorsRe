#include "gun_bros_re/ui/controls/CTextBox.h"
#include <algorithm>
#include <cctype>
std::vector<CTextBox::Line> CTextBox::Format(ZMovieRenderer &renderer, const std::string &text, float width,
    const std::array<unsigned, 5> &fonts) {
    std::vector<Line> lines(1);
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
        Run run;
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
bool CTextBox::PaintLabel(ZMovieRenderer &movies, const ZMovieRegion &region,
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
    const auto lines = Format(movies, text, region.width, {0, 0, 0, 0, 0});
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

