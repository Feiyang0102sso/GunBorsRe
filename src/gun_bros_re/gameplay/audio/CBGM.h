/** @file CBGM.h
 * @brief Original menu track and rotating battle tracks, with Windows decoding.
 */
#ifndef GUN_BROS_RE_CBGM_H
#define GUN_BROS_RE_CBGM_H
#include "engine/platform/ZAudioPlayer.h"
#include <filesystem>

class CBGM {
public:
    int GetTrack() const { return m_track; }
    /** Playback starts across instances, for scene handoff regression evidence. */
    static unsigned GetPlaybackStarts();
    void EnableSilentValidation() { m_audio.EnableSilentValidation(); }
    ZAudioPlaybackState GetPlaybackState() const { return m_audio.GetPlaybackState(); }
    bool Play(unsigned track, bool loop = true);
    bool NextTrack();
    void Update();
    void SetPaused(bool paused);
    void Stop();
    void SetEnabled(bool enabled);
    /** Original CBGM volume scale; OnSuspend uses 0.5, OnResume uses 1.0. */
    void SetVolume(float scale);
    static const char *TrackName(unsigned track);
private:
    ZAudioPlayer m_audio;
    int m_track = -1;
    bool m_enabled = true;
    float m_volumeScale = 1.0f;
};
#endif
