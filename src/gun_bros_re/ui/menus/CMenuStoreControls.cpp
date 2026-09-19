#include "gun_bros_re/ui/host/ZStoreRegionClip.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
namespace MenuDetail {

/** The four category tabs. Their widths come from the button movie each
 * MDS_BUTTON_STORE_CATEGORIES row names, not from measured screenshots. */
void DrawStoreCategories(ZMenuSurface &view, const ZMovieRegion &bar, CMenuSystem &state, bool interactive) {
    // The focus overlays belong to the same button movies: small 75, medium 76,
    // large 77 and extra large 78, in the same order as the movie ordinals.
    // The actual focus art is in each button's chapter 3 (:144639).
    float x = bar.x;
    for (unsigned category = 0; category < 4; ++category) {
        const CMenuDataProvider::Entry *entry = CMenuDataProvider::Find("MDS_BUTTON_STORE_CATEGORIES", category);
        if (entry == nullptr) { continue; }
        const unsigned plate = view.movies.Ordinal(entry->movies[0]);
        ZMovieRegion touch, label;
        if (!view.movies.Region(plate, 0, 0, touch) || !view.movies.Region(plate, 1, 0, label)) { continue; }
        const float width = label.width;
        const float touchX = x + touch.x - label.x;
        const float touchY = bar.y + touch.y - label.y;
        const bool selected = category == state.store.shopCategory;
        unsigned sprite = entry->sprites[1];
        // The bank tab keeps its green plate whether or not it is selected.
        if (selected || category == 3) { sprite = entry->sprites[0]; }
        view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, x, bar.y, width, bar.height);
        if (selected) {
            const CMovie *buttonMovie = view.movies.GetMovie(plate);
            unsigned focusStart = 0, focusEnd = 0;
            if (buttonMovie != nullptr && buttonMovie->GetChapterRange(3, focusStart, focusEnd)) {
                view.movies.DrawFitted(plate, focusEnd, x, bar.y, width, label.height, 1);
            }
        }
        PlateLabel(view, view.movies.NamedString(entry->strings[0]), x, bar.y, width, bar.height);
        if (interactive && view.Hit(touchX, touchY, touch.width, touch.height)) {
            view.NotePress(plate, x, bar.y, width, bar.height);
            {
                state.store.CancelButtons();
                state.store.shopCategory = category;
                if (state.stack.page == 17) { state.Navigate(2); }
                state.store.shopScroll = 0; state.store.shopMotion = ZMenuScrollMotion{};
                state.store.shopFilter = 0; state.store.filterAll = true; state.store.shopExclusionFilter = 0;
                state.selectedItem = -1;
                state.store.focused.shopDetailOpen = false;
            }
        }
        x += width + kCategoryGap;
    }
}

/** CMenuStore::InitSortButtons binds every row in the selected MDS table. */
unsigned StoreFilterRows(unsigned category, const char *&table) {
    table = "MDS_BUTTON_STORE_SORT_GUNS";
    if (category == 1) { table = "MDS_BUTTON_STORE_SORT_ARMOR"; }
    if (category == 2) { table = "MDS_BUTTON_STORE_SORT_POWERUP"; }
    if (category == 3) { table = "MDS_BUTTON_STORE_SORT_CURRENCY"; }
    unsigned count = 0;
    while (CMenuDataProvider::Find(table, count)) { ++count; }
    return count;
}

/** Non-looping SORT_BAR playback, CMenuStore::Bind :180092 and
 * HandleTouchInput :179259. Chapter 0 holds the initial closed pose; clicks
 * play chapter 1 forward or backward without restarting the current frame.
 * Bounds come from BIG ui_movie.bt / MovieChapter, never copied timestamps.
 * CMovie::Update :109097 advances milliseconds and clamps at chapter bounds. */
bool AdvanceStoreFilter(ZMenuSurface &view, CMenuSystem &state, const CMovie &movie) {
    unsigned closedStart = 0, closedEnd = 0, slideStart = 0, slideEnd = 0;
    if (!movie.GetChapterRange(0, closedStart, closedEnd) ||
        !movie.GetChapterRange(1, slideStart, slideEnd)) {
        std::printf("[store-filter] missing playback chapters\n");
        return false;
    }
    if (!state.store.shopFilterBound) {
        state.store.shopFilterTime = closedEnd;
        if (state.store.shopFilterOpen) { state.store.shopFilterTime = slideEnd; }
        state.store.shopFilterLastTick = view.clock;
        state.store.shopFilterBound = true;
        std::printf("[store-filter] BIG chapters closed=%u..%u slide=%u..%u\n",
            closedStart, closedEnd, slideStart, slideEnd);
        return true;
    }
    const std::uint64_t elapsed = view.clock - state.store.shopFilterLastTick;
    state.store.shopFilterLastTick = view.clock;
    if (state.store.shopFilterOpen) {
        state.store.shopFilterTime += static_cast<unsigned>(std::min<std::uint64_t>(elapsed, slideEnd - state.store.shopFilterTime));
    } else if (state.store.shopFilterTime > slideStart) {
        state.store.shopFilterTime -= static_cast<unsigned>(std::min<std::uint64_t>(elapsed, state.store.shopFilterTime - slideStart));
    }
    return true;
}

/** CMenuStore::GunSwapCallback :178863; button size comes from Movie region 1. */
bool StoreGunSwapOrigin(ZMenuSurface &view, const ZMovieRegion &parent, ZMovieRegion &origin) {
    const CMenuDataProvider::Entry *entry = CMenuDataProvider::Find("MDS_BUTTON_STORE_GUN_SWAP", 0);
    ZMovieRegion graphic;
    if (entry == nullptr || !view.movies.Region(view.movies.Ordinal(entry->movies[0]), 1, 0, graphic)) { return false; }
    origin = parent;
    origin.x = parent.x + parent.width - graphic.width;
    origin.y = parent.y + static_cast<int>(parent.height) / 2;
    return true;
}

/** Original button chapters own appearance, press completion and category hide. */
bool DrawStoreGunSwap(ZMenuSurface &view, CMenuSystem &state, const ZMovieRegion &parent, bool interactive) {
    const CMenuDataProvider::Entry *entry = CMenuDataProvider::Find("MDS_BUTTON_STORE_GUN_SWAP", 0);
    if (entry == nullptr) { return false; }
    const CMovie *movie = view.movies.GetMovie(view.movies.Ordinal(entry->movies[0]));
    unsigned showStart = 0, showEnd = 0, pressStart = 0, pressEnd = 0, idleStart = 0, idleEnd = 0;
    if (movie == nullptr || !movie->GetChapterRange(0, showStart, showEnd) ||
        !movie->GetChapterRange(1, pressStart, pressEnd) || !movie->GetChapterRange(2, idleStart, idleEnd)) { return false; }
    unsigned delta = 0;
    if (state.store.shopSwapLastTick != 0 && view.clock >= state.store.shopSwapLastTick) {
        delta = static_cast<unsigned>(view.clock - state.store.shopSwapLastTick);
    }
    state.store.shopSwapLastTick = view.clock;
    // CMenuStore::RefreshCategoryContent :179165 tests category, not filter bits.
    const bool visible = state.store.shopCategory == 0;
    if (visible && state.store.shopSwapPhase == 8) {
        state.store.shopSwapPhase = 0;
        state.store.shopSwapTime = showStart;
        delta = 0;
    } else if (!visible && state.store.shopSwapPhase != 1 && state.store.shopSwapPhase != 8) {
        state.store.shopSwapPhase = 1;
        state.store.shopSwapTime = showEnd;
        delta = 0;
    } else if (visible && state.store.shopSwapPhase == 1) {
        state.store.shopSwapPhase = 0;
        state.store.shopSwapTime = showStart;
        delta = 0;
    }
    if (state.store.shopSwapPhase == 0 || state.store.shopSwapPhase == 4) {
        unsigned end = showEnd;
        if (state.store.shopSwapPhase == 4) { end = pressEnd; }
        state.store.shopSwapTime = std::min(end, state.store.shopSwapTime + delta);
        if (state.store.shopSwapTime == end) {
            if (state.store.shopSwapPhase == 4) {
                // CMenuMovieButton::Update :144755 dispatches action 92 only
                // after chapter 1 finishes; PLAYER Flow then owns the swap.
                state.store.shopGunSlot = 1 - view.playerPreview.GetPlayerPreviewSlot();
                state.store.focused.shopDetailOpen = false;
                state.store.focused.shopPreview = false;
            }
            state.store.shopSwapPhase = 2;
            state.store.shopSwapTime = idleStart;
        }
    } else if (state.store.shopSwapPhase == 1) {
        if (delta >= state.store.shopSwapTime - showStart) { state.store.shopSwapPhase = 8; }
        else { state.store.shopSwapTime -= delta; }
    } else if (state.store.shopSwapPhase == 2) {
        state.store.shopSwapTime = idleStart + (state.store.shopSwapTime - idleStart + delta) % (idleEnd - idleStart + 1);
    }
    bool keyPressed = state.store.shopSwapKeyRequested;
    state.store.shopSwapKeyRequested = false;
    if (state.store.shopSwapPhase == 8) { return true; }
    ZMovieRegion origin;
    if (!StoreGunSwapOrigin(view, parent, origin)) { return false; }
    bool pressed = false;
    const bool enabled = interactive && state.store.shopSwapPhase == 2 && state.store.shopGunSlot == view.playerPreview.GetPlayerPreviewSlot();
    if (!CMenuMovieButton::DrawFrame(view, *entry, origin, std::to_string(view.playerPreview.GetPlayerPreviewSlot() + 1), 6,
        enabled, pressed, 0, 0, state.store.shopSwapTime)) { return false; }
    if (enabled && (pressed || keyPressed)) {
        state.store.shopSwapPhase = 4;
        state.store.shopSwapTime = pressStart;
    }
    return true;
}
}
