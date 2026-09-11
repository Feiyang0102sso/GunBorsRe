/**
 * @file CAudioPlayer.h
 * @brief Small SDL audio owner for one-shot sounds decoded from BIG resources.
 */

#ifndef GUN_BROS_RE_ENGINE_CAUDIOPLAYER_H
#define GUN_BROS_RE_ENGINE_CAUDIOPLAYER_H

#include <cstdint>
#include <memory>
#include <vector>

/** Read-only evidence for audio lifecycle and queue regression checks. */
struct AudioPlaybackState {
    unsigned voices = 0, devicesOpened = 0, streamsCreated = 0;
    std::int64_t queuedBytes = 0;
    bool paused = false;
    float volume = 0;
};

/** Plays overlapping one-shot WAV sounds and caches their decoded samples. */
class CAudioPlayer {
public:
    CAudioPlayer();
    ~CAudioPlayer();

    CAudioPlayer(const CAudioPlayer &) = delete;
    CAudioPlayer &operator=(const CAudioPlayer &) = delete;

    /** Process-wide startup option: validate WAVs without opening playback streams. */
    static void SetMuted(bool muted);
    static bool IsMuted();
    static void SetEffectsEnabled(bool enabled);
    /**
     * Gain every new effects player starts at, 0..1.
     *
     * The original mixes one sound effect voice at
     * playerVolume x eventVolume x 0.001 (CSoundEvent_Cocoa::SetVolume
     * :303199), where CMediaPlayer::SetVolume :361532 clamps playerVolume to
     * ten and CSoundEventPCM's constructor :363616 leaves eventVolume at a
     * hundred -- so the scale is playerVolume/10, the same 0..10 dial the
     * music bus uses through CBGM::SetVolume :59971 (x0.3). Music is set from
     * the original value; this host has not recovered the effects one, so it
     * is a configurable setting rather than a guess baked into the mix.
     */
    static void SetEffectsGain(float gain);
    static float GetEffectsGain();
    /** Music bypasses the effects switch, and has its own original 0.3 gain. */
    void SetMusicChannel(bool music);
    void SetVolume(float volume);

    /** Decode and cache a RIFF/WAVE resource under a caller-owned key. */
    bool Load(std::uint64_t key, const std::vector<std::uint8_t> &wavBytes);
    bool HasSound(std::uint64_t key) const;
    /** How many voices are sounding right now; research evidence for mixing. */
    unsigned GetVoiceCount() const;
    /** Decoded length of a cached sound, 0 when it is not loaded. */
    unsigned GetDurationMs(std::uint64_t key) const;
    /** Research-only: exercise real SDL streams at zero gain, even with --mute. */

#if GB_ENABLE_TESTS
    unsigned CheckSilentPlayback(std::uint64_t first, std::uint64_t second);
#endif

    /** Isolated regression players only; force zero gain for all future plays. */
    void EnableSilentValidation();
    AudioPlaybackState GetPlaybackState() const;
    /** Cache decoded signed little-endian 16-bit PCM from the media decoder. */
    bool LoadPcm(std::uint64_t key, const std::vector<std::uint8_t> &samples, unsigned sampleRate, unsigned channels);

    /** Start one cached sound. Return whether playback was successfully queued. */
    bool Play(std::uint64_t key, bool loop = false, std::uint64_t owner = 0);
    void StopOwner(std::uint64_t owner);
    void Stop(std::uint64_t key);
    void StopAll();
    void SetPaused(bool paused);

    /** Release device streams whose queued samples have finished. */
    void Update();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif  // GUN_BROS_RE_ENGINE_CAUDIOPLAYER_H
