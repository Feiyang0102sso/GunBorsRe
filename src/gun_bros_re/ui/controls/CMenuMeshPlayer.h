#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/gameplay/brother/CBrother.h"
#include "engine/platform/ZAudioPlayer.h"

namespace MenuDetail {
class ZMenuSurface;
/** CMenuMeshPlayer owns configuration, swap playback and model audio.
 * Original Update/Draw :169471/:169591; the surface supplies the GL context. */
class CMenuMeshPlayer {
public:
    void Bind(CGunBros &tables) { resourceTables = &tables; }
    bool Draw(ZMenuSurface &view, CResTOCManager &toc, CGunBros &tables, const CProfileManager &profile,
        const std::vector<CGun::Entry> &weapons, const std::vector<CArmor::Entry> &armors, unsigned slot,
        const GameObjectTypeRef *previewItem = nullptr, const ZMovieRegion *storePanel = nullptr, float spin = 0);

    const std::array<float, 16> &LastModelMatrix() const { return lastModelMatrix; }
    std::size_t PreviewSoundCount() const { return previewSoundCount; }
    void EnableSilentPreviewAudio() { previewAudio.EnableSilentValidation(); }
    ZAudioPlaybackState PreviewAudioState() const { return previewAudio.GetPlaybackState(); }
    CBrother *GetPlayerPreview() const { return equippedPreview.get(); }
    unsigned GetPlayerPreviewSlot() const { return previewGunSlot; }
    bool TakePlayerPreviewSlotChange() {
        const bool changed = previewSlotChanged;
        previewSlotChanged = false;
        return changed;
    }
    /** CMenuMeshPlayer::Update observes configuration only after native 3. */
    void AdvancePlayerPreview(int deltaMs);
private:
    std::array<float, 16> lastModelMatrix{};
    void PlayPreviewSounds(const std::vector<ZMoveSoundRef> &sounds);
    ZAudioPlayer previewAudio;
    std::size_t previewSoundCount = 0;
    std::unique_ptr<CBrother> equippedPreview;
    CPlayerConfiguration previewConfiguration;
    unsigned previewGunSlot = 0;
    unsigned previewPrimarySlot = 0;
    bool previewSwapPending = false;
    bool previewSlotChanged = false;
    std::uint64_t previewTicks = 0;
    CGunBros *resourceTables = nullptr;
};
}
