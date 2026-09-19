#include "gun_bros_re/gameplay/audio/ZCombatAudio.h"
#include <cstdio>

namespace {
std::uint64_t ResourceKey(const GameObjectRef &ref) {
    return (static_cast<std::uint64_t>(ref.packHash) << 32) | ref.localIndex;
}
}

void ZCombatAudio::PlayCue(const ZGunCue &cue, Collision::ObjectId actor) {
    if (cue.kind == ZGunCue::Kind::StopSound) {
        audio.StopOwner(actor);
        activeLoops.erase(actor);
        return;
    }
    const auto key = ResourceKey(cue.resource);
    auto found = soundReferences.find(key);
    if (found == soundReferences.end()) {
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(cue.resource.packHash, ZGameSection::SoundEffect,
            cue.resource.localIndex, payload)) {
            std::printf("[weapon-audio] missing sound %08x:%u\n", cue.resource.packHash, cue.resource.localIndex);
            return;
        }
        CArrayInputStream stream(payload);
        SoundEffect effect;
        if (!effect.Init(stream)) {
            std::printf("[weapon-audio] invalid sound %08x:%u\n", cue.resource.packHash, cue.resource.localIndex);
            return;
        }
        found = soundReferences.emplace(key, effect).first;
    }
    const CGameAssetRef &wav = found->second.wav;
    if (wav.assetId < 0) { return; }
    PlayWav(wav.packHash, wav.assetId, cue.kind == ZGunCue::Kind::LoopSound, actor);
}

void ZCombatAudio::PlayWav(std::uint32_t packHash, int ordinal, bool loop, Collision::ObjectId actor,
                 bool moveSound) {
    CGameAssetRef wav;
    wav.packHash = packHash;
    wav.assetId = ordinal;
    const std::uint64_t key = (static_cast<std::uint64_t>(wav.packHash) << 32) | wav.assetId;
    // User-verified simultaneous-death behaviour. Coalesce by resolved WAV,
    // not enemy ID.
    if (!loop && frameSounds.find(key) != frameSounds.end()) { return; }
    // Host audio adaptation, not original behaviour: the original starts a
    // separate voice per actor (CMoveSetMeshController::Update :134066 ->
    // CSoundQueue::PlaySound :102896 -> CMediaPlayer::PlayInternal :363259
    // allocates a new sound event every call), so a group death is many
    // copies of one WAV on top of each other and reads as a single hit. One
    // voice per WAV cannot get louder, so a kill streak spread over a few
    // ticks would instead retrigger it over and over. Move-frame sounds --
    // the death and animation cues -- therefore wait out the copy already
    // playing. Gun cues keep the per-tick rule so rapid fire stays rapid.
    const auto busy = moveSoundBusyMs.find(key);
    if (moveSound && !loop && busy != moveSoundBusyMs.end() && busy->second > audioClockMs) { return; }
    if (!audio.HasSound(key)) {
        std::vector<std::uint8_t> payload;
        if (!tables.ReadSectionResource(wav.packHash, ZGameSection::Wav, wav.assetId, payload)) {
            std::printf("[weapon-audio] missing WAV %08x:%d\n", wav.packHash, wav.assetId);
            return;
        }
        if (!audio.Load(key, payload)) { return; }
    }
    if (loop) {
        // Re-arming a loop that is already running restarts its attack
        // every tick, which is what turns a machine's running sound into
        // a buzz. Leave the copy that is already playing alone.
        const auto running = activeLoops.find(actor);
        if (running != activeLoops.end() && running->second == key) { return; }
        audio.StopOwner(actor);
        activeLoops[actor] = key;
    } else if (!moveSound) {
        // Host audio adaptation: one voice per WAV. The original layers
        // copies (PlayInternal :363259 builds a new sound event per call)
        // and absorbs them in its own per-event gain (:303199), which this
        // host has no data for -- layering here just makes one effect
        // louder the faster it repeats, which is exactly how a turret ends
        // up drowning out everything else. Retrigger instead of stacking:
        // the rate is unchanged, the level stays where the WAV put it.
        audio.Stop(key);
    }
    if (audio.Play(key, loop, actor)) {
        ++soundCues;
        if (!loop) { frameSounds.insert(key); }
        if (moveSound && !loop) { moveSoundBusyMs[key] = audioClockMs + audio.GetDurationMs(key); }
    }
}

void ZCombatAudio::RetireOwner(Collision::ObjectId owner) {
    audio.StopOwner(owner);
    // The id can come back on a new actor; do not let a stale entry keep its
    // loop silent.
    activeLoops.erase(owner);
}
void ZCombatAudio::Clear() {
    moveSoundBusyMs.clear();
    activeLoops.clear();
    audioClockMs = 0;
    audio.StopAll();
    frameSounds.clear();
}
