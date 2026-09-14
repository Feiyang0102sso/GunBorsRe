#include "engine/core/Paths.h"
/** @file PowerupMoviePlayer.cpp
 * @brief iOS CPowerup::Update :188652; native 3 radius / damage :188295.
 */
#define NOMINMAX
#include "gun_bros_re/gameplay/PowerupMoviePlayer.h"
#include "engine/core/CMatrix4d.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include <cstdio>

PowerupMoviePlayer::PowerupMoviePlayer(CResTOCManager &toc, PackTables &tables, CombatScene &scene)
    : m_toc(toc), m_tables(tables), m_scene(scene) {}

bool PowerupMoviePlayer::Start(const PowerupEntry &entry, bool fromSelector) {
    if (m_active) { return false; }
    if (!m_particles) {
        if (!m_program.Load(Paths::Shaders().c_str(), "ogles_vs_mvp_tex0", "ogles_ps_tex0")) { return false; }
        m_particles = std::make_unique<WeaponEffects>(m_toc, m_tables, m_program);
    }
    Reset();
    if (fromSelector) {
        if (!m_selectorRenderer) {
            auto renderer = std::make_unique<MovieRenderer>();
            CResPackTOC &core = *m_toc.GetPack(m_toc.GetCorePackIndex());
            if (!renderer->Init(core, core)) { return false; }
            m_selectorRenderer = std::move(renderer);
        }
        // CPowerUpSelector::Draw :186536 keeps its main Movie behind the
        // foreground powerup after HideOnlyItems. Native 5 closes it later.
        m_selectorOrdinal = m_selectorRenderer->Ordinal("GLU_MOVIE_POWERUP_MENU_NEW");
        const CMovie *movie = m_selectorRenderer->GetMovie(m_selectorOrdinal);
        if (movie == nullptr || !movie->GetChapterRange(2, m_selectorLoopStart, m_selectorLoopEnd)) { return false; }
        m_selectorTime = m_selectorLoopStart;
        m_selectorVisible = true;
    }
    m_script.Bind(entry.data);
    m_script.SetLevelContext(m_scene.GetLevel());
    m_script.Equip();
    m_script.Use(fromSelector);
    m_active = true;
    std::printf("[powerup-movie] start %s\n", entry.owner.c_str());
    if (!ApplyActions()) { Reset(); return false; }
    return true;
}

bool PowerupMoviePlayer::StartMovie(const PowerupAction &action) {
    const int packIndex = m_toc.GetPackIndexFromHash(action.resource.packHash);
    if (packIndex < 0) { return false; }
    if (m_renderers.count(packIndex) == 0) {
        auto renderer = std::make_unique<MovieRenderer>();
        if (!renderer->Init(*m_toc.GetPack(packIndex), *m_toc.GetPack(m_toc.GetCorePackIndex()))) { return false; }
        m_renderers[packIndex] = std::move(renderer);
    }
    m_renderer = m_renderers[packIndex].get();
    m_movieOrdinal = action.resource.localIndex;
    CMovie *movie = m_renderer->GetMovie(m_movieOrdinal);
    if (movie == nullptr) { return false; }
    m_movieDuration = movie->duration;
    m_movieTime = 0;
    m_movieActive = true;
    m_foregroundMovie = action.function == 15;
    m_loopMovie = action.function == 1 && action.arguments[1] == 1;
    std::printf("[powerup-movie] movie=%s:%u duration=%u loop=%d\n", m_toc.GetPack(packIndex)->GetShortName().c_str(),
        m_movieOrdinal, m_movieDuration, m_loopMovie);
    return true;
}

bool PowerupMoviePlayer::ApplyActions() {
    const auto actions = m_script.TakeActions();
    for (const PowerupAction &action : actions) {
        if (action.function == 1 || action.function == 15) {
            if (!StartMovie(action)) { ++failures; return false; }
        } else if (action.function == 2 || action.function == 4 || action.function == 5 || action.function == 13) {
            // Original selector/input-pad chapters finish over 300 ms; deliver
            // their events after presentation time, never from the Use call.
            m_callbackMs = 300;
            m_callbackEvent = 2;
            if (action.function == 4) { m_callbackEvent = 1; }
            if (action.function == 13) { m_callbackEvent = 4; }
            if ((action.function == 2 || action.function == 5) && m_selectorVisible) {
                // SetState(6/7) :185760 plays chapter 3, then Update :186710
                // emits OnSelectorHidden. Use the authored 100 ms, not 300.
                const CMovie *movie = m_selectorRenderer->GetMovie(m_selectorOrdinal);
                if (!movie->GetChapterRange(3, m_selectorTime, m_selectorEnd)) { ++failures; return false; }
                m_selectorClosing = true;
                m_callbackMs = static_cast<int>(m_selectorEnd - m_selectorTime);
            }
        } else if (action.function == 14 || action.function == 26) {
            // Selector-only input/mode controls are already inaccessible while
            // its powerup owns presentation. Neither native emits an event.
        } else if (action.function == 21) {
            // CPowerup native 21 calls OnRevive(1), distinct from rescue(0).
            if (!m_scene.ReviveActor(m_owner, 1)) { ++failures; return false; }
        } else if (action.function == 3) {
            CombatHit hit;
            hit.owner = m_owner;
            hit.ownerType = 0;
            // Native 3 reads CMap::CCamera's center (+9960/+9964), which
            // differs from the player near the camera bounds.
            hit.x = m_scene.GetViewCenterX();
            hit.y = m_scene.GetViewCenterY();
            hit.damage = action.arguments[0] * 10.0f;
            hit.splash = true;
            hit.flags = 2; // Original FireSplashDamageForceAttribute argument 7.
            hit.applyArmorAttack = false;
            // Native 3 passes radius second and damage first * 10. Both are
            // world units; target scripts still receive their splash callback.
            m_scene.Splash(hit, static_cast<float>(action.arguments[1]), 360, 0, 0);
            ++splashCount;
            std::printf("[powerup-movie] splash at=%u damage=%.0f radius=%d\n", m_elapsed, hit.damage, action.arguments[1]);
        } else if (action.function == 6 || action.function == 7) {
            float x = 0, y = 0;
            if (action.function == 7) {
                x = action.arguments[0] / 256.0f;
                y = action.arguments[1] / 256.0f;
            } else {
                m_random = m_random * 1664525u + 1013904223u;
                x = (m_random >> 8) / 16777216.0f;
                m_random = m_random * 1664525u + 1013904223u;
                y = (m_random >> 8) / 16777216.0f;
            }
            // Original maintains at most five simultaneous screen emitters.
            if (m_particles->GetEffectCount() < 5) {
                GunCue cue;
                cue.kind = GunCue::Kind::Effect;
                cue.resource = action.resource;
                m_particles->Emit(cue, x * 1024, y * 768, 0, 0);
                ++effectCount;
            }
        } else if (action.function == 9) {
            GunCue cue;
            cue.kind = GunCue::Kind::Sound;
            cue.resource = action.resource;
            m_particles->Emit(cue, 0, 0, 0, 0);
        } else {
            ++failures;
            std::printf("[powerup-movie] unhandled native=%u\n", action.function);
            return false;
        }
    }
    return true;
}

void PowerupMoviePlayer::Update(int deltaMs) {
    if (deltaMs <= 0) { return; }
    if (m_particles) { m_particles->AdvanceAmbientEffects(deltaMs); }
    if (!m_active) { return; }
    m_elapsed += deltaMs;
    if (m_selectorVisible) {
        m_selectorTime += static_cast<unsigned>(deltaMs);
        if (m_selectorClosing) {
            m_selectorTime = std::min(m_selectorTime, m_selectorEnd);
            if (m_selectorTime == m_selectorEnd) { m_selectorVisible = false; }
        } else if (m_selectorTime > m_selectorLoopEnd) {
            m_selectorTime = m_selectorLoopStart + (m_selectorTime - m_selectorLoopStart) %
                (m_selectorLoopEnd - m_selectorLoopStart + 1);
        }
    }
    if (m_callbackMs > 0) {
        m_callbackMs -= deltaMs;
        if (m_callbackMs <= 0) {
            m_script.HandleEvent(m_callbackEvent);
            if (!ApplyActions()) { Reset(); return; }
        }
    }
    if (m_movieActive) {
        m_movieTime += deltaMs;
        if (m_movieTime >= m_movieDuration) {
            if (m_loopMovie && m_movieDuration > 0) { m_movieTime %= m_movieDuration; }
            else {
                m_movieActive = false;
                ++movieCompletions;
                m_script.HandleEvent(0);
                if (!ApplyActions()) { Reset(); return; }
            }
        }
    }
    m_script.Update(deltaMs);
    if (!ApplyActions()) { Reset(); return; }
    if (m_script.IsDone()) {
        m_active = false;
        m_movieActive = false;
        m_selectorVisible = false;
        std::printf("[powerup-movie] complete elapsed=%u movies=%u splashes=%u\n", m_elapsed, movieCompletions, splashCount);
    }
}

bool PowerupMoviePlayer::Draw() {
    // This pass is also invoked by an isolated research harness before any HUD.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    if (m_particles) {
        float projection[16];
        Matrix4dOrthoTopLeft(1024, 768, 1, projection);
        m_particles->Draw(projection);
    }
    if (m_selectorVisible && !m_selectorRenderer->Draw(m_selectorOrdinal, m_selectorTime)) { return false; }
    if (m_movieActive && m_renderer != nullptr) { return m_renderer->Draw(m_movieOrdinal, m_movieTime); }
    return failures == 0;
}

void PowerupMoviePlayer::Reset() {
    if (m_active && m_scene.GetLevel() != nullptr) {
        // Cancel/restart must release the pause even without the script's exit.
        m_scene.GetLevel()->FunctionResolver(65, nullptr, 0);
    }
    m_active = false;
    m_movieActive = false;
    m_selectorVisible = false;
    m_selectorClosing = false;
    m_callbackMs = 0;
    m_elapsed = 0;
    if (m_particles) { m_particles->Clear(); }
}
