#pragma once
#include "gun_bros_re/effects/CParticleEffectPlayer.h"
#include "gun_bros_re/effects/ZParticleResources.h"
#include "gun_bros_re/effects/CParticlePool.h"
#include "engine/glu/sprite/ZSpriteRenderer.h"
#include "engine/glu/movie/ZMovieRenderer.h"
#include "engine/core/ZMatrix4d.h"
namespace MenuDetail {
/** Desktop resource lifetime only; menus own particle selection and playback. */
class ZMenuParticleContext {
public:
    void Bind(CResTOCManager &archive, CGunBros &tables, ZShaderProgram &program) {
        toc = &archive; source = &tables; shader = &program;
    }
    void Prepare() {
        if (!resources) { resources = std::make_unique<ZParticleResources>(*source); }
        if (!renderer) { renderer = std::make_unique<ZSpriteRenderer>(*toc, *shader); }
    }
    CResTOCManager *toc = nullptr;
    std::unique_ptr<ZParticleResources> resources;
    std::unique_ptr<ZSpriteRenderer> renderer;
    std::uint32_t random = 1;
    // CMenuSystem::Init :97381 supplies one shared pool to its menu players.
    std::shared_ptr<CParticlePool> pool = std::make_shared<CParticlePool>(200);
private:
    CGunBros *source = nullptr;
    ZShaderProgram *shader = nullptr;
};
}
