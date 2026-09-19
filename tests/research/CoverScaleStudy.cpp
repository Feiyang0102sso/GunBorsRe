#include "gun_bros_viewer/scenes/ZMapViewer.h"
/** Compare one real cover transition at fixed time under historical dock widths. */
#include "TestOutput.h"
#include "gun_bros_re/gameplay/map/CMapInternal.h"
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
    CMap loaded;
    std::size_t selected = 0;
    bool found = false;
    for (const ZCatalogMap &entry : BuildCatalog(toc)) {
        CMap candidate;
        if (!LoadPreviewMap(toc, entry.packIndex, entry.mapIndex, candidate)) { return 1; }
        candidate.LoadProps(toc);
        for (std::size_t index = 0; index < candidate.GetResources().props.size(); ++index) {
            if (ResearchPropKind(candidate.GetResources().props[index]) == ZInteractivePropKind::Cover) {
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
    const float coverX = loaded.GetResources().props[selected].x;
    const float coverY = loaded.GetResources().props[selected].y;
    CProp cover = std::move(loaded.GetResources().props[selected]);
    loaded.GetResources().props.clear();
    loaded.GetResources().props.push_back(std::move(cover));
    loaded.GetResources().props[0].BindResources();
    // State 1 and a fixed random salt exercise the same original transition twice.
    SetCoverState(loaded, static_cast<ZCoverState>(1));
    DispatchPreviewActions(toc, loaded);
    for (unsigned time = 0; time < 400; time += 16) { AdvanceParticleEffects(loaded, 16); }
    std::size_t particles = 0;
    float worldRadius = 0;
    for (const auto &effect : loaded.GetResources().activeParticleEffects) {
        for (std::size_t index = 0; index < effect.player.GetParticleCount(); ++index) {
            const auto &particle = effect.player.GetParticle(index);
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
        CCamera::Viewport camera = FitCamera(loaded, width - dockWidths[index], height);
        // Centre the same prop in every capture; only projection scale changes.
        camera.x = coverX - width / camera.zoom * 0.5f;
        camera.y = coverY - height / camera.zoom * 0.5f;
        float mvp[kMatrix4dElements];
        Matrix4dOrthoTopLeft(width / camera.zoom, height / camera.zoom, kMapDepthRange, mvp);
        Matrix4dTranslate(mvp, -camera.x, -camera.y);
        glViewport(0, 0, width, height);
        glClearColor(0.08f, 0.08f, 0.10f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        batch.Begin();
        loaded.GetResources().props[0].DrawSlot(batch, 1);
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
