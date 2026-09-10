/** @file MovieStudy.cpp
 * @brief Catalogue original UI timelines without altering any source assets.
 */
#define NOMINMAX
#include "runtime/MovieStudy.h"
#include "runtime/MovieRenderer.h"
#include "engine/platform/CWindow.h"
#include "gun_bros/CMovie.h"
#include "gun_bros/CResTOCManager.h"
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <algorithm>

int RunMovieStudy(const std::string &bigDirectory, unsigned ordinal, const std::string &screenshotPath, unsigned advanceMs,
    bool gallery, bool regionOverlay) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    CWindow window;
    if (!window.Open("Gun Bros - Original UI Movies", 1024, 768)) { return 1; }
    MovieRenderer renderer;
    CResPackTOC *core = toc.GetPack(toc.GetCorePackIndex());
    if (!renderer.Init(*core, *core)) { return 1; }
    if (ordinal == 73 && !screenshotPath.empty()) {
        CSpriteGlu glu;
        if (!glu.Init(*core)) { return 1; }
        const CSpriteGluArchetype *archetype = glu.GetArchetype(0);
        CSpriteIterator iterator(glu, *archetype);
        std::vector<SpriteQuad> quads;
        iterator.Expand(113, 0, quads);
        for (const SpriteQuad &quad : quads) {
            std::printf("[button-sprite] xy=%d,%d source=%u,%u,%u,%u flip=%d,%d rotate=%d blend=%d\n",
                quad.offsetX, quad.offsetY, quad.source.x, quad.source.y, quad.source.width, quad.source.height,
                quad.flipHorizontal, quad.flipVertical, quad.rotateTexture, static_cast<int>(quad.blend));
        }
    }
    if (gallery) { std::filesystem::create_directories("out/ui-movies"); }
    // User regions carry the original layout, so screenshots must show them too.
    bool overlay = regionOverlay;
    renderer.SetRegionOverlay(overlay);
    std::uint64_t start = window.GetTicksMs();
    while (window.PumpEvents()) {
        for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None; key = window.TakeKeyPress()) {
            if (key == KeyCode::Right) { ordinal = (ordinal + 1) % 148; start = window.GetTicksMs(); }
            if (key == KeyCode::Left) { ordinal = (ordinal + 147) % 148; start = window.GetTicksMs(); }
            if (key == KeyCode::C) { overlay = !overlay; renderer.SetRegionOverlay(overlay); }
        }
        CMovie *movie = renderer.GetMovie(ordinal);
        if (movie == nullptr) { return 1; }
        unsigned time = advanceMs;
        if (screenshotPath.empty() && !gallery) { time += static_cast<unsigned>(window.GetTicksMs() - start); }
        if (gallery) { time = std::min(1600u, movie->duration / 2); }
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        glViewport(0, 0, width, height);
        glClearColor(0.09f, 0.09f, 0.09f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        const unsigned failuresBefore = renderer.Failures();
        renderer.Draw(ordinal, time);
        renderer.Text("MOVIE " + std::to_string(ordinal) + " / " + std::to_string(movie->duration) + " MS", 12, 732, 0, 0.65f);
        if (renderer.Failures() != failuresBefore) { std::printf("[movie-render] movie=%u new-failures=%u\n", ordinal, renderer.Failures() - failuresBefore); }
        if (gallery) {
            char path[96];
            std::snprintf(path, sizeof(path), "out/ui-movies/%03u.png", ordinal);
            if (!window.SaveFrame(path)) { return 1; }
            if (++ordinal >= 148) {
                std::printf("[movie-render] movies=148 failures=%u\n", renderer.Failures());
                return renderer.Failures() != 0;
            }
        } else if (!screenshotPath.empty()) {
            if (!window.SaveFrame(screenshotPath)) { return 1; }
            return renderer.Failures() != 0;
        }
        window.Present();
    }
    return 0;
}

int RunMovieCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    std::ofstream report("out/movie-check.txt");
    unsigned movies = 0;
    unsigned failures = 0;
    const char *names[] = {
#include "runtime/MovieNames.inc"
    };
    for (unsigned packIndex = 0; packIndex < toc.GetPackCount(); ++packIndex) {
        CResPackTOC *pack = toc.GetPack(packIndex);
        const unsigned base = pack->GetResValue("GLU_MOVIE_MOVIE");
        if (base == 0) { continue; }
        // Fixed boundaries established against the shipped 3.6 BIG series.
        // A corrupt middle resource must be a failure, not silently end a scan.
        const std::map<std::string, unsigned> movieCounts = {{"pack0_core", 148}, {"pack1", 3}, {"pack2", 5},
            {"pack3", 3}, {"pack4", 3}, {"pack5", 6}, {"pack6", 1}, {"pack7", 4}, {"pack8", 1}, {"pack9", 1},
            {"pack10", 0}, {"pack11", 0}, {"pack12", 0}};
        const auto count = movieCounts.find(pack->GetShortName());
        if (count == movieCounts.end()) { ++failures; continue; }
        const unsigned expectedCount = count->second;
        // Empty packs still declare a base; it points to the following section.
        if (expectedCount == 0) { report << pack->GetShortName() << " movies=0 declared-base=" << base << '\n'; continue; }
        for (const char *name : names) {
            const unsigned handle = pack->GetResValue(name);
            if (handle != 0) { report << "alias " << pack->GetShortName() << ' ' << name << " movie=" << handle - base << '\n'; }
            if (handle != 0 && (handle < base || handle - base >= expectedCount)) { ++failures; }
        }
        // All named movies are aliases into this contiguous resource series.
        // The first non-movie closes the series; inspect its header in the report.
        // The comment above describes the initial boundary discovery; checks
        // now use its recorded bounds and validate every resource within them.
        for (unsigned ordinal = 0; ordinal < expectedCount; ++ordinal) {
            std::vector<std::uint8_t> bytes;
            if (!pack->GetResource(base + ordinal, bytes)) { ++failures; continue; }
            CArrayInputStream stream(bytes);
            CMovie movie;
            if (!movie.Init(stream)) {
                ++failures;
                report << pack->GetShortName() << " end=" << ordinal << " handle=" << base + ordinal << " bytes=" << bytes.size()
                    << " dimensions=" << movie.width << 'x' << movie.height << " remaining=" << stream.Available() << '\n';
                continue;
            }
            ++movies;
            if (pack->GetShortName() == "pack0_core" && ordinal == 15) {
                // The right strip is authored with width -3. Interpreting it
                // as 65533 produces a huge tiled band across the gallery.
                if (movie.objects.size() <= 5 || movie.objects[5].frames.empty() || movie.objects[5].frames[0].width != -3) { ++failures; }
            }
            report << pack->GetShortName() << " movie=" << ordinal << " handle=" << base + ordinal << ' ' << movie.width << 'x' << movie.height
                << " duration=" << movie.duration << " objects=" << movie.objects.size() << " chapters=";
            for (unsigned chapter : movie.chapters) { report << chapter << ','; }
            report << '\n';
            unsigned objectIndex = 0;
            for (const MovieObject &object : movie.objects) {
                report << " object=" << objectIndex++ << " type=" << object.type << " frames=" << object.frames.size() << '\n';
                for (const MovieKeyFrame &frame : object.frames) {
                    report << "  t=" << frame.time << " xy=" << frame.x << ',' << frame.y << " wh=" << frame.width << ',' << frame.height
                        << " a=" << frame.alpha << " scale=" << frame.scaleX << ',' << frame.scaleY << " rot=" << frame.rotation
                        << " layer=" << unsigned(frame.layer) << " anchor=" << unsigned(frame.selfAnchor) << ',' << unsigned(frame.parentAnchor)
                        << ',' << unsigned(frame.parent) << " content=" << unsigned(frame.content[0]) << ',' << unsigned(frame.content[1])
                        << ',' << unsigned(frame.content[2]) << ',' << unsigned(frame.content[3]) << " visible=" << frame.visible
                        << " text=" << frame.font << ',' << frame.text << ',' << frame.region << '\n';
                }
            }
        }
    }
    CResPackTOC *core = toc.GetPack(toc.GetCorePackIndex());
    std::vector<std::uint8_t> fontBytes;
    for (unsigned page = 0; page < 4; ++page) {
        std::vector<std::uint8_t> bytes;
        if (!core->GetResource(core->GetResValue("BASE_TEXTURE_PAGE_0") + page, bytes)) { ++failures; continue; }
        std::ofstream image("out/ui-atlas-" + std::to_string(page) + ".png", std::ios::binary);
        image.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    }
    if (core->GetResource(core->GetResValue("FONT_KEYSET"), fontBytes)) {
        CArrayInputStream fonts(fontBytes);
        const unsigned count = fonts.ReadUInt16();
        for (unsigned index = 0; index < count / 2; ++index) {
            const unsigned metrics = fonts.ReadUInt32();
            const unsigned texture = fonts.ReadUInt32();
            std::vector<std::uint8_t> bytes;
            if (!core->GetResource(metrics, bytes)) { ++failures; continue; }
            report << "font=" << index << " metrics=" << metrics << " texture=" << texture << " size=" << bytes.size() << " header=";
            for (unsigned byte = 0; byte < 24 && byte < bytes.size(); ++byte) { report << unsigned(bytes[byte]) << ','; }
            report << '\n';
        }
    }
    if (movies == 0) { ++failures; }
    CWindow window;
    if (!window.Open("Gun Bros - Original Fonts", 1024, 768)) { return 1; }
    MovieRenderer fontRenderer;
    if (!fontRenderer.Init(*core, *core)) { return 1; }
    // Inspect every authored leaf of the compatibility frame, including leaves
    // that the iterator currently discards. This is a visual regression probe.
    {
        CSpriteGlu glu;
        if (!glu.Init(*core)) { return 1; }
        const auto *archetype = glu.GetArchetype(0);
        CSpriteIterator iterator(glu, *archetype);
        std::vector<SpriteQuad> quads;
        if (!iterator.Expand(172, 0, quads)) { return 1; }
        std::printf("[decoration-check] sprite=0:172 quads=%zu skipped=%u\n", quads.size(), iterator.GetSkippedPartCount());
        if (iterator.GetSkippedPartCount() != 0) { ++failures; }
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!fontRenderer.DrawSprite(0, 172, 0, 100, 100)) { return 1; }
        std::vector<std::uint8_t> pixels(120 * 32 * 4);
        glReadPixels(120, 768 - 132, 120, 32, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        unsigned upper = 0, lower = 0;
        for (unsigned y = 0; y < 7; ++y) {
            for (unsigned x = 0; x < 120; ++x) {
                if (pixels[(y * 120 + x) * 4 + 1] > 12) { ++lower; }
                if (pixels[((31 - y) * 120 + x) * 4 + 1] > 12) { ++upper; }
            }
        }
        if (upper < 100 || lower < 100) { ++failures; }
        std::printf("[decoration-border-check] upper=%u lower=%u failures=%u\n", upper, lower, failures);
        if (!window.SaveFrame("out/powerup-border-check.png")) { ++failures; }
    }
    glClearColor(0.035f, 0.05f, 0.07f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    for (unsigned index = 0; index < 13; ++index) {
        fontRenderer.Text("FONT " + std::to_string(index), 20, 15 + index * 55.0f, 0, 0.7f);
        if (!fontRenderer.Text("GUN BROS 0123456789", 180, 15 + index * 55.0f, index, 1, 790)) { ++failures; }
    }
    if (!window.SaveFrame("out/ui-fonts.png")) { ++failures; }
    // MDS_BUTTON_* selects these original button backgrounds dynamically;
    // the movie alone contains only their transition/region placeholders.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for (unsigned index = 0; index < 24; ++index) {
        const unsigned animation = 62 + index;
        const float x = 16 + (index % 4) * 252.0f;
        const float y = 12 + (index / 4) * 125.0f;
        fontRenderer.Text("SPRITE 0:" + std::to_string(animation), x, y, 0, 0.6f);
        if (!fontRenderer.DrawSpriteFitted(0, animation, 0, x + 4, y + 25, 222, 70)) { ++failures; }
    }
    if (!window.SaveFrame("out/ui-components.png")) { ++failures; }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for (unsigned index = 0; index < 3; ++index) {
        const float x = 60 + index * 320.0f;
        fontRenderer.Text("WAVE 5:" + std::to_string(21 + index), x, 240, 0, 0.8f);
        if (!fontRenderer.DrawSpriteFitted(5, 21 + index, 0, x, 300, 220, 220)) { ++failures; }
    }
    if (!window.SaveFrame("out/ui-wave-components.png")) { ++failures; }
    // Original shop badges and card backgrounds share archetype 5 with waves.
    // Keep this contact sheet in the permanent movie check for future research.
    for (unsigned sheet = 0; sheet < 4; ++sheet) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        for (unsigned cell = 0; cell < 20; ++cell) {
            const unsigned animation = sheet * 20 + cell;
            if (animation >= 65) { break; }
            const float x = 10 + (cell % 4) * 254.0f;
            const float y = 10 + (cell / 4) * 149.0f;
            fontRenderer.Text("5:" + std::to_string(animation), x, y, 0, 0.6f);
            fontRenderer.DrawSpriteFitted(5, animation, 0, x, y + 22, 235, 115);
        }
        if (!window.SaveFrame("out/ui-shop-sprites-" + std::to_string(sheet) + ".png")) { ++failures; }
    }
    // Archetype 0 is the shared menu character: currency icons, mastery stars,
    // button plates. Sheeting all of it keeps icon hunts out of guesswork.
    for (unsigned sheet = 0; sheet < 10; ++sheet) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        for (unsigned cell = 0; cell < 20; ++cell) {
            const unsigned animation = sheet * 20 + cell;
            if (animation >= 193) { break; }
            const float x = 10 + (cell % 4) * 254.0f;
            const float y = 10 + (cell / 4) * 149.0f;
            fontRenderer.Text("0:" + std::to_string(animation), x, y, 0, 0.6f);
            fontRenderer.DrawSpriteFitted(0, animation, 0, x, y + 22, 235, 115);
        }
        if (!window.SaveFrame("out/ui-menu-sprites-" + std::to_string(sheet) + ".png")) { ++failures; }
    }
    // CMenuSystem::Load pulls character 23 with the menu itself and CMenuStore
    // adds 26; between them they hold the currency icons and the mastery meter.
    // Character 19 is the small player portrait set the menus print.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for (unsigned cell = 0; cell < 20; ++cell) {
        const float x = 10 + (cell % 4) * 254.0f;
        const float y = 10 + (cell / 4) * 149.0f;
        if (!fontRenderer.DrawSpriteFitted(19, cell, 0, x, y + 22, 235, 115)) { break; }
        fontRenderer.Text("19:" + std::to_string(cell), x, y, 0, 0.6f);
    }
    if (!window.SaveFrame("out/ui-portrait-sprites.png")) { ++failures; }
    for (unsigned character = 23; character <= 26; character += 3) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        for (unsigned cell = 0; cell < 20; ++cell) {
            const float x = 10 + (cell % 4) * 254.0f;
            const float y = 10 + (cell / 4) * 149.0f;
            // Both characters end before the sheet does; stop at the first gap.
            if (!fontRenderer.DrawSpriteFitted(character, cell, 0, x, y + 22, 235, 115)) { break; }
            fontRenderer.Text(std::to_string(character) + ":" + std::to_string(cell), x, y, 0, 0.6f);
        }
        if (!window.SaveFrame("out/ui-currency-sprites-" + std::to_string(character) + ".png")) { ++failures; }
    }
    // CInputPad uses archetype 1 for both sticks and the bottom-rail actions.
    for (unsigned sheet = 0; sheet < 3; ++sheet) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        for (unsigned cell = 0; cell < 20; ++cell) {
            const unsigned animation = sheet * 20 + cell;
            const float x = 10 + (cell % 4) * 254.0f;
            const float y = 10 + (cell / 4) * 149.0f;
            fontRenderer.Text("1:" + std::to_string(animation), x, y, 0, 0.6f);
            fontRenderer.DrawSpriteFitted(1, animation, 600, x, y + 22, 235, 115);
        }
        if (!window.SaveFrame("out/ui-hud-sprites-" + std::to_string(sheet) + ".png")) { ++failures; }
    }
    for (unsigned sheet = 0; sheet < 2; ++sheet) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        for (unsigned cell = 0; cell < 20; ++cell) {
            const unsigned animation = sheet * 20 + cell;
            if (animation >= 33) { break; }
            const float x = 10 + (cell % 4) * 254.0f;
            const float y = 10 + (cell / 4) * 149.0f;
            fontRenderer.Text("REFINERY 4:" + std::to_string(animation), x, y, 0, 0.6f);
            fontRenderer.DrawSpriteFitted(4, animation, 600, x, y + 22, 235, 115);
        }
        if (!window.SaveFrame("out/ui-refinery-sprites-" + std::to_string(sheet) + ".png")) { ++failures; }
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    fontRenderer.DrawSpriteFitted(0, 124, 200, 50, 150, 150, 150);
    for (unsigned index = 0; index < 3; ++index) {
        fontRenderer.Text("RADIO 1:" + std::to_string(85 + index), 245 + index * 252.0f, 100, 0, 0.7f);
        fontRenderer.DrawSpriteFitted(1, 85 + index, 600, 245 + index * 252.0f, 150, 235, 200);
    }
    if (!window.SaveFrame("out/ui-radio-loading-sprites.png")) { ++failures; }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for (unsigned cell = 0; cell < 20; ++cell) {
        const float x = 10 + (cell % 4) * 254.0f;
        const float y = 10 + (cell / 4) * 149.0f;
        fontRenderer.Text("SOCIAL 6:" + std::to_string(cell), x, y, 0, 0.6f);
        fontRenderer.DrawSpriteFitted(6, cell, 600, x, y + 22, 235, 115);
    }
    if (!window.SaveFrame("out/ui-social-sprites.png")) { ++failures; }
    if (movies != 175) { ++failures; }
    std::printf("[movie-check] movies=%u failures=%u report=out/movie-check.txt\n", movies, failures);
    return failures != 0;
}
