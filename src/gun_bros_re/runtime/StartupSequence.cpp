/** @file StartupSequence.cpp
 * @brief Read original intro video/audio directly; never substitute a still logo.
 */
#define NOMINMAX
#include "runtime/StartupSequence.h"
#include "engine/platform/CMediaDecoder.h"
#include "engine/platform/CWindow.h"
#include "engine/CQuadBatch.h"
#include "engine/CMatrix4d.h"
#include "engine/CAudioPlayer.h"
#include "gun_bros/CBGM.h"
#include <algorithm>
#include <cstdio>
#include <fstream>

namespace {
const std::filesystem::path kLogoDirectory = std::filesystem::path(ASSET_ROOT) / "glu_logo";
}

bool LoadStartupSplash(CTexture &texture) {
    std::ifstream file(std::filesystem::path(ASSET_ROOT) / "png/Default-Landscape.png", std::ios::binary);
    if (!file) { return false; }
    std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(file), {});
    PNGImage decoded;
    return PNGDecode(bytes, decoded) && texture.Create(decoded);
}

int RunMediaCheck() {
    unsigned failures = 0;
    for (unsigned track = 0; track < 7; ++track) {
        MediaAudio audio;
        if (!DecodeMediaAudio(std::filesystem::path(ASSET_ROOT) / "mp3" / CBGM::TrackName(track), audio)) { ++failures; continue; }
        bool nonzero = false;
        for (std::uint8_t sample : audio.samples) { if (sample != 0) { nonzero = true; break; } }
        if (!nonzero || audio.samples.size() < audio.sampleRate * audio.channels * 2) { ++failures; }
    }
    MediaAudio logoAudio;
    if (!DecodeMediaAudio(kLogoDirectory / "glu_logo_audio.wav", logoAudio)) { ++failures; }
    CMediaVideo video;
    if (!video.Open(kLogoDirectory / "glu_logo_landscape.m4v")) { return 1; }
    unsigned frames = 0;
    bool ended = false;
    std::uint64_t timestamp = 0;
    std::uint64_t previousTimestamp = 0;
    while (!ended) {
        PNGImage image;
        if (!video.ReadFrame(image, timestamp, ended)) { return 1; }
        if (ended) { break; }
        if (timestamp < previousTimestamp || image.pixels.size() != image.width * image.height * 4) { ++failures; }
        previousTimestamp = timestamp;
        ++frames;
        if (frames == 61 && !PNGEncode(image, "out/glu-logo-frame60.png")) { ++failures; }
    }
    // Independent ffprobe baseline: H.264 480x320, 30 fps, 4.166667 seconds.
    if (frames != 125 || video.Width() != 480 || video.Height() != 320 || timestamp < 4100) { ++failures; }
    std::printf("[media-check] tracks=7 video-frames=%u final-ms=%llu failures=%u\n", frames, timestamp, failures);
    return failures != 0;
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
    if (!program.Load(ASSET_ROOT "/src/gun_bros_re/shaders", "ogles_vs_mvp_tex0", "ogles_ps_tex0") || !batch.Create(program)) { return 1; }
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
        if (!screenshotPath.empty() && !window.SaveFrame(screenshotPath)) { return 1; }
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
        if (!screenshotPath.empty()) { return !window.SaveFrame(screenshotPath); }
        window.Present();
    }
    return 2; // Closing the intro closes the game, rather than opening another window.
}
