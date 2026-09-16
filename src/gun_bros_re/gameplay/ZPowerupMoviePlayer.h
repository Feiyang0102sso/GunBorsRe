/** @file ZPowerupMoviePlayer.h
 * @brief Host for CPowerup movies, screen particles and completion callbacks.
 */
#ifndef GUN_BROS_RE_ZPOWERUPMOVIEPLAYER_H
#define GUN_BROS_RE_ZPOWERUPMOVIEPLAYER_H
#include "engine/glu/movie/ZMovieRenderer.h"
#include "gun_bros_re/data/ZPowerupCatalog.h"
#include "gun_bros_re/gameplay/ZCombatWorld.h"

class ZPowerupMoviePlayer {
public:
    ZPowerupMoviePlayer(CResTOCManager &toc, ZPackTables &tables, ZCombatWorld &scene);
    bool Start(const ZPowerupEntry &entry, bool fromSelector = false);
    void SetOwner(ZCombatId owner) { m_owner = owner; }
    void Update(int deltaMs);
    bool Draw();
    void Reset();
    bool IsActive() const { return m_active; }
    unsigned GetElapsed() const { return m_elapsed; }
    bool IsForegroundMovie() const { return m_movieActive && m_foregroundMovie; }
    bool HasSelectorFrame() const { return m_selectorVisible; }
    bool IsSelectorFrameClosing() const { return m_selectorVisible && m_selectorClosing; }
    unsigned movieCompletions = 0, splashCount = 0, effectCount = 0, failures = 0;
private:
    bool ApplyActions();
    bool StartMovie(const ZPowerupAction &action);
    CResTOCManager &m_toc;
    ZPackTables &m_tables;
    ZCombatWorld &m_scene;
    ZCombatId m_owner = kPlayerCombatId;
    CPowerup m_script;
    ZShaderProgram m_program;
    std::unique_ptr<ZWeaponEffects> m_particles;
    std::map<unsigned, std::unique_ptr<ZMovieRenderer>> m_renderers;
    ZMovieRenderer *m_renderer = nullptr;
    std::unique_ptr<ZMovieRenderer> m_selectorRenderer;
    unsigned m_selectorOrdinal = 0, m_selectorTime = 0;
    unsigned m_selectorLoopStart = 0, m_selectorLoopEnd = 0, m_selectorEnd = 0;
    bool m_selectorVisible = false, m_selectorClosing = false;
    unsigned m_movieOrdinal = 0, m_movieTime = 0, m_movieDuration = 0, m_elapsed = 0;
    int m_callbackMs = 0;
    std::uint8_t m_callbackEvent = 0;
    bool m_active = false, m_movieActive = false, m_loopMovie = false;
    bool m_foregroundMovie = false;
    std::uint32_t m_random = 0xC0381125;
};
#endif
