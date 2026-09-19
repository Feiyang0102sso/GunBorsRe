#pragma once
#include "gun_bros_re/ui/menus/CMenuPostGameOption.h"
#include "gun_bros_re/ui/host/ZMenuParticleContext.h"
namespace MenuDetail {
class CMenuPostGameOption::Effects {
public:
    Effects(ZMenuParticleContext &shared, ZMovieRenderer &renderer) : context(shared), movies(renderer) {}
    /** CMenuPostGameOption::Bind/Update/Draw :249755..249964 owns one
     * particle player per card, behind the centered icon. */
    bool AdvancePostGameEffect(unsigned index, unsigned elapsed);
    void ResetPostGameEffects() {
        for (auto &effect : postGameEffects) { effect.reset(); }
    }
    std::size_t PostGameParticleCount(unsigned index) const {
        if (!postGameEffects[index]) { return 0; }
        return postGameEffects[index]->GetParticleCount();
    }
    void DrawPostGameEffect(unsigned index, const ZMovieRegion &icon) {
        float transform[16];
        std::copy(movies.CurrentProjection(), movies.CurrentProjection() + 16, transform);
        Matrix4dTranslate(transform, icon.x + icon.width / 2, icon.y + icon.height / 2);
        postGameEffects[index]->Draw(*context.renderer, transform);
    }
private:
    ZMenuParticleContext &context;
    ZMovieRenderer &movies;
    std::array<std::unique_ptr<CParticleEffectPlayer>, 16> postGameEffects;
};
}
