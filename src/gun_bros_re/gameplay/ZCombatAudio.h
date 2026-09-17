#pragma once
/** Windows combat sound adaptation. Original events stay with their actors.
 * Keep WAV coalescing, loop ownership and move-frame busy windows unchanged.
 */
#include "engine/platform/ZAudioPlayer.h"
#include "gun_bros_re/data/ZPackTables.h"
#include "gun_bros_re/gameplay/CGun.h"
#include "gun_bros_re/gameplay/ZCombatTypes.h"
#include <map>
#include <set>

class ZCombatAudio {
public:
    explicit ZCombatAudio(ZPackTables &resources) : tables(resources) {}
    void PlayCue(const ZGunCue &cue, ZCombatId actor = kPlayerCombatId);
    void PlayWav(std::uint32_t packHash, int ordinal, bool loop = false,
        ZCombatId actor = kPlayerCombatId, bool moveSound = false);
    void BeginFrame() { frameSounds.clear(); }
    void AdvanceClock(int deltaMs) { if (deltaMs > 0) { audioClockMs += deltaMs; } }
    void Update() { audio.Update(); }
    void RetireOwner(ZCombatId owner);
    void Clear();
    void SetPaused(bool paused) { audio.SetPaused(paused); }
    unsigned GetVoiceCount() const { return audio.GetVoiceCount(); }
    std::size_t GetSoundCueCount() const { return soundCues; }
private:
    ZPackTables &tables;
    ZAudioPlayer audio;
    std::uint64_t loopSound = 0;
    std::size_t soundCues = 0;
    std::set<std::uint64_t> frameSounds;
    // Milliseconds of simulated combat, and when each move sound stops covering
    // repeats of itself.
    int audioClockMs = 0;
    std::map<std::uint64_t, int> moveSoundBusyMs;
    // The loop each owner currently has running, so it is not re-armed.
    std::map<ZCombatId, std::uint64_t> activeLoops;
    std::map<std::uint64_t, CGameAssetRef> soundReferences;
};
