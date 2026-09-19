#include "gun_bros_re/ui/menus/CMenuMovieMultiplayerOverlayEffects.h"
namespace MenuDetail {
    bool CMenuMovieMultiplayerOverlay::Effects::PrepareModeEffects() {
        if (modeEffects[0]) { return true; }
        context.Prepare();
        static const struct { const char *pack; int ordinals[2]; } binding =
#include "gun_bros_re/ui/CMenuModeParticleData.inc"
        ;
        const int pack = context.toc->GetPackIndexFromName(binding.pack);
        if (pack < 0) { return false; }
        for (unsigned index = 0; index < 2; ++index) {
            if (binding.ordinals[index] < 0) { return false; }
            modeEffectRefs[index].packHash = context.toc->GetPack(pack)->GetPackHash();
            modeEffectRefs[index].localIndex = static_cast<std::uint8_t>(binding.ordinals[index]);
            const auto *data = context.resources->Get(modeEffectRefs[index]);
            if (data == nullptr) { return false; }
            modeEffects[index] = std::make_unique<CParticleEffectPlayer>();
            modeEffects[index]->Init(*data, context.pool);
            modeEffects[index]->SetLooping(data->GetDurationMs() == 0);
        }
        // Bind stops the selection burst; the second emitter runs continuously.
        modeEffects[0]->Stop();
        return true;
    }

    void CMenuMovieMultiplayerOverlay::Effects::DrawModeEffects(const ZMovieRegion &label) {
        // LabelCallback :250137 positions the burst at label top-center and
        // the persistent glow at its center, before drawing the text itself.
        for (unsigned index = 0; index < 2; ++index) {
            float transform[16];
            std::copy(movies.CurrentProjection(), movies.CurrentProjection() + 16, transform);
            float y = label.y;
            if (index == 1) { y += label.height / 2; }
            Matrix4dTranslate(transform, label.x + label.width / 2, y);
            modeEffects[index]->Draw(*context.renderer, transform);
        }
    }


}
