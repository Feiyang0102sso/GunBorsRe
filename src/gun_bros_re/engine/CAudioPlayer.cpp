/**
 * @file CAudioPlayer.cpp
 * @brief SDL3 implementation of one-shot WAV playback.
 */

#include "engine/CAudioPlayer.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <map>

namespace {

constexpr std::size_t kMaximumPlayingSounds = 32;

struct DecodedSound {
    SDL_AudioSpec specification;
    std::vector<std::uint8_t> samples;
};

struct PlayingSound {
    SDL_AudioStream *stream;
    std::uint64_t finishMs;
    std::uint64_t key;
    bool loop;
};

}  // namespace

struct CAudioPlayer::Impl {
    std::map<std::uint64_t, DecodedSound> sounds;
    std::vector<PlayingSound> playing;
    bool paused = false;
    std::uint64_t pauseStartMs = 0;
};

CAudioPlayer::CAudioPlayer() : m_impl(new Impl()) {}

CAudioPlayer::~CAudioPlayer() {
    for (std::size_t i = 0; i < m_impl->playing.size(); ++i) {
        SDL_DestroyAudioStream(m_impl->playing[i].stream);
    }
}

bool CAudioPlayer::Load(std::uint64_t key,
                        const std::vector<std::uint8_t> &wavBytes) {
    if (m_impl->sounds.find(key) != m_impl->sounds.end()) {
        return true;
    }
    if (wavBytes.empty()) {
        return false;
    }

    SDL_IOStream *input = SDL_IOFromConstMem(wavBytes.data(), wavBytes.size());
    if (input == nullptr) {
        std::printf("[audio] could not open WAV memory: %s\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec specification;
    Uint8 *samples = nullptr;
    Uint32 sampleBytes = 0;
    if (!SDL_LoadWAV_IO(input, true, &specification, &samples, &sampleBytes)) {
        std::printf("[audio] could not decode WAV: %s\n", SDL_GetError());
        return false;
    }

    DecodedSound sound;
    sound.specification = specification;
    sound.samples.assign(samples, samples + sampleBytes);
    SDL_free(samples);
    m_impl->sounds.insert(std::make_pair(key, sound));
    return true;
}

bool CAudioPlayer::Play(std::uint64_t key, bool loop) {
    const std::map<std::uint64_t, DecodedSound>::const_iterator found =
        m_impl->sounds.find(key);
    if (found == m_impl->sounds.end()) {
        return false;
    }

    Update();
    if (m_impl->playing.size() >= kMaximumPlayingSounds) {
        SDL_DestroyAudioStream(m_impl->playing.front().stream);
        m_impl->playing.erase(m_impl->playing.begin());
    }

    const DecodedSound &sound = found->second;
    SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &sound.specification, nullptr, nullptr);
    if (stream == nullptr) {
        std::printf("[audio] could not open playback stream: %s\n", SDL_GetError());
        return false;
    }
    if (!SDL_PutAudioStreamData(stream, sound.samples.data(),
                                static_cast<int>(sound.samples.size())) ||
        !SDL_FlushAudioStream(stream) || !SDL_ResumeAudioStreamDevice(stream)) {
        std::printf("[audio] could not queue sound: %s\n", SDL_GetError());
        SDL_DestroyAudioStream(stream);
        return false;
    }

    const std::uint32_t bytesPerFrame = SDL_AUDIO_FRAMESIZE(sound.specification);
    std::uint64_t durationMs = 0;
    if (bytesPerFrame > 0 && sound.specification.freq > 0) {
        const std::uint64_t frameCount = sound.samples.size() / bytesPerFrame;
        durationMs = frameCount * 1000u /
                     static_cast<std::uint64_t>(sound.specification.freq);
    }

    // The stream's input queue can empty before the device has played its
    // buffered tail. Keep it alive for the decoded duration plus a small
    // device-buffer allowance, otherwise short explosions are cut off.
    PlayingSound playing;
    playing.stream = stream;
    playing.finishMs = SDL_GetTicks() + durationMs + 150u;
    playing.key = key;
    playing.loop = loop;
    m_impl->playing.push_back(playing);
    if (m_impl->paused) { SDL_PauseAudioStreamDevice(stream); }
    return true;
}

void CAudioPlayer::Update() {
    if (m_impl->paused) { return; }
    const std::uint64_t nowMs = SDL_GetTicks();
    std::size_t index = 0;
    while (index < m_impl->playing.size()) {
        const PlayingSound &playing = m_impl->playing[index];
        if (playing.loop) {
            const DecodedSound &sound = m_impl->sounds.at(playing.key);
            if (SDL_GetAudioStreamQueued(playing.stream) < static_cast<int>(sound.samples.size())) {
                SDL_PutAudioStreamData(playing.stream, sound.samples.data(), static_cast<int>(sound.samples.size()));
            }
            ++index;
            continue;
        }
        if (nowMs < playing.finishMs) {
            index++;
            continue;
        }

        SDL_DestroyAudioStream(playing.stream);
        m_impl->playing.erase(m_impl->playing.begin() + index);
    }
}

void CAudioPlayer::Stop(std::uint64_t key) {
    std::size_t index = 0;
    while (index < m_impl->playing.size()) {
        if (m_impl->playing[index].key == key) {
            SDL_DestroyAudioStream(m_impl->playing[index].stream);
            m_impl->playing.erase(m_impl->playing.begin() + index);
        } else { ++index; }
    }
}

void CAudioPlayer::StopAll() {
    for (const PlayingSound &sound : m_impl->playing) { SDL_DestroyAudioStream(sound.stream); }
    m_impl->playing.clear();
}

void CAudioPlayer::SetPaused(bool paused) {
    if (m_impl->paused == paused) { return; }
    const std::uint64_t nowMs = SDL_GetTicks();
    if (paused) { m_impl->pauseStartMs = nowMs; }
    for (PlayingSound &sound : m_impl->playing) {
        if (paused) { SDL_PauseAudioStreamDevice(sound.stream); }
        else {
            sound.finishMs += nowMs - m_impl->pauseStartMs;
            SDL_ResumeAudioStreamDevice(sound.stream);
        }
    }
    m_impl->paused = paused;
}
