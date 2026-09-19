/** @file CMenuUpgradePopup.h
 * @brief Original upgrade popup playback; rendering and input stay in the host.
 * Sources: CMenuUpgradePopup::Bind/Update/Hide/PerformUpgrade :393038-393802.
 * Chapter boundaries and gun XP thresholds are supplied by BIG resources.
 */
#ifndef GUN_BROS_RE_CMENUUPGRADEPOPUP_H
#define GUN_BROS_RE_CMENUUPGRADEPOPUP_H
#include "engine/glu/movie/CMovie.h"
#include "gun_bros_re/ui/host/ZMenuTypes.h"
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

namespace MenuDetail {
class CMenuSystem;
class ZMenuSurface;

const ZWeaponEntry *FindMasteryWeapon(const std::vector<ZWeaponEntry> &weapons, const GameObjectRef &ref);

const ZStoreEntry *FindWeaponStore(const std::vector<ZStoreEntry> &store, const GameObjectRef &ref);

// GLU_MOVIE_UPGRADE_POPUP regions, in the order the movie declares them:
// portrait, close, meter, CURRENT and NEXT headers, the two stat columns, the
// weapon icon plate, the title bar, the buy tab and the weapon name strip.
constexpr unsigned kUpgradePortraitRegion = 0;
constexpr unsigned kUpgradeCloseRegion = 1;
constexpr unsigned kUpgradeMeterRegion = 2;
constexpr unsigned kUpgradeCurrentHeaderRegion = 3;
constexpr unsigned kUpgradeNextHeaderRegion = 4;
constexpr unsigned kUpgradeCurrentColumnRegion = 5;
constexpr unsigned kUpgradeNextColumnRegion = 6;
constexpr unsigned kUpgradeIconRegion = 7;
constexpr unsigned kUpgradeTitleRegion = 8;
constexpr unsigned kUpgradeBuyRegion = 9;
constexpr unsigned kUpgradeNameRegion = 11;
// The player headshots the menus print next to a title.
constexpr unsigned kBrotherPortrait = 161;
// The old fixed fill interval is replaced by CMenuUpgradePopup's original 1x playback.

/** The upgrade popup is reached from the store as well as from the results,
 * so closing it returns to whichever page pushed it. */
void CloseMastery(CMenuSystem &state);

/** How far into GLU_MOVIE_WEAPON_UPGRADE_MASTERY the meter stands for this
 * much experience. The movie's chapters are the three cells. */
unsigned MasteryMeterTime(ZMenuSurface &view, const ZWeaponEntry &weapon, unsigned experience);

/** Bind CMenuMovieButton's original region 1 graphic/label and region 0 hit box. */
bool DrawUpgradeButton(ZMenuSurface &view, unsigned index, const ZMovieRegion &area,
    const std::string &label, unsigned font, bool interactive, bool &pressed);

/** Original callbacks use each MovieRegion and bitmap font without fitting. */
void UpgradeCenteredText(ZMenuSurface &view, const ZMovieRegion &region, const std::string &text, unsigned font);

/** CMenuUpgradePopup::DrawBodyText :392576: only changed stats, then CRIT.
 * CURRENT is the absolute STORE value; NEXT is the relative percentage change. */
void DrawUpgradeStats(ZMenuSurface &view, const ZMovieRegion &area, const CStoreItem &item, unsigned level, bool next);

/** The original popup advances its own movie and stars through six states.
 * All geometry, fonts, item values, chapter times and art are read from BIG. */
bool DrawMastery(ZMenuSurface &view, CMenuSystem &state, CProfileManager &profile, CResTOCManager &toc,
    ZPackTables &tables, const std::vector<ZStoreEntry> &store, const std::vector<ZWeaponEntry> &weapons,
    const std::filesystem::path &savePath, CPlayerProgress *headerProgress = nullptr);

}
#endif
