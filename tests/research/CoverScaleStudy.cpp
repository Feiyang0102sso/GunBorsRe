/** Compare one real cover transition at fixed time under historical dock widths. */
#include "TestOutput.h"
#include "gun_bros_re/gameplay/ZMapWorldInternal.h"
#include "gun_bros_re/debug/Capture.h"
#include "tests/TestOutput.h"
using namespace MapDetail;

int RunCoverScaleStudy(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.InitAuto(bigDirectory) || !toc.Bind()) { return 1; }
    ZWindow window;
    if (!window.Open("Cover scale comparison", 1440, 900)) { return 1; }
    ZShaderProgram program;
    if (!program.Load(Paths::Shaders(), "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return 1; }
    ZQuadBatch batch;
    if (!batch.Create(program)) { return 1; }
    ZLoadedMap loaded;
    std::size_t selected = 0;
    bool found = false;
    for (const ZCatalogMap &entry : BuildCatalog(toc)) {
        ZLoadedMap candidate;
        if (!LoadMap(toc, entry.packIndex, entry.mapIndex, candidate)) { return 1; }
        LoadProps(toc, candidate);
        for (std::size_t index = 0; index < candidate.props.size(); ++index) {
            if (candidate.props[index].sprite->interactiveKind == ZInteractivePropKind::Cover) {
                selected = index;
                found = true;
                break;
            }
        }
        if (!found) { continue; }
        loaded = std::move(candidate);
        std::printf("[cover-scale] selected %s map %u prop=%zu\n", entry.packName.c_str(), entry.mapIndex, selected);
        break;
    }
    if (!found) { return 1; }
    const ZPlacedProp cover = loaded.props[selected];
    loaded.props.clear();
    loaded.props.push_back(cover);
    // State 1 and a fixed random salt exercise the same original transition twice.
    SetCoverState(loaded, static_cast<ZCoverState>(1));
    StartTransitionParticles(toc, loaded, ZInteractivePropKind::Cover, 1);
    for (unsigned time = 0; time < 400; time += 16) { AdvanceParticleEffects(loaded, 16); }
    std::size_t particles = 0;
    float worldRadius = 0;
    for (const auto &effect : loaded.activeParticleEffects) {
        for (const auto &particle : effect.particles) {
            ++particles;
            const float dx = particle.x - effect.x;
            const float dy = particle.y - effect.y;
            worldRadius = std::max(worldRadius, std::sqrt(dx * dx + dy * dy));
        }
    }
    if (particles == 0 || worldRadius <= 0) { return 1; }
    int width = 0, height = 0;
    window.GetDrawableSize(width, height);
    const int dockWidths[] = {0, 340, 480};
    const char *names[] = {"no-dock", "old-dock", "large-text-dock"};
    float previousZoom = 0;
    for (unsigned index = 0; index < 3; ++index) {
        ZMapCamera camera = FitCamera(loaded, width - dockWidths[index], height);
        // Centre the same prop in every capture; only projection scale changes.
        camera.x = cover.x - width / camera.zoom * 0.5f;
        camera.y = cover.y - height / camera.zoom * 0.5f;
        float mvp[kMatrix4dElements];
        Matrix4dOrthoTopLeft(width / camera.zoom, height / camera.zoom, kMapDepthRange, mvp);
        Matrix4dTranslate(mvp, -camera.x, -camera.y);
        glViewport(0, 0, width, height);
        glClearColor(0.08f, 0.08f, 0.10f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        batch.Begin();
        AddSpriteQuads(loaded.props[0], CurrentQuads(*MainSlotFor(loaded.props[0]), loaded.props[0].main), batch);
        AddParticleQuads(loaded, batch, -1000, 1000);
        batch.Upload();
        batch.Draw(program, mvp);
        if (!Capture::SaveFrame(window, TestOutput::Path(std::string(names[index]) + ".png"))) { return 1; }
        std::printf("[cover-scale] %s viewport=%dx%d zoom=%.6f particles=%zu world-radius=%.3f screen-radius=%.3f\n",
            names[index], width - dockWidths[index], height, camera.zoom, particles, worldRadius, worldRadius * camera.zoom);
        if (index == 2) {
            const float ratio = camera.zoom / previousZoom;
            std::printf("[cover-scale] large-text/old screen ratio=%.6f; same world particle state\n", ratio);
            if (ratio >= 1) { return 1; }
        }
        previousZoom = camera.zoom;
    }
    return 0;
}
