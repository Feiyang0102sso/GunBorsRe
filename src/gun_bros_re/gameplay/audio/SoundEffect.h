#pragma once
/** Original SoundEffect::Init :130191; entries/sound_effect.bt.
 * SOUNDEFFECT contains only a WAV reference. Playback mode belongs to the caller.
 */
#include "gun_bros_re/data/objects/CGameAssetRef.h"

struct SoundEffect {
    CGameAssetRef wav;
    bool Init(CArrayInputStream &stream) {
        wav.Init(stream);
        return !stream.Overran();
    }
};
