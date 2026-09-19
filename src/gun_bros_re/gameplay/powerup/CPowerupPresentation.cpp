#include "engine/core/ZPaths.h"
/** @file CPowerupPresentation.cpp
 * @brief iOS CPowerup::Update :188652; native 3 radius / damage :188295.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/powerup/CPowerup.h"
#include "gun_bros_re/data/ZPowerupCatalog.h"
#include "engine/glu/movie/ZMovieRenderer.h"
#include "engine/core/ZMatrix4d.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/effects/CParticlePool.h"
#include "gun_bros_re/ui/hud/CPowerUpSelector.h"
#include <cstdio>
#include "gun_bros_re/effects/CParticleEffectPlayer.h"
#include "gun_bros_re/effects/ZParticleResources.h"
#include "gun_bros_re/gameplay/audio/ZCombatAudio.h"
#include "engine/glu/sprite/ZSpriteRenderer.h"

/** Host for CPowerup movies, screen particles and completion callbacks.
 * Desktop rendering state belongs to the same CPowerup as its interpreter.
 */
struct CPowerup::Presentation {
    Presentation(CResTOCManager &source, ZPackTables &resources, CLevel &level)
        : toc(source), tables(resources), scene(level), particleResources(resources), audio(resources) {}
    CResTOCManager &toc;
    ZPackTables &tables;
    CLevel &scene;
    ZShaderProgram program;
    ZParticleResources particleResources;
    ZCombatAudio audio;
    std::unique_ptr<ZSpriteRenderer> particleRenderer;
    std::shared_ptr<CParticlePool> particlePool = std::make_shared<CParticlePool>(100);
    std::array<CParticleEffectPlayer, 5> particles;
    std::uint32_t particleRandom = 1;
    std::map<unsigned, std::unique_ptr<ZMovieRenderer>> renderers;
    ZMovieRenderer *renderer = nullptr;
    CMovie::Playback movie;
    unsigned movieOrdinal = 0, elapsed = 0;
    bool active = false, movieActive = false;
    bool occupiesPresentation = false;
    bool foregroundMovie = false;
    std::uint32_t random = 0xC0381125;
};

CPowerup::CPowerup() = default;
CPowerup::CPowerup(CResTOCManager &toc, ZPackTables &tables, CLevel &scene)
    : m_presentation(std::make_unique<Presentation>(toc, tables, scene)) {}
CPowerup::~CPowerup() = default;

void CPowerup::SetOwner(Collision::ObjectId owner) {
    m_owner = owner;
}
bool CPowerup::IsActive() const { return m_presentation && m_presentation->active; }
bool CPowerup::IsPresentationActive() const {
    // A presentation may wait on Flow timers between Movies. Keep ownership
    // until Exit; a timer-only Flow never acquires this presentation lifetime.
    return HasSelectorFrame() || (m_presentation && m_presentation->active && (m_pausedLevel || m_presentation->occupiesPresentation));
}
unsigned CPowerup::GetElapsed() const {
    if (!m_presentation) { return 0; }
    return m_presentation->elapsed;
}
bool CPowerup::IsForegroundMovie() const {
    return m_presentation && m_presentation->movieActive && m_presentation->foregroundMovie;
}
bool CPowerup::HasSelectorFrame() const { return m_selector != nullptr && m_selector->HasPowerupFrame(); }
bool CPowerup::IsSelectorFrameClosing() const {
    return m_selector != nullptr && m_selector->IsPowerupFrameClosing();
}

bool CPowerup::Start(const ZPowerupEntry &entry, bool fromSelector, unsigned stock) {
    if (!m_presentation) {
        std::printf("[powerup] presentation is not bound\n");
        return false;
    }
    Presentation &presentation = *m_presentation;
    if (presentation.active || HasSelectorFrame()) { return false; }
    if (!presentation.particleRenderer) {
        if (!presentation.program.Load(Paths::Shaders().c_str(), "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return false; }
        // CPowerup::Bind :188745 shares 100 slots across its five screen players.
        presentation.particleRenderer = std::make_unique<ZSpriteRenderer>(presentation.toc, presentation.program);
    }
    // A previously accepted throw keeps its inventory reservation while other
    // effects run; only an explicit Reset cancels the complete host lifecycle.
    ResetExecution();
    if (m_selector != nullptr && !m_selector->BeginPowerupPresentation(fromSelector)) { ResetExecution(); return false; }
    if (fromSelector && m_selector != nullptr) {
        // CPowerUpSelector::Draw :186536 keeps its main Movie behind the
        // foreground powerup after HideOnlyItems. Native 5 closes it later.
        presentation.occupiesPresentation = true;
    }
    m_resource = entry.resource;
    m_stock = stock;
    Bind(entry.data, ReadActorStatus());
    SetLevelContext(&presentation.scene);
    Equip();
    if (m_actorActionFailed || m_unsupported > 0) { ResetExecution(); return false; }
    Use(fromSelector);
    presentation.active = true;
    std::printf("[powerup-movie] start %s\n", entry.owner.c_str());
    if (!ApplyPresentationActions()) { ResetExecution(); return false; }
    if (IsDone()) {
        presentation.active = false;
        presentation.movieActive = false;
        if (m_selector != nullptr) { m_selector->FinishPowerupPresentation(); }
    }
    return true;
}

bool CPowerup::StartMovie(const ZPowerupAction &action) {
    Presentation &presentation = *m_presentation;
    const int packIndex = presentation.toc.GetPackIndexFromHash(action.resource.packHash);
    if (packIndex < 0) { return false; }
    if (presentation.renderers.count(packIndex) == 0) {
        auto renderer = std::make_unique<ZMovieRenderer>();
        if (!renderer->Init(*presentation.toc.GetPack(packIndex), *presentation.toc.GetPack(presentation.toc.GetCorePackIndex()))) { return false; }
        presentation.renderers[packIndex] = std::move(renderer);
    }
    presentation.renderer = presentation.renderers[packIndex].get();
    presentation.movieOrdinal = action.resource.localIndex;
    CMovie *movie = presentation.renderer->GetMovie(presentation.movieOrdinal);
    if (movie == nullptr) { return false; }
    presentation.movie.Bind(*movie);
    presentation.movieActive = true;
    presentation.occupiesPresentation = true;
    presentation.foregroundMovie = action.function == 15;
    const bool loop = action.function == 1 && action.arguments[1] == 1;
    presentation.movie.SetLoop(loop);
    std::printf("[powerup-movie] movie=%s:%u duration=%u loop=%d\n", presentation.toc.GetPack(packIndex)->GetShortName().c_str(),
        presentation.movieOrdinal, movie->duration, loop);
    return true;
}

bool CPowerup::ApplyPresentationActions() {
    Presentation &presentation = *m_presentation;
    if (m_actorActionFailed || m_unsupported > 0) { return false; }
    // A ready InputPad may notify synchronously. Drain the resulting native
    // actions before observing Exit, otherwise its final damage can be lost.
    while (!m_actions.empty()) {
        const auto actions = TakeActions();
        for (const ZPowerupAction &action : actions) {
            if (action.function == 1 || action.function == 15) {
                if (!StartMovie(action)) { ++failures; return false; }
            } else if (action.function == 2 || action.function == 4 || action.function == 5 || action.function == 13) {
                presentation.occupiesPresentation = true;
                // The former 300 ms approximation is replaced by the consumers:
                // selector SetState(6/7) plays authored chapter 3; item controls
                // reverse chapter 0; InputPad reports its own completed sequence.
                if (m_selector == nullptr) {
                    std::printf("[powerup] UI native=%u has no selector binding\n", action.function);
                    ++failures;
                    return false;
                }
                bool applied = false;
                if (action.function == 2) { applied = m_selector->Hide(); }
                if (action.function == 4) { applied = m_selector->HideOnlyItems(); }
                if (action.function == 5) { applied = m_selector->HideSelector(); }
                if (action.function == 13) { applied = m_selector->RestoreInputPad(); }
                if (!applied) { ++failures; return false; }
            } else if (action.function == 14 || action.function == 26) {
                // Selector-only input/mode controls are already inaccessible while
                // its powerup owns presentation. Neither native emits an event.
            } else if (action.function == 21) {
                // CPowerup native 21 calls OnRevive(1), distinct from rescue(0).
                if (!presentation.scene.ReviveActor(m_owner, 1)) { ++failures; return false; }
            } else if (action.function == 3) {
                Collision::Hit hit;
                hit.owner = m_owner;
                hit.ownerType = 0;
                // Native 3 reads CMap::CCamera's center (+9960/+9964), which
                // differs from the player near the camera bounds.
                hit.x = presentation.scene.GetViewCenterX();
                hit.y = presentation.scene.GetViewCenterY();
                hit.damage = action.arguments[0] * 10.0f;
                hit.splash = true;
                hit.flags = 2; // Original FireSplashDamageForceAttribute argument 7.
                hit.applyArmorAttack = false;
                // Native 3 passes radius second and damage first * 10. Both are
                // world units; target scripts still receive their splash callback.
                presentation.scene.Splash(hit, static_cast<float>(action.arguments[1]), 360, 0, 0);
                ++splashCount;
                std::printf("[powerup-movie] splash at=%u damage=%.0f radius=%d\n", presentation.elapsed, hit.damage, action.arguments[1]);
            } else if (action.function == 6 || action.function == 7) {
                float x = 0, y = 0;
                if (action.function == 7) {
                    x = action.arguments[0] / 256.0f;
                    y = action.arguments[1] / 256.0f;
                } else {
                    presentation.random = presentation.random * 1664525u + 1013904223u;
                    x = (presentation.random >> 8) / 16777216.0f;
                    presentation.random = presentation.random * 1664525u + 1013904223u;
                    y = (presentation.random >> 8) / 16777216.0f;
                }
                // Original maintains at most five simultaneous screen emitters.
                for (auto &player : presentation.particles) {
                    if (!player.IsDone()) { continue; }
                    const auto *data = presentation.particleResources.Get(action.resource);
                    if (data == nullptr) { ++failures; return false; }
                    player.Init(*data, presentation.particlePool);
                    player.SetLooping(false);
                    player.SetPosition(x * 1024, y * 768, 0, 0);
                    ++effectCount;
                    break;
                }
            } else if (action.function == 9) {
                ZGunCue cue;
                cue.kind = ZGunCue::Kind::Sound;
                cue.resource = action.resource;
                if (GetLevelContext() != nullptr) {
                    float x = 0, y = 0;
                    presentation.scene.ActorPosition(m_owner, x, y);
                    GetLevelContext()->Emit(cue, x, y, 0, 0, m_owner);
                } else { presentation.audio.PlayCue(cue); }
            } else {
                ++failures;
                std::printf("[powerup-movie] unhandled native=%u\n", action.function);
                return false;
            }
        }
    }
    return true;
}

void CPowerup::UpdatePresentation(int deltaMs) {
    Presentation &presentation = *m_presentation;
    if (deltaMs <= 0) { return; }
    presentation.audio.BeginFrame();
    presentation.audio.Update();
    for (auto &player : presentation.particles) { player.Update(deltaMs, presentation.particleRandom); }
    if (!presentation.active) {
        // Native Hide may outlive Flow Exit. Let the selector finish closing;
        // only an explicit Reset cancels that playback without completion.
        if (m_selector != nullptr) {
            m_selector->UpdatePowerupPresentation(static_cast<unsigned>(deltaMs));
            if (!HasSelectorFrame()) { m_selector->EndPowerupPresentation(); }
        }
        return;
    }
    presentation.elapsed += deltaMs;
    // Advance only instances already playing at frame entry. A completion may
    // start another movie, which must not receive this frame's delta twice.
    if (presentation.movieActive) { presentation.movie.Update(static_cast<unsigned>(deltaMs)); }
    if (m_selector != nullptr) { m_selector->UpdatePowerupPresentation(static_cast<unsigned>(deltaMs)); }
    if (!ApplyPresentationActions()) { ResetExecution(); return; }
    if (presentation.movieActive) {
        if (presentation.movie.TakeCompletion()) {
            presentation.movieActive = false;
            ++movieCompletions;
            HandleEvent(0);
            if (!ApplyPresentationActions()) { ResetExecution(); return; }
        }
    }
    UpdateTimer(deltaMs);
    if (!ApplyPresentationActions()) { ResetExecution(); return; }
    if (IsDone()) {
        presentation.active = false;
        presentation.movieActive = false;
        if (m_selector != nullptr) { m_selector->FinishPowerupPresentation(); }
        std::printf("[powerup-movie] complete elapsed=%u movies=%u splashes=%u\n", presentation.elapsed, movieCompletions, splashCount);
    }
}

bool CPowerup::Draw() {
    if (!m_presentation) { return true; }
    Presentation &presentation = *m_presentation;
    // This pass is also invoked by an isolated research harness before any HUD.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    if (presentation.particleRenderer) {
        float projection[16];
        Matrix4dOrthoTopLeft(1024, 768, 1, projection);
        presentation.particleRenderer->Begin();
        for (const auto &player : presentation.particles) { player.QueueParticles(*presentation.particleRenderer); }
        presentation.particleRenderer->Draw(projection);
    }
    if (m_selector != nullptr && !m_selector->DrawPowerupPresentation()) { return false; }
    if (presentation.movieActive && presentation.renderer != nullptr) { return presentation.renderer->Draw(presentation.movieOrdinal, presentation.movie.GetTime()); }
    return failures == 0;
}

void CPowerup::ResetExecution() {
    m_actions.clear();
    m_timerMs = 0;
    m_done = true;
    if (m_selector != nullptr) { m_selector->EndPowerupPresentation(); }
    if (!m_presentation) { return; }
    Presentation &presentation = *m_presentation;
    if (m_pausedLevel) {
        // Cancel/restart must release the pause even without the script's exit.
        presentation.scene.FunctionResolver(65, nullptr, 0);
        m_pausedLevel = false;
    }
    presentation.active = false;
    presentation.movieActive = false;
    presentation.occupiesPresentation = false;
    presentation.movie.Cancel();
    presentation.elapsed = 0;
    for (auto &player : presentation.particles) { player.Stop(); }
    presentation.audio.Clear();
}
