#pragma once
// Combat-specific dispatch belongs beside CBGM and SoundEffect. The shared
// SDL device/decoder remains in engine/platform; the Z name marks policies
// that have not yet been replaced by the original sound-event lifecycle.
/** Windows combat sound adaptation. Original events stay with their actors.
 * Keep WAV coalescing, loop ownership and move-frame busy windows unchanged.
 */
#include "engine/platform/ZAudioPlayer.h"
#include "gun_bros_re/application/CGunBros.h"
#include "gun_bros_re/gameplay/weapon/CGun.h"
#include "gun_bros_re/gameplay/collision/Collision.h"
#include "gun_bros_re/gameplay/audio/SoundEffect.h"
#include <map>
#include <set>

class ZCombatAudio {
public:
    explicit ZCombatAudio(CGunBros &resources) : tables(resources) {}
    void PlayCue(const ZGunCue &cue, Collision::ObjectId actor = Collision::Player);
    void PlayWav(std::uint32_t packHash, int ordinal, bool loop = false,
        Collision::ObjectId actor = Collision::Player, bool moveSound = false);
    void BeginFrame() { frameSounds.clear(); }
    void AdvanceClock(int deltaMs) { if (deltaMs > 0) { audioClockMs += deltaMs; } }
    void Update() { audio.Update(); }
    void RetireOwner(Collision::ObjectId owner);
    void Clear();
    void SetPaused(bool paused) { audio.SetPaused(paused); }
    unsigned GetVoiceCount() const { return audio.GetVoiceCount(); }
    std::size_t GetSoundCueCount() const { return soundCues; }
private:
    CGunBros &tables;
    ZAudioPlayer audio;
    std::size_t soundCues = 0;
    std::set<std::uint64_t> frameSounds;
    // Milliseconds of simulated combat, and when each move sound stops covering
    // repeats of itself.
    int audioClockMs = 0;
    std::map<std::uint64_t, int> moveSoundBusyMs;
    // The loop each owner currently has running, so it is not re-armed.
    std::map<Collision::ObjectId, std::uint64_t> activeLoops;
    std::map<std::uint64_t, SoundEffect> soundReferences;
};
