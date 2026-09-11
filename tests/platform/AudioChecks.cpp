#include "engine/platform/CAudioPlayerInternal.h"
#include <chrono>
unsigned CAudioPlayer::CheckSilentPlayback(std::uint64_t first, std::uint64_t second) {
    // Tests the real device/stream path without making the user's speakers play.
    StopAll();
    const float volume = m_impl->volume;
    SetVolume(0);
    m_impl->silentDeviceValidation = true;
    const unsigned devicesBefore = m_impl->devicesOpened;
    const unsigned streamsBefore = m_impl->streamsCreated;
    const auto start = std::chrono::steady_clock::now();
    unsigned failures = 0;
    for (unsigned round = 0; round < 8; ++round) {
        if (!Play(first) || !Play(second)) { ++failures; }
        SetPaused(true);
        SetPaused(false);
        StopAll();
    }
    const unsigned devices = m_impl->devicesOpened - devicesBefore;
    const unsigned streams = m_impl->streamsCreated - streamsBefore;
    if (devices > 1 || streams > 2) { ++failures; }
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::printf("[audio-backend-check] plays=16 devices=%u streams=%u elapsed-ms=%.3f failures=%u\n", devices, streams, elapsed, failures);
    const auto warmStart = std::chrono::steady_clock::now();
    for (unsigned round = 0; round < 100; ++round) {
        if (!Play(first) || !Play(second)) { ++failures; }
        StopAll();
    }
    const auto warmElapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - warmStart).count();
    if (m_impl->devicesOpened != devicesBefore + devices || m_impl->streamsCreated != streamsBefore + streams) { ++failures; }
    if (!Play(first, true, 101) || !Play(second, true, 102)) { ++failures; }
    StopOwner(101);
    if (m_impl->playing.size() != 1 || m_impl->playing[0].owner != 102) { ++failures; }
    SetPaused(true);
    if (!SDL_AudioDevicePaused(m_impl->device)) { ++failures; }
    Update();
    if (m_impl->playing.size() != 1) { ++failures; }
    SetPaused(false);
    Stop(second);
    if (!m_impl->playing.empty()) { ++failures; }
    std::printf("[audio-backend-check] warm-plays=200 elapsed-ms=%.3f owner-loop-pause failures=%u\n", warmElapsed, failures);
    m_impl->silentDeviceValidation = false;
    SetVolume(volume);
    return failures;
}