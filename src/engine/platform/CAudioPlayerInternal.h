#pragma once
#include "engine/platform/CAudioPlayer.h"
#include <SDL3/SDL.h>
#include <map>
#include <cstdio>
namespace AudioDetail {
struct DecodedSound {
    SDL_AudioSpec specification;
    std::vector<std::uint8_t> samples;
};

struct PlayingSound {
    SDL_AudioStream *stream;
    std::uint64_t finishMs;
    std::uint64_t key;
    bool loop;
    std::uint64_t owner;
};

}
using namespace AudioDetail;
struct CAudioPlayer::Impl {
    std::map<std::uint64_t, DecodedSound> sounds;
    std::vector<PlayingSound> playing;
    std::vector<SDL_AudioStream *> idleStreams;
    SDL_AudioDeviceID device = 0;
    bool paused = false;
    std::uint64_t pauseStartMs = 0;
    float volume = CAudioPlayer::GetEffectsGain();
    bool musicChannel = false;
    bool silentDeviceValidation = false;
    unsigned devicesOpened = 0;
    unsigned streamsCreated = 0;

    // Host counterpart of Cocoa's reusable OpenAL source pool. SDL streams
    // remain bound to one device; bursts do not open a device per enemy.
    void Recycle(SDL_AudioStream *stream) {
        SDL_ClearAudioStream(stream);
        idleStreams.push_back(stream);
    }

    SDL_AudioStream *Acquire(const SDL_AudioSpec &specification) {
        if (device == 0) {
            device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
            if (device == 0) {
                std::printf("[audio] could not open device: %s\n", SDL_GetError());
                return nullptr;
            }
            ++devicesOpened;
            if (paused) { SDL_PauseAudioDevice(device); }
        }
        SDL_AudioStream *stream = nullptr;
        if (!idleStreams.empty()) {
            stream = idleStreams.back();
            idleStreams.pop_back();
            if (!SDL_SetAudioStreamFormat(stream, &specification, nullptr)) {
                std::printf("[audio] could not reuse stream: %s\n", SDL_GetError());
                SDL_DestroyAudioStream(stream);
                return nullptr;
            }
        } else {
            SDL_AudioSpec output{};
            if (!SDL_GetAudioDeviceFormat(device, &output, nullptr)) { return nullptr; }
            stream = SDL_CreateAudioStream(&specification, &output);
            if (stream == nullptr) { return nullptr; }
            if (!SDL_BindAudioStream(device, stream)) {
                SDL_DestroyAudioStream(stream);
                return nullptr;
            }
            ++streamsCreated;
        }
        return stream;
    }
};