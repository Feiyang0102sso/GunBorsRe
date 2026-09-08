/** @file PowerupMoviePlayer.h
 * @brief Host for CPowerup movies, screen particles and completion callbacks.
 */
#ifndef GUN_BROS_RE_POWERUPMOVIEPLAYER_H
#define GUN_BROS_RE_POWERUPMOVIEPLAYER_H
#include "runtime/MovieRenderer.h"
#include "runtime/PowerupCatalog.h"
#include "runtime/CombatScene.h"

class PowerupMoviePlayer {
public:
    PowerupMoviePlayer(CResTOCManager &toc, PackTables &tables, CombatScene &scene);
    bool Start(const PowerupEntry &entry);
    void Update(int deltaMs);
    bool Draw();
    void Reset();
    bool IsActive() const { return m_active; }
    unsigned GetElapsed() const { return m_elapsed; }
    unsigned movieCompletions = 0, splashCount = 0, effectCount = 0, failures = 0;
private:
    bool ApplyActions();
    bool StartMovie(const PowerupAction &action);
    CResTOCManager &m_toc;
    PackTables &m_tables;
    CombatScene &m_scene;
    CPowerup m_script;
    CShaderProgram m_program;
    std::unique_ptr<WeaponEffects> m_particles;
    std::map<unsigned, std::unique_ptr<MovieRenderer>> m_renderers;
    MovieRenderer *m_renderer = nullptr;
    unsigned m_movieOrdinal = 0, m_movieTime = 0, m_movieDuration = 0, m_elapsed = 0;
    int m_callbackMs = 0;
    std::uint8_t m_callbackEvent = 0;
    bool m_active = false, m_movieActive = false, m_loopMovie = false;
    std::uint32_t m_random = 0xC0381125;
};
#endif
