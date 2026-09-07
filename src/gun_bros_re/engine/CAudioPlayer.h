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

    /** Decode and cache a RIFF/WAVE resource under a caller-owned key. */
    bool Load(std::uint64_t key, const std::vector<std::uint8_t> &wavBytes);

    /** Start one cached sound. Several instances may overlap. */
    void Play(std::uint64_t key);

    /** Release device streams whose queued samples have finished. */
    void Update();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif  // GUN_BROS_RE_ENGINE_CAUDIOPLAYER_H
