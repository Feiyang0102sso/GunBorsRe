/** @file CMenuUpgradePopup.h
 * @brief Original upgrade popup playback; rendering and input stay in the host.
 * Sources: CMenuUpgradePopup::Bind/Update/Hide/PerformUpgrade :393038-393802.
 * Chapter boundaries and gun XP thresholds are supplied by BIG resources.
 */
#ifndef GUN_BROS_RE_CMENUUPGRADEPOPUP_H
#define GUN_BROS_RE_CMENUUPGRADEPOPUP_H
#include "engine/glu/movie/CMovie.h"
#include "gun_bros_re/gameplay/weapon/CGun.h"
#include <array>

class CMenuUpgradePopup {
public:
    enum class State { Opening, Ready, Upgrading, Flash, Closing, Closed };
    bool Bind(const CMovie &popup, const CMovie &stars, const CGun::Template &gun, unsigned experience);
    void Update(unsigned deltaMs);
    bool PerformUpgrade(const CMovie &stars, const CGun::Template &gun, unsigned experience);
    void Hide();
    bool SelectGun(const CMovie &stars, const CGun::Template &gun, unsigned experience);
    static bool StarsTarget(const CMovie &stars, const CGun::Template &gun, unsigned experience, unsigned &target);
    State GetState() const { return m_state; }
    unsigned MovieTime() const { return m_time; }
    unsigned StarsTime() const { return m_starsTime; }
    unsigned TargetTime() const { return m_target; }
    unsigned DisplayExperience() const { return m_displayExperience; }
    unsigned BackdropAlpha() const;
    unsigned FlashAlpha() const;
    bool IsBound() const { return m_bound; }
private:
    struct Chapter { unsigned start = 0, end = 0; };
    std::array<Chapter, 3> m_chapters;
    State m_state = State::Closed;
    unsigned m_time = 0, m_starsTime = 0, m_target = 0, m_flashTime = 0;
    unsigned m_displayExperience = 0, m_pendingExperience = 0, m_pendingLevel = 0;
    bool m_bound = false;
};
#endif
