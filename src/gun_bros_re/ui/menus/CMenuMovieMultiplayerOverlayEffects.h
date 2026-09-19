#pragma once
#include "gun_bros_re/ui/menus/CMenuMovieMultiplayerOverlay.h"
#include "gun_bros_re/ui/host/ZMenuParticleContext.h"
namespace MenuDetail {
class CMenuMovieMultiplayerOverlay::Effects {
public:
    Effects(ZMenuParticleContext &shared, ZMovieRenderer &renderer) : context(shared), movies(renderer) {}
    bool PrepareModeEffects();
    bool StartModeSelectionEffect() {
        if (!PrepareModeEffects()) { return false; }
        modeEffects[0]->Start();
        return true;
    }
    bool AdvanceModeEffects(unsigned elapsed) {
        if (!PrepareModeEffects()) { return false; }
        for (auto &effect : modeEffects) { effect->Update(elapsed, context.random); }
        return true;
    }
    void DrawModeEffects(const ZMovieRegion &label);
    std::size_t ModeParticleCount() const {
        if (!modeEffects[0]) { return 0; }
        return modeEffects[0]->GetParticleCount();
    }
private:
    ZMenuParticleContext &context;
    ZMovieRenderer &movies;
    std::array<std::unique_ptr<CParticleEffectPlayer>, 2> modeEffects;
    std::array<GameObjectRef, 2> modeEffectRefs;
};
}
