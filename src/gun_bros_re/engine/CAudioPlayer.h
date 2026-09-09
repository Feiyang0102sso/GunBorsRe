/**
 * @file CAudioPlayer.h
 * @brief Small SDL audio owner for one-shot sounds decoded from BIG resources.
 */

#ifndef GUN_BROS_RE_ENGINE_CAUDIOPLAYER_H
#define GUN_BROS_RE_ENGINE_CAUDIOPLAYER_H

#include <cstdint>
#include <memory>
#include <vector>

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
    /** Music bypasses the effects switch, and has its own original 0.3 gain. */
    void SetMusicChannel(bool music);
    void SetVolume(float volume);

    /** Decode and cache a RIFF/WAVE resource under a caller-owned key. */
    bool Load(std::uint64_t key, const std::vector<std::uint8_t> &wavBytes);
    bool HasSound(std::uint64_t key) const;
    /** Research-only: exercise real SDL streams at zero gain, even with --mute. */
    unsigned CheckSilentPlayback(std::uint64_t first, std::uint64_t second);
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
