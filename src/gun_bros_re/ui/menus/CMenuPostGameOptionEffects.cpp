#include "gun_bros_re/ui/menus/CMenuPostGameOptionEffects.h"
namespace MenuDetail {
    /** CMenuPostGameOption::Bind/Update/Draw :249755..249964 owns one
     * particle player per card, behind the centered icon. */
    bool CMenuPostGameOption::Effects::AdvancePostGameEffect(unsigned index, unsigned elapsed) {
        static const struct { const char *pack; int ordinals[7]; } binding =
#include "gun_bros_re/ui/CMenuPostGameParticleData.inc"
        ;
        static const struct { const char *pack; int ordinals[9]; } liveBinding =
#include "gun_bros_re/ui/CMenuLivePostGameParticleData.inc"
        ;
        context.Prepare();
        if (index >= postGameEffects.size()) { return false; }
        if (!postGameEffects[index]) {
            const char *packName = binding.pack;
            int ordinal = 0;
            if (index < 7) { ordinal = binding.ordinals[index]; }
            else { packName = liveBinding.pack; ordinal = liveBinding.ordinals[index - 7]; }
            const int pack = context.toc->GetPackIndexFromName(packName);
            if (pack < 0 || ordinal < 0) { return false; }
            GameObjectRef resource;
            resource.packHash = context.toc->GetPack(pack)->GetPackHash();
            resource.localIndex = static_cast<std::uint8_t>(ordinal);
            const auto *data = context.resources->Get(resource);
            if (data == nullptr) { return false; }
            auto effect = std::make_unique<CParticleEffectPlayer>();
            effect->Init(*data, context.pool);
            // CParticleEffectPlayer's constructor enables looping (:131269).
            effect->SetLooping(true);
            postGameEffects[index] = std::move(effect);
        }
        postGameEffects[index]->Update(elapsed, context.random);
        return true;
    }


}
