#include "gun_bros_re/debug/Capture.h"
/** @file MovieStudy.cpp
 * @brief Catalogue original UI timelines without altering any source assets.
 */
#define NOMINMAX
#include "TestOutput.h"
#include "tests/research/MovieStudy.h"
#include "engine/glu/movie/ZMovieRenderer.h"
#include "engine/platform/ZWindow.h"
#include "engine/glu/movie/CMovie.h"
#include "engine/resources/CResTOCManager.h"
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <algorithm>

int RunMovieStudy(const std::string &bigDirectory, unsigned ordinal, const std::string &screenshotPath, unsigned advanceMs,
    bool gallery, bool regionOverlay) {
    CResTOCManager toc;
    if (!toc.InitAuto(bigDirectory) || !toc.Bind()) { return 1; }
    ZWindow window;
    if (!window.Open("Gun Bros - Original UI Movies", 1024, 768)) { return 1; }
    ZMovieRenderer renderer;
    CResPackTOC *core = toc.GetPack(toc.GetCorePackIndex());
    if (!renderer.Init(*core, *core)) { return 1; }
    if (ordinal == 73 && !screenshotPath.empty()) {
        CSpriteGlu glu;
        if (!glu.Init(*core)) { return 1; }
        const ZSpriteArchetype *archetype = glu.GetArchetype(0);
        CSpriteIterator iterator(glu, *archetype);
        std::vector<ZSpriteQuad> quads;
        iterator.Expand(113, 0, quads);
        for (const ZSpriteQuad &quad : quads) {
            std::printf("[button-sprite] xy=%d,%d source=%u,%u,%u,%u flip=%d,%d rotate=%d blend=%d\n",
                quad.offsetX, quad.offsetY, quad.source.x, quad.source.y, quad.source.width, quad.source.height,
                quad.flipHorizontal, quad.flipVertical, quad.rotateTexture, static_cast<int>(quad.blend));
        }
    }
    
    if (gallery) { std::filesystem::create_directories(TestOutput::Path("ui-movies")); }

    // User regions carry the original layout, so screenshots must show them too.
    bool overlay = regionOverlay;
    renderer.SetRegionOverlay(overlay);
    std::uint64_t start = window.GetTicksMs();
    while (window.PumpEvents()) {
        for (ZKeyCode key = window.TakeKeyPress(); key != ZKeyCode::None; key = window.TakeKeyPress()) {
            if (key == ZKeyCode::Right) { ordinal = (ordinal + 1) % 148; start = window.GetTicksMs(); }
            if (key == ZKeyCode::Left) { ordinal = (ordinal + 147) % 148; start = window.GetTicksMs(); }
            if (key == ZKeyCode::C) { overlay = !overlay; renderer.SetRegionOverlay(overlay); }
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
            std::snprintf(path, sizeof(path), "%03u.png", ordinal);
            if (!Capture::SaveFrame(window, TestOutput::Path(std::string("ui-movies/") + path))) { return 1; }
            if (++ordinal >= 148) {
                std::printf("[movie-render] movies=148 failures=%u\n", renderer.Failures());
                return renderer.Failures() != 0;
            }
        } else if (!screenshotPath.empty()) {
            if (!Capture::SaveFrame(window, screenshotPath)) { return 1; }
            return renderer.Failures() != 0;
        }
        window.Present();
    }
    return 0;
}
