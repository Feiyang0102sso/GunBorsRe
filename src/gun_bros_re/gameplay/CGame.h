/** @file CGame.h
 * @brief Original game-session owner coordinating CLevel and presentation.
 */
#ifndef GUN_BROS_RE_CGAME_H
#define GUN_BROS_RE_CGAME_H

#include "gun_bros_re/gameplay/level/CLevel.h"
#include "gun_bros_re/gameplay/CMPMatch.h"
#include "gun_bros_re/ui/CPowerUpSelector.h"
#include "gun_bros_re/gameplay/ZPropWorld.h"
#include <chrono>

class CInputPad;

/** Coordinates one active CLevel and the HUD/session state around it. */
class CGame {
public:
    CGame(CLevel &level, CMap &map, const std::vector<ZEnemyTemplateData> &catalog);
    bool Load(CResTOCManager &toc, ZPackTables &tables, std::uint32_t mapPack, unsigned mapIndex,
        const GameObjectRef *selectedLevel = nullptr, bool archive = false);
    void Restart(float x, float y, float facingDegrees);
    void SetHorde(bool enabled) {
        m_horde = enabled;
        m_archive = false;
        m_level.SetArchive(false);
        m_level.SetHorde(enabled);
    }
    void SetStartWave(int wave) { m_startWave = wave; }
    void SetDeathmatch(CMPMatch *match) { m_match = match; m_level.SetMatch(match); }
    void SetDialogHud(CInputPad *hud) { m_dialogHud = hud; }
    void SetChallenges(CChallengeManager *manager, CProfileManager *profile,
        const std::vector<ZWeaponEntry> *weapons) {
        m_challenges = manager;
        m_challengeProfile = profile;
        m_challengeWeapons = weapons;
    }
    bool SubmitChallenges(bool ended, bool waveCleared = false);
    void SetHud(CInputPad *hud) { m_hud = hud; }
    bool HasHud() const { return m_hud != nullptr; }
    void Update(int deltaMs, float moveX, float moveY, bool fire);
    void SetSuspended(bool suspended) { m_suspended = suspended; }
    void UpdateAfterDeath(int deltaMs);
    bool IsDeathComplete() const { return m_level.IsDeathComplete(); }
    bool IsFinished() const {
        if (m_match != nullptr) { return m_match->GetResult() != CMPMatch::Result::Playing; }
        return m_level.IsCleared() || IsDeathComplete();
    }
    bool IsReadyForResults() const;
    bool IsDeathmatchFading() const { return m_matchFading; }
    bool SkipToBoss();
    bool StartBossSkip();
    void AdvanceBossSkip();
    bool IsBossSkipActive() const { return m_bossSkipActive; }

    void SetProps(ZPropWorld *props) { m_level.SetProps(props); }
    CLevel &GetLevel() { return m_level; }
    const CLevel &GetLevel() const { return m_level; }
    int CountEnemies(const GameObjectRef *enemy = nullptr, int objectId = -1) const {
        return m_level.CountEnemies(enemy, objectId);
    }
    void OnWaveCleared(unsigned perfectRewardPercent);
    bool IsTransitioning() const;
    unsigned GetTransitionElapsed() const;
    unsigned GetKills() const { return m_level.GetKills(); }
    float GetClosestSpawnDistance() const { return m_level.GetClosestSpawnDistance(); }
    unsigned GetOnScreenSpawns() const { return m_level.GetOnScreenSpawns(); }
    void SetViewSize(float width, float height) { m_level.SetViewSize(width, height); }
    void SetScriptRandomSeed(std::uint32_t seed) { m_scriptRandomSeed = seed; m_hasScriptRandomSeed = true; }
    const std::string &GetDialogText() const { return m_dialogText; }
    void CompleteDialog();
    unsigned GetPowerupCount(unsigned localIndex) const { return m_level.GetPowerupCount(localIndex); }

private:
    void FinishBossSkip();
    void UpdateDialog(int deltaMs);

    bool m_bossSkipActive = false;
    unsigned m_bossSkipIntroSerial = 0;
    unsigned m_bossSkipDefeated = 0;
    int m_bossSkipElapsedMs = 0;
    std::int16_t m_bossSkipPreviousFlag = 0;
    std::chrono::steady_clock::time_point m_bossSkipStarted;
    CMPMatch *m_match = nullptr;
    // Keep the completed BIG death presentation visible before the result fade.
    static constexpr int MatchEndingHoldMs = 800;
    int m_matchEndingHoldMs = 0;
    bool m_matchFading = false;
    bool m_suspended = false;
    CChallengeManager *m_challenges = nullptr;
    CProfileManager *m_challengeProfile = nullptr;
    const std::vector<ZWeaponEntry> *m_challengeWeapons = nullptr;
    GameObjectRef m_levelReference;
    bool m_challengeSessionEnded = false;
    CLevel::Template m_template;
    CLevel &m_level;
    CMap &m_map;
    int m_transitionMs = 1200;
    int m_transitionDuration = 1200;
    unsigned m_bossIntroSerial = 0;
    int m_startWave = 0;
    bool m_archive = false;
    bool m_horde = false;
    CInputPad *m_dialogHud = nullptr;
    CInputPad *m_hud = nullptr;
    bool m_bossWave = false;
    std::uint32_t m_scriptRandomSeed = 0;
    bool m_hasScriptRandomSeed = false;
    CResTOCManager *m_toc = nullptr;
    std::string m_dialogText;
    bool m_dialogBound = false;
    unsigned m_dialogSerial = 0;
};

#endif
