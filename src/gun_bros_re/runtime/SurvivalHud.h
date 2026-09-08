/** @file SurvivalHud.h
 * @brief Original CInputPad artwork bound to the desktop survival state.
 */
#ifndef GUN_BROS_RE_SURVIVALHUD_H
#define GUN_BROS_RE_SURVIVALHUD_H
#include "runtime/MovieRenderer.h"
#include "runtime/StoreCatalog.h"
#include "gun_bros/CLevelIndicator.h"

enum class SurvivalHudAction { None, Pause, Resume, Retry, Exit, Weapon1, Weapon2, UseItem, NextItem, Continue };

struct SurvivalHudState {
    float health = 0, maximumHealth = 1, brotherHealth = 0, brotherMaximumHealth = 1;
    unsigned level = 1, wave = 0, enemies = 0, kills = 0, weaponSlot = 0, itemCount = 0;
    std::uint64_t experience = 0, experienceDelta = 1, xplodium = 0, perfectBonus = 0;
    unsigned transitionTime = 0;
    unsigned stopwatchMs = 0;
    unsigned bossIntroSerial = 0;
    int xplodiumMultiplier = 100;
    bool horde = false;
    unsigned score = 0, killStreak = 0;
    bool paused = false, dead = false, cleared = false, transitioning = false, withBrother = false;
    std::string weapon, item, buffs, dialog, mission;
    std::string brotherName;
    float brotherLabelX = 0, brotherLabelY = 0, brotherLabelAlpha = 0;
    GameObjectRef guns[2], powerup;
    std::vector<CLevelIndicator> indicators;
};

/** The same rectangles drive drawing and pointer input; no gameplay is owned here. */
class SurvivalHud {
public:
    bool Init(CResTOCManager &toc, PackTables &tables);
    bool Draw(const SurvivalHudState &state);
    void Advance(int deltaMs);
    void ResetNotices();
    unsigned NoticeCount() const { return static_cast<unsigned>(m_notices.size()); }
    unsigned NoticeTime() const { if (m_notices.empty()) { return 0; } return m_notices.front().elapsed; }
    SurvivalHudAction Pointer(const SurvivalHudState &state, float x, float y, bool down);
    bool CapturesPointer(const SurvivalHudState &state, float x, float y) const;
private:
    struct Button { MovieRegion rect; SurvivalHudAction action; const char *label; };
    std::vector<Button> Buttons(const SurvivalHudState &state) const;
    void Centre(const std::string &text, float y, unsigned font = 0, float scale = 1);
    void Icon(unsigned type, const GameObjectRef &object, const MovieRegion &region);
    void ObserveProgress(const SurvivalHudState &state);
    void DrawNotice();
    struct Notice { unsigned movie, elapsed; std::string title, footer; };
    std::vector<Notice> m_notices;
    unsigned m_previousLevel = 0, m_previousWave = 0;
    unsigned m_previousBossIntro = 0;
    bool m_observedProgress = false;
    MovieRenderer m_movies;
    PackTables *m_tables = nullptr;
    std::vector<StoreEntry> m_store;
    std::map<std::uint64_t, std::unique_ptr<CTexture>> m_icons;
    bool m_previousDown = false;
    float m_mouseX = -1, m_mouseY = -1;
};

int RunSurvivalHudCheck(const std::string &bigDirectory);
#endif
