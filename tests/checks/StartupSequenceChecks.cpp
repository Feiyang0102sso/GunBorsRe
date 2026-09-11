#include "engine/core/Paths.h"
/** @file StartupSequence.cpp
 * @brief Read original intro video/audio directly; never substitute a still logo.
 */
#define NOMINMAX
#include "TestOutput.h"
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
#include "Checks.h"

int RunMediaCheck() {
    unsigned failures = 0;
    for (unsigned track = 0; track < 7; ++track) {
        MediaAudio audio;
        if (!DecodeMediaAudio(Paths::Root() / Paths::AudioDirectory / CBGM::TrackName(track), audio)) { ++failures; continue; }
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
        if (frames == 61 && !PNGEncode(image, TestOutput::Path("glu-logo-frame60.png"))) { ++failures; }
    }
    // Independent ffprobe baseline: H.264 480x320, 30 fps, 4.166667 seconds.
    if (frames != 125 || video.Width() != 480 || video.Height() != 320 || timestamp < 4100) { ++failures; }
    std::printf("[media-check] tracks=7 video-frames=%u final-ms=%llu failures=%u\n", frames, timestamp, failures);
    return failures != 0;
}