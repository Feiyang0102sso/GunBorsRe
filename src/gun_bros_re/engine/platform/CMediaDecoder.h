/** @file CMediaDecoder.h
 * @brief Windows decoder for the original loose M4V and MP3 assets.
 */
#ifndef GUN_BROS_RE_CMEDIADECODER_H
#define GUN_BROS_RE_CMEDIADECODER_H
#include "engine/CPNG.h"
#include <filesystem>
#include <memory>

struct MediaAudio {
    unsigned sampleRate = 0;
    unsigned channels = 0;
    std::vector<std::uint8_t> samples;
};

/** Decode signed 16-bit PCM without opening an audio device, also when muted. */
bool DecodeMediaAudio(const std::filesystem::path &path, MediaAudio &audio);

/** One sequential video reader; timestamps use milliseconds from the file. */
class CMediaVideo {
public:
    CMediaVideo();
    ~CMediaVideo();
    bool Open(const std::filesystem::path &path);
    bool ReadFrame(PNGImage &image, std::uint64_t &timestampMs, bool &ended);
    unsigned Width() const;
    unsigned Height() const;
    std::uint64_t DurationMs() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
#endif
