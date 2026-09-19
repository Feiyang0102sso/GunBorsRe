#pragma once
#include "gun_bros_re/ui/host/ZMenuTypes.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuStoreOptionGroup.h"
#include "gun_bros_re/ui/controls/ZMenuScrollMotion.h"
#include "gun_bros_re/data/CProfileManager.h"

namespace MenuDetail {
class CMenuSystem;
class ZMenuSurface;
class CMenuStore {
public:
    void CancelButtons() { options.buttons.clear(); focused.previewButton.Cancel(); }
    /** The original store draws two cards per column (ItemCallback :178878) on a
     * horizontal belt and expands the focused card in place. Item identity and
     * purchases still come directly from the BIG catalog. */
    bool Draw(ZMenuSurface &view, CResTOCManager &toc, ZPackTables &tables, CProfileManager &profile,
        unsigned level, const std::vector<ZStoreEntry> &store, const std::vector<ZWeaponEntry> &weapons,
        const std::vector<ZArmorEntry> &armors, CMenuSystem &state, const std::filesystem::path &savePath);

    unsigned shopCategory = 0;
    unsigned shopGunSlot = 0;
    // CMenuMovieButton states: showing 0, hiding 1, idle 2, selected 4, hidden 8.
    unsigned shopSwapPhase = 8, shopSwapTime = 0;
    std::uint64_t shopSwapLastTick = 0;
    bool shopSwapKeyRequested = false;
    float shopScroll = 0;
    ZMenuScrollMotion shopMotion;
    unsigned shopFilter = 0;
    // ALL and an explicitly empty selection have different native masks.
    bool filterAll = true;
    unsigned shopExclusionFilter = 0;
    bool shopFilterOpen = false;
    // Each menu owns its playback cursor; cached CMovie resources stay immutable.
    unsigned shopFilterTime = 0;
    std::uint64_t shopFilterLastTick = 0;
    bool shopFilterBound = false;
    CMenuStoreOption focused;
    CMenuStoreOptionGroup options;

};

/** The four category tabs. Their widths come from the button movie each
 * MDS_BUTTON_STORE_CATEGORIES row names, not from measured screenshots. */
void DrawStoreCategories(ZMenuSurface &view, const ZMovieRegion &bar, CMenuSystem &state, bool interactive);

/** CMenuStore::InitSortButtons binds every row in the selected MDS table. */
unsigned StoreFilterRows(unsigned category, const char *&table);

/** Non-looping SORT_BAR playback, CMenuStore::Bind :180092 and
 * HandleTouchInput :179259. Chapter 0 holds the initial closed pose; clicks
 * play chapter 1 forward or backward without restarting the current frame.
 * Bounds come from BIG ui_movie.bt / MovieChapter, never copied timestamps.
 * CMovie::Update :109097 advances milliseconds and clamps at chapter bounds. */
bool AdvanceStoreFilter(ZMenuSurface &view, CMenuSystem &state, const CMovie &movie);

/** CMenuStore::GunSwapCallback :178863; button size comes from Movie region 1. */
bool StoreGunSwapOrigin(ZMenuSurface &view, const ZMovieRegion &parent, ZMovieRegion &origin);

/** Original button chapters own appearance, press completion and category hide. */
bool DrawStoreGunSwap(ZMenuSurface &view, CMenuSystem &state, const ZMovieRegion &parent, bool interactive);

// GLU_MOVIE_STORE_MENU carries the whole screen skeleton. Its four user
// regions are bound, in this order, by CMenuStore::Init (:180199) to the
// content list, the category row, the player mesh and the gun swap button.
constexpr unsigned kStoreContentRegion = 0;
constexpr unsigned kStoreCategoryRegion = 1;
constexpr unsigned kStorePlayerRegion = 2;
constexpr unsigned kStoreGunSwapRegion = 3;
// CMenuStore::CategoryCallback places each category button four pixels after
// the previous one, starting at the left edge of the category region.
constexpr float kCategoryGap = 4;
// GLU_MOVIE_STORE_SCROLL holds five card columns. Slot 1 is the leftmost one
// on screen and every further slot is one column to the right; the slots that
// reach past the player already carry the original half transparency.
constexpr unsigned kFirstColumnRegion = 1;

// GLU_MOVIE_SORT_BAR: CMenuStore::Init :180331 binds region 1 to
// SortButtonCallback and region 2 to SortLabelCallback; region 0 is the touch area.
constexpr unsigned kSortButtonRegion = 0;
constexpr unsigned kSortPanelRegion = 1;
constexpr unsigned kSortLabelRegion = 2;
// CMenuStore::SortButtonCallback stacks the options at 1.5 button heights.
constexpr float kSortRowSpacing = 1.5f;
}
