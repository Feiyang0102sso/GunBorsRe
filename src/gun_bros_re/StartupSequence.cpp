#include "engine/core/Paths.h"
/** @file StartupSequence.cpp
 * @brief Read original intro video/audio directly; never substitute a still logo.
 */
#define NOMINMAX
#include "gun_bros_re/StartupSequence.h"
#include "engine/platform/CMediaDecoder.h"
#include "engine/platform/CWindow.h"
#include "engine/graphics/CQuadBatch.h"
#include "engine/core/CMatrix4d.h"
#include "engine/platform/CAudioPlayer.h"
#include "gun_bros_re/gameplay/CBGM.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include "gun_bros_re/StartupSequenceInternal.h"
using namespace StartupSequenceDetail;

bool LoadStartupSplash(CTexture &texture) {
    std::ifstream file(Paths::Root() / Paths::StartupDirectory / "Default-Landscape.png", std::ios::binary);
    if (!file) { return false; }
    std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(file), {});
    PNGImage decoded;
    return PNGDecode(bytes, decoded) && texture.Create(decoded);
}

int RunStartupSequence(const std::string &screenshotPath, unsigned advanceMs, CWindow *sharedWindow) {
    CWindow ownedWindow;
    CWindow &window = sharedWindow ? *sharedWindow : ownedWindow;
    if (!window.Open("Gun Bros", kDefaultWindowWidth, kDefaultWindowHeight)) { return 1; }
    CMediaVideo video;
    MediaAudio decoded;
    CAudioPlayer audio;
    if (!video.Open(kLogoDirectory / "glu_logo_landscape.m4v") ||
        !DecodeMediaAudio(kLogoDirectory / "glu_logo_audio.wav", decoded) ||
        !audio.LoadPcm(0, decoded.samples, decoded.sampleRate, decoded.channels)) { return 1; }
    CShaderProgram program;
    CQuadBatch batch;
    CTexture texture, splash;
    if (!LoadStartupSplash(splash)) { return 1; }
    if (!program.Load(Paths::Shaders().c_str(), "ogles_vs_mvp_tex0", "ogles_ps_tex0") || !batch.Create(program)) { return 1; }
    // Preload before the video starts. Its last frame hands directly to the
    // original launch image; BIG loading later adds the corner SpritePlayer.
    const auto finishIntro = [&]() {
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        glViewport(0, 0, width, height);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        float projection[16];
        Matrix4dOrthoTopLeft(1024, 768, 1, projection);
        const SourceRect source{0, 0, std::uint16_t(splash.GetWidth()), std::uint16_t(splash.GetHeight())};
        batch.Begin();
        batch.AddQuad(splash, 0, 0, 1024, 768, source, false, false, BlendMode::Alpha);
        batch.Upload();
        batch.Draw(program, projection);
        if (!screenshotPath.empty() && !GB_SAVE_FRAME(window, screenshotPath)) { return 1; }
        window.Present();
        std::printf("[startup] launch-image presented; no CG\n");
        return 0;
    };
    PNGImage pending;
    std::uint64_t pendingTime = 0;
    bool ended = false;
    if (!video.ReadFrame(pending, pendingTime, ended) || ended) { return 1; }
    if (!audio.Play(0)) { return 1; }
    const std::uint64_t start = window.GetTicksMs();
    while (window.PumpEvents()) {
        const std::uint64_t elapsed = window.GetTicksMs() - start + advanceMs;
        for (KeyCode key = window.TakeKeyPress(); key != KeyCode::None; key = window.TakeKeyPress()) {
            if (key == KeyCode::Space) { std::printf("[startup] intro skipped\n"); return finishIntro(); }
        }
        if (window.IsLeftMouseDown()) { std::printf("[startup] intro skipped\n"); return finishIntro(); }
        while (!ended && pendingTime <= elapsed) {
            if (!texture.Create(pending) || !video.ReadFrame(pending, pendingTime, ended)) { return 1; }
        }
        int width = 0, height = 0;
        window.GetDrawableSize(width, height);
        glViewport(0, 0, width, height);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        if (texture.IsValid()) {
            const float scale = std::min(static_cast<float>(width) / video.Width(), static_cast<float>(height) / video.Height());
            const float drawWidth = video.Width() * scale;
            const float drawHeight = video.Height() * scale;
            float projection[16];
            Matrix4dOrthoTopLeft(static_cast<float>(width), static_cast<float>(height), 1, projection);
            const SourceRect source{0, 0, static_cast<std::uint16_t>(video.Width()), static_cast<std::uint16_t>(video.Height())};
            batch.Begin();
            batch.AddQuad(texture, (width - drawWidth) * 0.5f, (height - drawHeight) * 0.5f, drawWidth, drawHeight, source, false, false, BlendMode::Alpha);
            batch.Upload();
            batch.Draw(program, projection);
        }
        audio.Update();
        if (ended && elapsed >= video.DurationMs()) { std::printf("[startup] intro completed\n"); return finishIntro(); }
        if (!screenshotPath.empty()) { return !GB_SAVE_FRAME(window, screenshotPath); }
        window.Present();
    }
    return 2; // Closing the intro closes the game, rather than opening another window.
}