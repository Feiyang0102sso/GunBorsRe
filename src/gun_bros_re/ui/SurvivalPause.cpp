/** @file SurvivalHud.cpp
 * @brief CInputPad::Base::Bind (:88320) binds meters to regions 0/1 and guns to 2/3.
 */
#define NOMINMAX
#include "gun_bros_re/ui/SurvivalHud.h"
#include "gun_bros_re/ui/OriginalMenuData.h"
#include "gun_bros_re/ui/OriginalTextLayout.h"
#include "gun_bros_re/HostSettings.h"
#include "gun_bros_re/data/PowerupCatalog.h"
#include "engine/platform/CWindow.h"
#include "engine/resources/CResTOCManager.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "engine/graphics/CPNG.h"
#include <algorithm>
#include <cstdio>
#include <sstream>

void SurvivalHud::Scroll(const SurvivalHudState &state, float amount) {
    if (state.remoteShop) { return; }
    if (amount == 0) { return; }
    if (state.shopOpen) {
        if (!state.itemChoice) {
            const float maximum = std::max(0.0f, float(m_selectorEntries.size()) - 3);
            m_selectorTarget = std::clamp(m_selectorTarget - amount, std::min(2.0f, maximum), maximum);
        }
        return;
    }
    if (state.paused && !state.shopOpen) {
        MovieRegion content;
        const unsigned movie = m_movies.Ordinal("GLU_MOVIE_LIST_MENU_PAUSE");
        unsigned start = 0, end = 0;
        if (!m_movies.GetMovie(movie)->GetChapterRange(1, start, end)) { return; }
        if (m_movies.Region(movie, 8, start, content) && content.Contains(m_mouseX, m_mouseY)) {
            m_pauseBodyPosition = std::max(0.0f, m_pauseBodyPosition - amount);
        } else {
            m_pauseTarget = std::clamp(m_pauseTarget - amount, 0.0f, std::max(0.0f, float(m_pauseItems.size()) - 3));
        }
        return;
    }

}

void SurvivalHud::ScrollMenuInput(const SurvivalHudState &state, float wheel, float dragX, float dragY) {
    if (state.remoteShop) { return; }
    if (state.shopOpen) {
        Scroll(state, wheel);
        if (state.itemChoice || m_selectorPromptRequested || m_selectorPrompt.IsActive() || dragX == 0) { return; }
        const unsigned layout = m_movies.Ordinal("GLU_MOVIE_POWER_UP_LAYOUT");
        unsigned start = 0, end = 0;
        MovieRegion first, second;
        if (!m_movies.GetMovie(layout)->GetChapterRange(1, start, end) ||
            !m_movies.Region(layout, 2, start, first) || !m_movies.Region(layout, 3, start, second)) { return; }
        const float spacing = std::abs(second.x + second.width / 2 - first.x - first.width / 2);
        if (spacing <= 0) { return; }
        // Drag input is in the same 1024-wide logical canvas as the BIG layout.
        // Follow the pointer immediately, leaving fractional positions at rest.
        m_selectorTarget = m_selectorPosition;
        Scroll(state, dragX / spacing);
        m_selectorPosition = m_selectorTarget;
        return;
    }
    if (state.paused && std::abs(dragY) > 8) { wheel = dragY; }
    Scroll(state, wheel);
}

void SurvivalHud::ResetPauseList() {
    m_pauseBound = false;
    m_pauseTime = 0;
    m_pauseBodyTime = 0;
    m_pauseFocus = 0;
    m_pausePosition = 0;
    m_pauseTarget = 0;
    m_pauseBodyPosition = 0;
    m_pauseHits.clear();
}

void SurvivalHud::AdvanceMenu(unsigned deltaMs) {
    m_pauseDelta = deltaMs;
    if (!m_selectorBound) { return; }
    m_selectorTime += deltaMs;
    m_selectorChoiceTime += deltaMs;
    m_selectorPrompt.Update(deltaMs);
    unsigned start = 0, end = 0;
    if (m_movies.GetMovie(m_movies.Ordinal("GLU_MOVIE_POWER_UP_LAYOUT"))->GetChapterRange(1, start, end)) {
        const float step = float(deltaMs) / (end - start + 1);
        if (m_selectorPosition < m_selectorTarget) { m_selectorPosition = std::min(m_selectorPosition + step, m_selectorTarget); }
        else { m_selectorPosition = std::max(m_selectorPosition - step, m_selectorTarget); }
    }
}

bool SurvivalHud::BackFromHelp() {
    if (!m_pauseHelp) { return false; }
    m_pauseHelp = false;
    ResetPauseList();
    return true;
}

bool SurvivalHud::DrawOriginalPause(const SurvivalHudState &state) {
    const unsigned list = m_movies.Ordinal("GLU_MOVIE_LIST_MENU_PAUSE");
    const unsigned button = m_movies.Ordinal("GLU_MOVIE_LIST_MENU_BUTTON");
    const unsigned body = m_movies.Ordinal("GLU_MOVIE_LIST_MENU_TEXT");
    unsigned start = 0, end = 0, focusStart = 0, focusEnd = 0, restStart = 0, restEnd = 0;
    unsigned bodyStart = 0, bodyEnd = 0, textStart = 0, textEnd = 0;
    if (!m_movies.GetMovie(list)->GetChapterRange(1, start, end) ||
        !m_movies.GetMovie(button)->GetChapterRange(2, focusStart, focusEnd) ||
        !m_movies.GetMovie(button)->GetChapterRange(0, restStart, restEnd) ||
        !m_movies.GetMovie(body)->GetChapterRange(0, bodyStart, bodyEnd) ||
        !m_movies.GetMovie(body)->GetChapterRange(1, textStart, textEnd)) { return false; }
    if (!m_pauseBound) {
        m_pauseBound = true;
        m_pauseItems.clear();
        const char *table = "MDS_PAUSE_ROOT";
        if (m_pauseHelp) { table = "MDS_HELP"; }
        for (unsigned index = 0; OriginalMenuData(table, index) != nullptr; ++index) {
            if (!m_pauseHelp && OriginalMenuData(table, index)->action == 135) { continue; }
            m_pauseItems.push_back(index);
        }
        m_pauseButtonTimes.assign(m_pauseItems.size(), 0);
        m_pauseButtonTimes[0] = focusStart;
    } else {
        m_pauseTime = std::min(start, m_pauseTime + m_pauseDelta);
        m_pauseBodyTime = std::min(bodyEnd, m_pauseBodyTime + m_pauseDelta);
    }
    const float step = float(m_pauseDelta) / (end - start + 1);
    if (m_pausePosition < m_pauseTarget) { m_pausePosition = std::min(m_pauseTarget, m_pausePosition + step); }
    else { m_pausePosition = std::max(m_pauseTarget, m_pausePosition - step); }
    const int first = static_cast<int>(std::floor(m_pausePosition)) - 1;
    unsigned time = m_pauseTime;
    if (time == start) { time += unsigned((m_pausePosition - std::floor(m_pausePosition)) * (end - start)); }
    m_pauseHits.clear();
    // CMenuSystem supplies HEADER with NAVBAR_DISABLED; no trunk buttons.
    const unsigned header = m_movies.Ordinal("GLU_MOVIE_HEADER");
    unsigned headerStart = 0, headerEnd = 0;
    if (!m_movies.GetMovie(header)->GetChapterRange(2, headerStart, headerEnd)) { return false; }
    unsigned headerTime = m_pauseTime;
    if (m_pauseTime == start) { headerTime = headerStart; }
    class PauseHeader : public IMovieRegionCallback {
    public:
        PauseHeader(MovieRenderer &renderer, const SurvivalHudState &snapshot) : movies(renderer), state(snapshot) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index == 14 || region.index == 15) {
                std::uint32_t value = static_cast<std::uint32_t>(state.coins);
                if (region.index == 15) { value = static_cast<std::uint32_t>(state.warbucks); }
                movies.Text(std::to_string(value), region.x, region.y, 0, 1, 0, region.alpha);
            }
            if (region.index == 16) {
                const unsigned info = movies.Ordinal("GLU_MOVIE_INFO_CLUSTER");
                unsigned start = 0, end = 0;
                start = 0; // INFO_CLUSTER has no chapter track; native code loops duration.
                if (!movies.Draw(info, start, region.x, region.y)) { return false; }
                for (const auto &part : movies.Regions(info, start, region.x, region.y)) {
                    if (part.index == 0) {
                        const float ratio = std::clamp(float(state.experience) / std::max<std::uint64_t>(1, state.experienceDelta), 0.0f, 1.0f);
                        movies.Rectangle(part.x, part.y, int(part.width * ratio), part.height, 1.0f / 255, 149.0f / 255, 215.0f / 255, part.alpha);
                    } else if (part.index < 4) {
                        char digits[16];
                        std::snprintf(digits, sizeof(digits), "%.3u", state.level);
                        movies.Text(std::string(1, digits[part.index - 1]), part.x, part.y + part.height / 2 - std::floor(movies.TextHeight(7) / 2), 7, 1, 0, part.alpha);
                    }
                }
            }
            return true;
        }
        MovieRenderer &movies;
        const SurvivalHudState &state;
    } headerCallback(m_movies, state);
    if (m_pauseHelp && !m_movies.DrawNamed("GLU_MOVIE_BG_OPTIONS", m_pauseTime)) { return false; }
    if (!m_movies.Draw(header, headerTime, 512, 384, 1024, 768, 0, 1, &headerCallback) || !m_movies.Draw(list, time)) { return false; }
    // Shared RADIAL_WIDGET is present even when the root has no BACK action.
    for (const auto &region : m_movies.Regions(list, time)) {
        if (region.index != m_movies.Regions(list, time, 512, 384, true).size() - 3) { continue; }
        const unsigned radial = m_movies.Ordinal("GLU_MOVIE_RADIAL_WIDGET");
        unsigned radialStart = 0, radialEnd = 0;
        if (!m_movies.GetMovie(radial)->GetChapterRange(1, radialStart, radialEnd)) { return false; }
        if (!m_movies.Draw(radial, radialStart, region.x + int(region.width) / 2, region.y + int(region.height) / 2)) { return false; }
    }
    if (m_pauseHelp) {
        // CMenuList::Init :140686 creates BACK_BUTTON at region tagged 1.
        for (const auto &region : m_movies.Regions(list, time)) {
            if (region.index != m_movies.Regions(list, time, 512, 384, true).size() - 3) { continue; }
            const unsigned back = m_movies.Ordinal("GLU_MOVIE_BACK_BUTTON");
            unsigned backStart = 0, backEnd = 0;
            if (!m_movies.GetMovie(back)->GetChapterRange(0, backStart, backEnd)) { return false; }
            const float x = region.x + int(region.width) / 2, y = region.y + int(region.height) / 2;
            if (!m_movies.Draw(back, std::min(m_pauseTime, backEnd), x, y)) { return false; }
            if (m_pauseTime != start) { continue; }
            for (const auto &part : m_movies.Regions(back, backEnd, x, y, true)) {
                if (part.index == 0) { m_pauseHits.insert(m_pauseHits.begin(), {part, UINT32_MAX}); }
            }
        }
    }
    for (const auto &region : m_movies.Regions(list, time)) {
        if (region.type < 2) { continue; }
        const int index = first + int(region.type) - 2;
        if (index < 0 || index >= int(m_pauseItems.size())) { continue; }
        auto &buttonTime = m_pauseButtonTimes[index];
        if (unsigned(index) == m_pauseFocus) { buttonTime = focusStart + (buttonTime - focusStart + m_pauseDelta) % (focusEnd - focusStart + 1); }
        else { buttonTime = std::min(restEnd, buttonTime + m_pauseDelta); }
        const float x = region.x + int(region.width) / 2, y = region.y + int(region.height) / 2;
        const std::string label = PauseText(state, m_pauseItems[index], 0);
        class Caption : public IMovieRegionCallback {
        public:
            Caption(MovieRenderer &renderer, const std::string &caption) : movies(renderer), label(caption) {}
            bool DrawMovieRegion(const MovieRegion &part) override {
                if (part.index == 1) { movies.Text(label, part.x, part.y + int(part.height) / 2 - int(movies.TextHeight(0)) / 2, 0, 1, 0, part.alpha); }
                return true;
            }
            MovieRenderer &movies;
            const std::string &label;
        } callback(m_movies, label);
        if (!m_movies.Draw(button, buttonTime, x, y, 1024, 768, 0, region.alpha, &callback)) { return false; }
        if (m_pauseTime != start) { continue; }
        for (const auto &part : m_movies.Regions(button, buttonTime, x, y, true)) {
            if (part.index == 0) { m_pauseHits.push_back({part, unsigned(index)}); }
        }
    }
    MovieRegion content, scrollBar, pageBounds;
    if (!m_movies.Region(list, 8, time, content) || !m_movies.Region(list, 9, time, scrollBar) ||
        !m_movies.Region(body, 2, 0, pageBounds)) { return false; }
    const auto lines = FormatStoreText(m_movies, PauseText(state, m_pauseItems[m_pauseFocus], 1), content.width - scrollBar.width, {0, 6, 0, 0, 0});
    std::vector<std::vector<StoreTextLine>> pages(1);
    float pageHeight = 0;
    for (const auto &line : lines) {
        if (!pages.back().empty() && pageHeight + line.height > pageBounds.height) { pages.push_back({}); pageHeight = 0; }
        pages.back().push_back(line);
        pageHeight += line.height;
    }
    m_pauseBodyPosition = std::clamp(m_pauseBodyPosition, 0.0f, float(pages.size() - 1));
    const int firstPage = int(std::floor(m_pauseBodyPosition));
    unsigned textTime = m_pauseBodyTime;
    if (textTime == bodyEnd) { textTime = textStart + unsigned((m_pauseBodyPosition - firstPage) * (textEnd - textStart)); }
    if (!m_movies.Draw(body, textTime, content.x, content.y)) { return false; }
    GLint viewport[4], previousClip[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_SCISSOR_BOX, previousClip);
    const GLboolean clipped = glIsEnabled(GL_SCISSOR_TEST);
    glEnable(GL_SCISSOR_TEST);
    glScissor(int(content.x * viewport[2] / 1024), int((768 - content.y - content.height) * viewport[3] / 768),
        int(content.width * viewport[2] / 1024), int(content.height * viewport[3] / 768));
    for (const auto &region : m_movies.Regions(body, textTime, content.x, content.y)) {
        if (region.type < 2) { continue; }
        const int page = firstPage + int(region.type) - 2;
        if (page < 0 || page >= int(pages.size())) { continue; }
        float y = region.y;
        for (const auto &line : pages[page]) {
            for (const auto &run : line.runs) { m_movies.Text(run.text, region.x + run.x, y + (line.height - run.height) / 2, run.font, 1, 0, content.alpha * region.alpha); }
            y += line.height;
        }
    }
    glScissor(previousClip[0], previousClip[1], previousClip[2], previousClip[3]);
    if (!clipped) { glDisable(GL_SCISSOR_TEST); }
    if (pages.size() > 1) {
        const auto *entry = OriginalMenuData("MDS_SCROLLBARS", 1);
        const unsigned bar = m_movies.Ordinal(entry->movies[0]);
        MovieRegion bounds;
        if (!m_movies.Region(bar, 0, 0, bounds)) { return false; }
        if (!m_movies.Draw(bar, unsigned(m_movies.GetMovie(bar)->duration * m_pauseBodyPosition / (pages.size() - 1)),
            scrollBar.x, scrollBar.y + int(scrollBar.height) / 2 - int(bounds.height) / 2)) { return false; }
    }
    m_pauseDelta = 0;
    return m_movies.Failures() == 0;
}
