/** @file CDialogPopup.h
 * @brief CDialogPopup playback hosted by the BIG Movie renderer.
 * Sources: ui_movie.bt; CDialogPopup::Update/Show :183511/183585;
 * CTextBox::Setup/tick/setPageMode :103058/104254/103293.
 */
#ifndef GUN_BROS_RE_CDIALOGPOPUP_H
#define GUN_BROS_RE_CDIALOGPOPUP_H
#include "gun_bros_re/ui/controls/CTextBox.h"

class CDialogPopup : public ZMovieRegionCallback {
public:
    bool Show(ZMovieRenderer &renderer, const std::string &text, bool automatic, unsigned arrow) {
        if (text.empty()) { return false; }
        m_renderer = &renderer;
        m_movie = renderer.Ordinal("GLU_MOVIE_POP_UP");
        const CMovie *movie = renderer.GetMovie(m_movie);
        if (movie == nullptr || !movie->GetChapterRange(1, m_loopStart, m_loopEnd)) { return false; }
        ZMovieRegion region;
        if (!renderer.Region(m_movie, 1, m_loopStart, region) || region.width <= 0 || region.height <= 0) { return false; }
        m_lines = CTextBox::Format(renderer, text, region.width, {0, 0, 0, 0, 0});
        m_pages.clear();
        Page page;
        for (unsigned index = 0; index < m_lines.size(); ++index) {
            const auto &line = m_lines[index];
            if (page.end > page.begin && page.height + line.height > region.height) {
                m_pages.push_back(page);
                page = {};
                page.begin = index;
            }
            page.end = index + 1;
            page.height += line.height;
            for (const auto &run : line.runs) { page.characters += static_cast<unsigned>(run.text.size()); }
            if (line.runs.size() > 1) { page.characters += static_cast<unsigned>(line.runs.size() - 1); }
        }
        m_pages.push_back(page);
        m_duration = movie->duration;
        m_time = 0;
        m_elapsed = 0;
        m_textElapsed = 0;
        m_readMs = 0;
        m_page = 0;
        m_automatic = automatic;
        m_arrow = arrow;
        m_closing = false;
        m_done = false;
        return true;
    }

    void Clear(bool immediate) {
        if (immediate) { m_done = true; return; }
        // ClearChapterPlayback finishes the current loop, then the outro.
        m_closing = true;
    }

    void Update(unsigned deltaMs) {
        if (m_done) { return; }
        m_elapsed += deltaMs;
        m_time += deltaMs;
        if (!m_closing && m_time > m_loopEnd) {
            m_time = m_loopStart + (m_time - m_loopStart) % (m_loopEnd - m_loopStart + 1);
        }
        if (m_closing && m_time >= m_duration) { m_done = true; return; }
        if (m_closing || m_time < m_loopStart) { return; }
        // Show configures 50 ms per character, then >1000 ms per completed page.
        m_textElapsed += deltaMs;
        if (m_textElapsed < m_pages[m_page].characters * kCharacterMs) { return; }
        m_readMs += deltaMs;
        if (m_readMs <= kPageHoldMs) { return; }
        m_readMs = 0;
        if (m_page + 1 < m_pages.size()) {
            ++m_page;
            m_textElapsed = 0;
        } else if (m_automatic) { Clear(false); }
    }

    bool IsDone() const { return m_done; }
    unsigned PageCount() const { return static_cast<unsigned>(m_pages.size()); }

    bool Draw() {
        if (m_done) { return true; }
        if (!m_renderer->Draw(m_movie, m_time, 512, 384, 1024, 768, 0, 1, this)) { return false; }
        if (m_arrow == 0) { return true; }
        const unsigned ordinal = m_renderer->Ordinal("GLU_MOVIE_TUT_ARROWS");
        const CMovie *movie = m_renderer->GetMovie(ordinal);
        unsigned start = 0, end = 0;
        if (m_arrow > 4 || movie == nullptr || !movie->GetChapterRange(m_arrow - 1, start, end)) { return false; }
        return m_renderer->Draw(ordinal, start + m_elapsed % (end - start + 1));
    }

    bool DrawMovieRegion(const ZMovieRegion &region) override {
        if (region.index == 0) {
            // Original core archetype 1, animations 85 -> 86 -> 87.
            // The second 3000 ms countdown starts after the first 1000 ms.
            unsigned animation = 85, time = m_elapsed;
            if (m_elapsed >= 4000) { animation = 87; time -= 4000; }
            else if (m_elapsed >= 1000) { animation = 86; time -= 1000; }
            return m_renderer->DrawSprite(1, animation, time, region.x, region.y, 1, region.alpha);
        }
        if (region.index != 1) { return true; }
        const Page &page = m_pages[m_page];
        float y = region.y + static_cast<int>(region.height - page.height) / 2;
        unsigned remaining = m_textElapsed / kCharacterMs;
        for (unsigned line = page.begin; line < page.end; ++line) {
            bool first = true;
            for (const auto &run : m_lines[line].runs) {
                if (!first && remaining > 0) { --remaining; }
                first = false;
                const unsigned count = std::min(remaining, static_cast<unsigned>(run.text.size()));
                if (count > 0 && !m_renderer->Text(run.text.substr(0, count), region.x + run.x, y, 0, 1, 0, region.alpha)) { return false; }
                remaining -= count;
            }
            y += m_lines[line].height;
        }
        return true;
    }

private:
    static constexpr unsigned kCharacterMs = 50;
    static constexpr unsigned kPageHoldMs = 1000;
    struct Page { unsigned begin = 0, end = 0, characters = 0; float height = 0; };
    ZMovieRenderer *m_renderer = nullptr;
    std::vector<CTextBox::Line> m_lines;
    std::vector<Page> m_pages;
    unsigned m_movie = 0, m_loopStart = 0, m_loopEnd = 0, m_duration = 0;
    unsigned m_time = 0, m_elapsed = 0, m_textElapsed = 0, m_readMs = 0, m_page = 0, m_arrow = 0;
    bool m_automatic = false, m_closing = false, m_done = true;
};
#endif
