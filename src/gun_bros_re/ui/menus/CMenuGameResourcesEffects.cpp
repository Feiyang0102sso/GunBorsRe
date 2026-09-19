#include "gun_bros_re/ui/menus/CMenuGameResourcesEffects.h"
namespace MenuDetail {
    bool CMenuGameResources::Effects::StartRefineryEffect(unsigned slot, unsigned icon, float x, float y) {
        static const struct { const char *pack; int ordinals[4]; } binding =
#include "gun_bros_re/ui/CMenuRefineryParticleData.inc"
        ;
        if (slot >= refineryEffects.size() || icon >= std::size(binding.ordinals)) { return false; }
        const int pack = context.toc->GetPackIndexFromName(binding.pack);
        if (pack < 0 || binding.ordinals[icon] < 0) { return false; }
        GameObjectRef resource;
        resource.packHash = context.toc->GetPack(pack)->GetPackHash();
        resource.localIndex = static_cast<std::uint8_t>(binding.ordinals[icon]);
        context.Prepare();
        const auto *data = context.resources->Get(resource);
        if (data == nullptr) { return false; }
        auto &effect = refineryEffects[slot];
        effect.player = std::make_unique<CParticleEffectPlayer>();
        // SetupTransfer :174510/:174534 uses the original ICON_STANDARD particle.
        // Positions are local to CTransferEffect::Draw, including living particles.
        effect.player->Init(*data, context.pool);
        effect.player->SetLooping(true);
        effect.x = x;
        effect.y = y;
        return true;
    }

    void CMenuGameResources::Effects::AdvanceRefineryEffects(unsigned elapsed) {
        for (auto &effect : refineryEffects) {
            if (effect.player) { effect.player->Update(elapsed, context.random); }
        }
    }

    void CMenuGameResources::Effects::MoveRefineryEffect(unsigned slot, float x, float y) {
        refineryEffects[slot].x = x;
        refineryEffects[slot].y = y;
    }

    void CMenuGameResources::Effects::StopRefineryEffect(unsigned slot) {
        auto &effect = refineryEffects[slot];
        // CTransferEffect::Update :174361 calls StopSpawning, not Clear.
        // StopEffect detaches the emitter while its living particles expire.
        // Correction: that old host name now explicitly becomes StopSpawning.
        if (effect.player) { effect.player->StopSpawning(); }
    }

    void CMenuGameResources::Effects::DrawRefineryEffects() {
        for (auto &effect : refineryEffects) {
            if (!effect.player) { continue; }
            float transform[16];
            std::copy(movies.CurrentProjection(), movies.CurrentProjection() + 16, transform);
            Matrix4dTranslate(transform, effect.x, effect.y);
            effect.player->Draw(*context.renderer, transform);
        }
    }


}
