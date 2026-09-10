/** @file CBGM.cpp
 * @brief CBGM::Play / NextTrack follow iOS :60102 / :60204.
 */
#include "gun_bros/CBGM.h"
#include "engine/platform/CMediaDecoder.h"
#include <cstdio>

namespace {
constexpr const char *kTracks[] = {"game_0.mp3", "1.mp3", "2.mp3", "3.mp3", "4.mp3", "5.mp3", "6.mp3"};
unsigned g_lastBattleTrack = 0;
unsigned g_playbackStarts = 0;
}

unsigned CBGM::GetPlaybackStarts() { return g_playbackStarts; }

const char *CBGM::TrackName(unsigned track) {
    if (track >= 7) { return nullptr; }
    return kTracks[track];
}

bool CBGM::Play(unsigned track, bool loop) {
    if (track >= 7) { return false; }
    if (m_track == static_cast<int>(track)) { return true; }
    // Decode while the previous track is still queued; reuse PCM on revisits.
    if (!m_audio.HasSound(track)) {
        MediaAudio decoded;
        if (!DecodeMediaAudio(std::filesystem::path(ASSET_ROOT) / "mp3" / kTracks[track], decoded) ||
            !m_audio.LoadPcm(track, decoded.samples, decoded.sampleRate, decoded.channels)) { return false; }
    }
    Stop();
    m_audio.SetMusicChannel(true);
    SetEnabled(m_enabled);
    if (!m_audio.Play(track, loop)) { return false; }
    m_track = static_cast<int>(track);
    ++g_playbackStarts;
    std::printf("[bgm] track=%u file=%s loop=%d muted=%d\n", track, kTracks[track], loop, CAudioPlayer::IsMuted());
    return true;
}

bool CBGM::NextTrack() {
    ++g_lastBattleTrack;
    if (g_lastBattleTrack > 6) { g_lastBattleTrack = 1; }
    return Play(g_lastBattleTrack);
}

void CBGM::Update() { m_audio.Update(); }
void CBGM::SetEnabled(bool enabled) {
    m_enabled = enabled;
    float volume = 0;
    if (enabled) { volume = 0.3f * m_volumeScale; }
    m_audio.SetVolume(volume);
}
void CBGM::SetVolume(float scale) {
    // CBGM::SetVolume :59971 applies the music bus gain after the scene scale.
    m_volumeScale = scale;
    SetEnabled(m_enabled);
}
void CBGM::SetPaused(bool paused) { m_audio.SetPaused(paused); }
void CBGM::Stop() { m_audio.StopAll(); m_track = -1; }
