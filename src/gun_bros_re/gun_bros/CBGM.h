/** @file CBGM.h
 * @brief Original menu track and rotating battle tracks, with Windows decoding.
 */
#ifndef GUN_BROS_RE_CBGM_H
#define GUN_BROS_RE_CBGM_H
#include "engine/CAudioPlayer.h"
#include <filesystem>

class CBGM {
public:
    bool Play(unsigned track, bool loop = true);
    bool NextTrack();
    void Update();
    void SetPaused(bool paused);
    void Stop();
    void SetEnabled(bool enabled);
    static const char *TrackName(unsigned track);
private:
    CAudioPlayer m_audio;
    int m_track = -1;
};
#endif
