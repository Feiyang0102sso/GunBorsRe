#include "gun_bros_re/debug/FrameRateOverlay.h"
#include "gun_bros_re/debug/DebugConfig.h"
#include "engine/core/Paths.h"
#include "engine/core/CMatrix4d.h"
#include "engine/resources/CResTOCManager.h"
#include <cstdio>

bool SetDebugFPS(CWindow &window, bool enabled, const std::string &bigDirectory) {
    if (!enabled) { window.SetPresentationOverlay(nullptr); return true; }
    if (window.HasPresentationOverlay()) { return true; }
    std::string directory = bigDirectory;
    if (directory.empty()) { directory = (Paths::Root() / Paths::BigDirectory).string(); }
    auto overlay = std::make_unique<FrameRateOverlay>();
    if (!overlay->Init(directory)) { return false; }
    window.SetPresentationOverlay(std::move(overlay));
    return true;
}

bool FrameRateOverlay::Init(const std::string &bigDirectory) {
    // CFontMgr::GetFont :56753 / bitmap_font.bt: font and atlas are resolved through FONT_KEYSET.
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return false; }
    return m_font.Init(*toc.GetPack(toc.GetCorePackIndex()), DebugConfig::FPS.font) &&
        m_program.Load(Paths::Shaders(), "ogles_vs_mvp_tex0", "ogles_ps_tex0") && m_batch.Create(m_program);
}

void FrameRateOverlay::Tick(std::uint64_t now) {
    if (m_started == 0) { m_started = now; return; }
    ++m_frames;
    const auto elapsed = now - m_started;
    if (elapsed < DebugConfig::FpsSampleMs) { return; }
    m_fps = m_frames * 1000.0f / elapsed;
    m_frames = 0;
    m_started = now;
}

void FrameRateOverlay::Draw(int width, int height) {
    // Present is shared by video, loading, menus and gameplay. Restore every
    // state touched here because the next scene may reuse the same GL context.
    GLint viewport[4], program = 0, vao = 0, buffer = 0, activeTexture = 0, texture = 0;
    GLint blendSource = 0, blendDestination = 0;
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    glGetIntegerv(GL_BLEND_SRC, &blendSource);
    glGetIntegerv(GL_BLEND_DST, &blendDestination);
    const bool depth = glIsEnabled(GL_DEPTH_TEST) != 0;
    const bool scissor = glIsEnabled(GL_SCISSOR_TEST) != 0;
    const bool blend = glIsEnabled(GL_BLEND) != 0;
    const bool cull = glIsEnabled(GL_CULL_FACE) != 0;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glViewport(0, 0, width, height);
    float projection[16];
    Matrix4dOrthoTopLeft(DebugConfig::CanvasWidth, DebugConfig::CanvasHeight, 1, projection);
    char label[32];
    if (m_fps > 0) { std::snprintf(label, sizeof(label), DebugConfig::Text::Fps, m_fps); }
    else { std::snprintf(label, sizeof(label), "%s", DebugConfig::Text::FpsPending); }
    const auto &style = DebugConfig::FPS;
    m_batch.Begin();
    m_font.Draw(m_batch, label, style.x, style.y, style.scale, style.alpha);
    m_batch.Upload();
    m_batch.Draw(m_program, projection);
    glUseProgram(program);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBindTexture(GL_TEXTURE_2D, texture);
    glActiveTexture(activeTexture);
    glBlendFunc(blendSource, blendDestination);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    if (depth) { glEnable(GL_DEPTH_TEST); }
    if (scissor) { glEnable(GL_SCISSOR_TEST); }
    if (!blend) { glDisable(GL_BLEND); }
    if (cull) { glEnable(GL_CULL_FACE); }
}
