#include "engine/platform/CAudioPlayerInternal.h"
/**
 * @file CAudioPlayer.cpp
 * @brief SDL3 implementation of one-shot WAV playback.
 */

#include "engine/platform/CAudioPlayer.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <map>
#include <chrono>

namespace {

constexpr std::size_t kMaximumPlayingSounds = 32;
bool g_muted = false;
bool g_effectsEnabled = true;
float g_effectsGain = 0.3f;

}  // namespace

CAudioPlayer::CAudioPlayer() : m_impl(new Impl()) {}

void CAudioPlayer::SetMuted(bool muted) {
    g_muted = muted;
}

bool CAudioPlayer::IsMuted() {
    return g_muted;
}

void CAudioPlayer::SetEffectsEnabled(bool enabled) { g_effectsEnabled = enabled; }

void CAudioPlayer::SetEffectsGain(float gain) {
    if (gain < 0) { gain = 0; }
    if (gain > 1) { gain = 1; }
    g_effectsGain = gain;
}

float CAudioPlayer::GetEffectsGain() { return g_effectsGain; }
void CAudioPlayer::SetMusicChannel(bool music) { m_impl->musicChannel = music; }
void CAudioPlayer::SetVolume(float volume) {
    if (volume < 0) { volume = 0; }
    if (volume > 1) { volume = 1; }
    m_impl->volume = volume;
    if (m_impl->silentDeviceValidation) { volume = 0; }
    for (PlayingSound &sound : m_impl->playing) { SDL_SetAudioStreamGain(sound.stream, volume); }
}

CAudioPlayer::~CAudioPlayer() {
    for (std::size_t i = 0; i < m_impl->playing.size(); ++i) {
        SDL_DestroyAudioStream(m_impl->playing[i].stream);
    }
    for (SDL_AudioStream *stream : m_impl->idleStreams) { SDL_DestroyAudioStream(stream); }
    if (m_impl->device != 0) { SDL_CloseAudioDevice(m_impl->device); }
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

bool CAudioPlayer::Play(std::uint64_t key, bool loop, std::uint64_t owner) {
    const std::map<std::uint64_t, DecodedSound>::const_iterator found =
        m_impl->sounds.find(key);
    if (found == m_impl->sounds.end()) {
        return false;
    }
    // Keep loading and key validation active during silent regression runs.
    if (!m_impl->silentDeviceValidation && (g_muted || (!m_impl->musicChannel && !g_effectsEnabled))) {
        return true;
    }

    Update();
    if (m_impl->playing.size() >= kMaximumPlayingSounds) {
        m_impl->Recycle(m_impl->playing.front().stream);
        m_impl->playing.erase(m_impl->playing.begin());
    }

    const DecodedSound &sound = found->second;
    SDL_AudioStream *stream = m_impl->Acquire(sound.specification);
    if (stream == nullptr) {
        std::printf("[audio] could not open playback stream: %s\n", SDL_GetError());
        return false;
    }
    float volume = m_impl->volume;
    if (m_impl->silentDeviceValidation) { volume = 0; }
    SDL_SetAudioStreamGain(stream, volume);
    if (!SDL_PutAudioStreamData(stream, sound.samples.data(),
                                static_cast<int>(sound.samples.size())) ||
        !SDL_FlushAudioStream(stream)) {
        std::printf("[audio] could not queue sound: %s\n", SDL_GetError());
        SDL_DestroyAudioStream(stream);
        return false;
    }
    if (!m_impl->paused) { SDL_ResumeAudioDevice(m_impl->device); }

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
    playing.owner = owner;
    m_impl->playing.push_back(playing);
    if (m_impl->paused) { SDL_PauseAudioDevice(m_impl->device); }
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

        m_impl->Recycle(playing.stream);
        m_impl->playing.erase(m_impl->playing.begin() + index);
    }
}

void CAudioPlayer::Stop(std::uint64_t key) {
    std::size_t index = 0;
    while (index < m_impl->playing.size()) {
        if (m_impl->playing[index].key == key) {
            m_impl->Recycle(m_impl->playing[index].stream);
            m_impl->playing.erase(m_impl->playing.begin() + index);
        } else { ++index; }
    }
}

void CAudioPlayer::StopAll() {
    for (const PlayingSound &sound : m_impl->playing) { m_impl->Recycle(sound.stream); }
    m_impl->playing.clear();
}

bool CAudioPlayer::HasSound(std::uint64_t key) const {
    return m_impl->sounds.find(key) != m_impl->sounds.end();
}

unsigned CAudioPlayer::GetVoiceCount() const {
    return static_cast<unsigned>(m_impl->playing.size());
}

void CAudioPlayer::EnableSilentValidation() {
    m_impl->silentDeviceValidation = true;
    SetVolume(m_impl->volume);
}

AudioPlaybackState CAudioPlayer::GetPlaybackState() const {
    AudioPlaybackState state;
    state.voices = GetVoiceCount();
    state.devicesOpened = m_impl->devicesOpened;
    state.streamsCreated = m_impl->streamsCreated;
    state.paused = m_impl->paused;
    state.volume = m_impl->volume;
    for (const auto &sound : m_impl->playing) { state.queuedBytes += SDL_GetAudioStreamQueued(sound.stream); }
    return state;
}

unsigned CAudioPlayer::GetDurationMs(std::uint64_t key) const {
    const auto found = m_impl->sounds.find(key);
    if (found == m_impl->sounds.end()) { return 0; }
    const DecodedSound &sound = found->second;
    const std::uint32_t bytesPerFrame = SDL_AUDIO_FRAMESIZE(sound.specification);
    if (bytesPerFrame == 0 || sound.specification.freq <= 0) { return 0; }
    const std::uint64_t frameCount = sound.samples.size() / bytesPerFrame;
    return static_cast<unsigned>(frameCount * 1000u / static_cast<std::uint64_t>(sound.specification.freq));
}

bool CAudioPlayer::LoadPcm(std::uint64_t key, const std::vector<std::uint8_t> &samples, unsigned sampleRate, unsigned channels) {
    if (samples.empty() || sampleRate == 0 || channels == 0 || samples.size() % (channels * 2) != 0) { return false; }
    DecodedSound sound;
    sound.specification = {};
    sound.specification.format = SDL_AUDIO_S16LE;
    sound.specification.freq = static_cast<int>(sampleRate);
    sound.specification.channels = static_cast<int>(channels);
    sound.samples = samples;
    m_impl->sounds[key] = std::move(sound);
    return true;
}

void CAudioPlayer::StopOwner(std::uint64_t owner) {
    for (std::size_t i = 0; i < m_impl->playing.size();) {
        if (m_impl->playing[i].owner == owner && m_impl->playing[i].loop) {
            m_impl->Recycle(m_impl->playing[i].stream);
            m_impl->playing.erase(m_impl->playing.begin() + i);
        } else { ++i; }
    }
}

void CAudioPlayer::SetPaused(bool paused) {
    if (m_impl->paused == paused) { return; }
    const std::uint64_t nowMs = SDL_GetTicks();
    if (paused) { m_impl->pauseStartMs = nowMs; }
    for (PlayingSound &sound : m_impl->playing) {
        if (!paused) {
            sound.finishMs += nowMs - m_impl->pauseStartMs;
        }
    }
    if (m_impl->device != 0) {
        if (paused) { SDL_PauseAudioDevice(m_impl->device); }
        else { SDL_ResumeAudioDevice(m_impl->device); }
    }
    m_impl->paused = paused;
}

