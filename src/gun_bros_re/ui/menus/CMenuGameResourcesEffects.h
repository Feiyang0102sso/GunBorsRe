#pragma once
#include "gun_bros_re/ui/menus/CMenuGameResources.h"
#include "gun_bros_re/ui/host/ZMenuParticleContext.h"
namespace MenuDetail {
class CMenuGameResources::Effects {
public:
    Effects(ZMenuParticleContext &shared, ZMovieRenderer &renderer) : context(shared), movies(renderer) {}
    /** Native refinery transfer players keep living particles after arrival. */
    bool StartRefineryEffect(unsigned slot, unsigned icon, float x, float y);
    void AdvanceRefineryEffects(unsigned elapsed);
    void MoveRefineryEffect(unsigned slot, float x, float y);
    void StopRefineryEffect(unsigned slot);
    void DrawRefineryEffects();
    void ResetRefineryEffects() {
        for (auto &effect : refineryEffects) { effect = {}; }
    }
    std::size_t RefineryParticleCount(unsigned slot) const {
        if (!refineryEffects[slot].player) { return 0; }
        return refineryEffects[slot].player->GetParticleCount();
    }
private:
    ZMenuParticleContext &context;
    ZMovieRenderer &movies;
    struct RefineryEffect {
        std::unique_ptr<CParticleEffectPlayer> player;
        float x = 0, y = 0;
    };
    std::array<RefineryEffect, kRefinementSlotCount> refineryEffects;
};
}
