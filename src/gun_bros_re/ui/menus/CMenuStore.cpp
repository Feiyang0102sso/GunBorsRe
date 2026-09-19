#include "gun_bros_re/ui/host/ZStorePurchase.h"
#include "gun_bros_re/ui/host/ZStoreRegionClip.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
namespace MenuDetail {

// Page callback implementations.

bool CMenuStore::Draw(ZMenuSurface &view, CResTOCManager &toc, ZPackTables &tables, CProfileManager &profile,
    unsigned level, const std::vector<ZStoreEntry> &store, const std::vector<ZWeaponEntry> &weapons,
    const std::vector<ZArmorEntry> &armors, CMenuSystem &state, const std::filesystem::path &savePath) {
    const unsigned storeMenu = view.movies.Ordinal("GLU_MOVIE_STORE_MENU");
    const unsigned storeScroll = view.movies.Ordinal("GLU_MOVIE_STORE_SCROLL");
    const unsigned shopBox = view.movies.Ordinal("GLU_MOVIE_SHOP_BOX");
    const unsigned sortBar = view.movies.Ordinal("GLU_MOVIE_SORT_BAR");
    const CMovie *cardMovie = view.movies.GetMovie(shopBox);
    unsigned cardStart = 0, cardEnd = 0, openStart = 0, openEnd = 0;
    if (cardMovie == nullptr || !cardMovie->GetChapterRange(1, cardStart, cardEnd) ||
        !cardMovie->GetChapterRange(2, openStart, openEnd) || !state.store.focused.Update(view.clock, *cardMovie)) { return false; }
    const CMovie *filterMovie = view.movies.GetMovie(sortBar);
    if (filterMovie == nullptr || !AdvanceStoreFilter(view, state, *filterMovie)) { return false; }
    ZMovieRegion content, categoryBar, playerPanel, gunSwap;
    if (!RequireRegion(view, storeMenu, kStoreContentRegion, 0, content, "content") ||
        !RequireRegion(view, storeMenu, kStoreCategoryRegion, 0, categoryBar, "categories") ||
        !RequireRegion(view, storeMenu, kStorePlayerRegion, 0, playerPanel, "player") ||
        !RequireRegion(view, storeMenu, kStoreGunSwapRegion, 0, gunSwap, "gun swap")) { return false; }
    // CMenuStore::Draw :179478 draws STORE_MENU itself before its children.
    // Its type-7 object supplies the authored fill; never synthesize a rectangle.
    if (!view.movies.Draw(storeMenu, 0)) { return false; }
    const bool modalOpen = state.store.focused.shopDetailOpen || state.store.shopFilterOpen || state.currencyPending ||
        state.storePromptRequested || state.storePopup.IsActive() || state.promotion.IsActive();
    DrawStoreCategories(view, categoryBar, state, !modalOpen);

    CStoreAggregator::InitFilteredList(store, profile, weapons, armors,
        shopCategory, shopGunSlot, shopFilter, filterAll, shopExclusionFilter, options.items, options.itemSlots);
    const auto &items = options.items;
    const auto &itemSlots = options.itemSlots;
    const unsigned columns = static_cast<unsigned>((items.size() + 1) / 2);

    // The belt scrolls by whole columns; drag and wheel move the same pixel
    // offset and it settles back onto a column once the button is released.
    ZMovieRegion firstSlot, secondSlot, viewport;
    if (!RequireRegion(view, storeScroll, kFirstColumnRegion, view.storeRestTime, firstSlot, "first column") ||
        !RequireRegion(view, storeScroll, kFirstColumnRegion + 1, view.storeRestTime, secondSlot, "second column") ||
        !RequireRegion(view, storeScroll, 0, view.storeRestTime, viewport, "belt viewport")) { return false; }
    const float columnPitch = std::max(1.0f, secondSlot.x - firstSlot.x);
    const float maximumScroll = std::max(0.0f, (columns - 1.0f) * columnPitch);
    unsigned scrollStart = 0, scrollEnd = 0, nextScrollStart = 0, nextScrollEnd = 0;
    const CMovie *scrollMovie = view.movies.GetMovie(storeScroll);
    if (scrollMovie == nullptr || !scrollMovie->GetChapterRange(1, scrollStart, scrollEnd) ||
        !scrollMovie->GetChapterRange(2, nextScrollStart, nextScrollEnd)) { return false; }
    const float previousScroll = shopScroll;
    view.Scroll(shopMotion, shopScroll, viewport, !modalOpen, maximumScroll, columnPitch, nextScrollStart - scrollStart);
    if (shopScroll != previousScroll) { CancelButtons(); }

    int purchaseIndex = -1;
    unsigned purchaseSlot = 0;
    int focusedColumn = -1, focusedRow = 0;
    const unsigned firstColumn = static_cast<unsigned>(std::max(0.0f, std::floor(state.store.shopScroll / columnPitch)));
    // CMenuStore::ItemCallback :178878 places both rows inside the scroll
    // control. Its input must cover the same authored viewport as drawing.
    const bool listHover = view.MouseIn(content.x, viewport.y, content.width, viewport.height);
    // The belt has its own viewport; the content region alone cuts the second row.
    // STORE_SCROLL region 0 is the control's input rectangle, not a vertical
    // drawing clip. CMenuStore::ItemCallback :178878 and CMovieRegion::Draw
    // :109978 allow corner sprites outside it. Clip only horizontally; the
    // host framebuffer supplies the vertical boundary, just as for CMovie.
    view.Clip(content.x, 0, content.width, kMenuHeight);
    for (const auto &slot : view.movies.Regions(storeScroll, view.storeRestTime)) {
        if (slot.index < kFirstColumnRegion) { continue; }
        const unsigned column = firstColumn + slot.index - kFirstColumnRegion;
        if (column >= columns) { continue; }
        const float slotX = firstSlot.x + column * columnPitch - state.store.shopScroll;
        for (unsigned row = 0; row < 2; ++row) {
            const unsigned position = column * 2 + row;
            if (position >= items.size()) { break; }
            const unsigned index = items[position];
            const unsigned slotKind = itemSlots[position];
            if (state.store.focused.shopDetailOpen && state.selectedItem == static_cast<int>(index)) {
                focusedColumn = static_cast<int>(column);
                focusedRow = static_cast<int>(row);
                continue;
            }
            CMenuStoreOption::Face face;
            face.x = slotX;
            // ItemCallback stacks the second card at half the slot height plus five.
            face.y = slot.y + row * (slot.height / 2 + 5);
            face.alpha = slot.alpha;
            ZMovieRegion body;
            if (!CardRegion(view, shopBox, kCardBodyRegion, face, body)) { continue; }
            const bool cardEnabled = !modalOpen && listHover;
            // Input may leave the belt while the selected button finishes.
            // Native Update is independent from HandleTouchInput hover tests.
            const bool actionEnabled = !modalOpen && (listHover || options.buttons[index].IsSelected());
            if (index >= store.size()) {
                // The invite friends card art, then the free Warbucks entry.
                view.movies.Draw(shopBox, face.time, face.x, face.y, 1024, 768, 0, face.alpha);
                unsigned promoSprite = kInviteCard;
                if (index != store.size()) { promoSprite = kFreeWarbucksCard; }
                // CMenuTapjoyOption::Draw :221948 uses the sprite origin and
                // bounds, without scaling it to SHOP_BOX's user rectangle.
                view.movies.DrawSprite(5, promoSprite, 0, face.x, face.y);
                if (index != store.size()) {
                    // The money pile is art only; the original prints the words.
                    ZMovieRegion bounds;
                    if (!view.movies.SpriteBounds(5, promoSprite, bounds)) { view.EndClip(); return false; }
                    const float center = face.x + bounds.x + bounds.width / 2;
                    view.CenterText(kFreeCardTop, center, face.y + bounds.y, 6, 1);
                    view.CenterText(kFreeCardBottom, center, face.y + bounds.y + bounds.height - view.movies.TextHeight(6), 6, 1);
                }
                if (cardEnabled && view.Hit(body.x, body.y, body.width, body.height)) {
                    unsigned action = 125;
                    if (index != store.size()) { action = 130; }
                    if (!state.promotion.Activate(view.movies, action)) { view.EndClip(); return false; }
                    state.promotionTick = view.clock;
                    view.EndClip();
                    return true;
                }
                continue;
            }
            const ZStoreEntry &item = store[index];
            if (item.data.type >= 14 && item.data.type <= 16) {
                if (!DrawCurrencyCard(view, toc, tables, item, index, shopBox, face,
                    actionEnabled, state, profile, savePath)) { return false; }
                continue;
            }
            CMenuStoreOption::Action action = CMenuStoreOption::Action::None;
            if (!CMenuStoreOption::DrawCompact(view, toc, tables, profile, item, weapons, slotKind,
                shopBox, face, cardEnabled, actionEnabled, options.buttons[index], action)) { return false; }
            if (action == CMenuStoreOption::Action::Upgrade) {
                state.masteryWeapon = item.data.objects[0].object;
                state.Navigate(26);
            } else if (action == CMenuStoreOption::Action::Purchase) {
                purchaseIndex = static_cast<int>(index);
                purchaseSlot = slotKind;
            } else if (action == CMenuStoreOption::Action::Focus) {
                state.selectedItem = static_cast<int>(index);
                state.slot = slotKind;
                focused.shopDetailOpen = true;
                focused.shopPreview = false;
                focused.shopDetailStart = view.clock;
                focused.shopDetailLastTick = view.clock;
                focused.shopDetailTime = cardStart;
                focused.shopDetailClosing = false;
                focused.shopFocusAmount = 0;
            }
        }
    }
    view.EndClip();
    // The belt's own gradient fades the far column out behind the player.
    view.movies.Draw(storeScroll, view.storeRestTime);

    const GameObjectTypeRef *preview = nullptr;
    unsigned previewSlot = state.store.shopGunSlot;
    if (state.store.focused.shopPreview && state.selectedItem >= 0 && state.selectedItem < static_cast<int>(store.size()) &&
        state.slot < 5) {
        preview = &store[state.selectedItem].data.objects[0];
        previewSlot = state.slot;
    }
    // The original lets the player turn the model by dragging it.
    unsigned rotationDelta = 0;
    if (view.clock >= state.playerMeshLastTick) { rotationDelta = static_cast<unsigned>(view.clock - state.playerMeshLastTick); }
    state.playerMeshLastTick = view.clock;
    view.UpdateMeshRotation(state.playerMesh, rotationDelta, playerPanel, !modalOpen);
    if (!view.playerPreview.Draw(view, toc, tables, profile, weapons, armors, previewSlot, preview, &playerPanel,
        state.playerMesh.GetRadians())) { return false; }
    if (view.playerPreview.TakePlayerPreviewSlotChange()) {
        profile.activeWeaponSlot = view.playerPreview.GetPlayerPreviewSlot();
        if (!profile.SaveToDisk(savePath)) { return false; }
    }
    // MDS_BUTTON_STORE_GUN_SWAP is the round weapon slot toggle.
    if (!DrawStoreGunSwap(view, state, gunSwap, !modalOpen)) { return false; }

    // The FILTER button and its drop-down both live in GLU_MOVIE_SORT_BAR.
    const unsigned sortTime = state.store.shopFilterTime;
    ZMovieRegion sortButton, sortLabel;
    if (!RequireRegion(view, sortBar, kSortButtonRegion, sortTime, sortButton, "filter button") ||
        !RequireRegion(view, sortBar, kSortLabelRegion, sortTime, sortLabel, "filter label")) { return false; }
    view.movies.Draw(sortBar, sortTime);
    // SortLabelCallback :178777 uses font 5 at the label region's top and
    // centres its measured width. The touch rectangle is not a text layout.
    const std::string filterLabel = view.movies.NamedString("IDS_SHOP_FILTER");
    view.movies.Text(filterLabel, sortLabel.x + (sortLabel.width - view.movies.TextWidth(filterLabel, 5)) * 0.5f,
        sortLabel.y, 5, 1, 0, sortLabel.alpha);
    if (!state.store.focused.shopDetailOpen && !state.currencyPending && view.Hit(sortButton.x, sortButton.y, sortButton.width, sortButton.height)) {
        state.store.shopFilterOpen = !state.store.shopFilterOpen;
    }
    // The original region callback keeps drawing while the movie reverses.
    // Only open-state buttons accept input (CMenuStore::Update :179558).
    ZMovieRegion sortPanel;
    if (view.movies.Region(sortBar, kSortPanelRegion, sortTime, sortPanel)) {
        const char *table = nullptr;
        const unsigned rows = StoreFilterRows(state.store.shopCategory, table);
        float optionY = sortPanel.y;
        for (unsigned row = 0; row < rows; ++row) {
            const CMenuDataProvider::Entry *entry = CMenuDataProvider::Find(table, row);
            if (entry == nullptr) { return false; }
            // MDS is extracted from the original executable; it selects each
            // button movie, sprite and string. The movie supplies its geometry.
            const unsigned optionPlate = view.movies.Ordinal(entry->movies[0]);
            ZMovieRegion optionLabel, optionTouch;
            if (!RequireRegion(view, optionPlate, 1, 0, optionLabel, "filter option label") ||
                !RequireRegion(view, optionPlate, 0, 0, optionTouch, "filter option touch")) { return false; }
            const float optionX = sortPanel.x + (sortPanel.width - optionLabel.width) * 0.5f;
            const float y = optionY;
            optionY += optionLabel.height * kSortRowSpacing;
            unsigned bit = 0;
            unsigned mask = state.store.shopFilter;
            if (entry->action == 66) {
                bit = 1u << (entry->parameter - 1);
                mask = state.store.shopExclusionFilter;
            } else if (entry->parameter != 17) { bit = 1u << entry->parameter; }
            bool selected = state.store.filterAll && mask == 0;
            if (bit != 0) { selected = (mask & bit) != 0; }
            unsigned sprite = entry->sprites[1];
            if (selected) { sprite = entry->sprites[0]; }
            view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, optionX, y, optionLabel.width, optionLabel.height);
            PlateLabel(view, view.movies.NamedString(entry->strings[0]), optionX, y, optionLabel.width, optionLabel.height);
            const float touchX = optionX + optionTouch.x - optionLabel.x;
            const float touchY = y + optionTouch.y - optionLabel.y;
            if (!state.store.shopFilterOpen || !view.Hit(touchX, touchY, optionTouch.width, optionTouch.height)) { continue; }
            view.NotePress(optionPlate, optionX, y, optionLabel.width, optionLabel.height);
            if (entry->action == 66) { state.store.shopExclusionFilter ^= bit; }
            else if (bit == 0) { state.store.shopFilter = 0; state.store.filterAll = true; }
            else { state.store.shopFilter ^= bit; state.store.filterAll = false; }
            state.store.shopScroll = 0; state.store.shopMotion = ZMenuScrollMotion{};
        }
    }

    if (state.store.focused.shopDetailOpen && state.selectedItem >= 0 && state.selectedItem < static_cast<int>(store.size())) {
        const ZStoreEntry &item = store[state.selectedItem];
        const GameObjectTypeRef &ref = item.data.objects[0];
        CMenuStoreOption::Face face;
        face.time = state.store.focused.shopDetailTime;
        ZMovieRegion body, foldedBody;
        if (!CardRegion(view, shopBox, kCardBodyRegion, face, body) ||
            !view.movies.Region(shopBox, kCardBodyRegion, cardStart, foldedBody)) { return false; }
        // The card grows out of its own place on the belt and stays in the list.
        float grownX = content.x, grownY = content.y;
        if (focusedColumn >= 0) {
            ZMovieRegion slot;
            if (view.movies.Region(storeScroll, kFirstColumnRegion, view.storeRestTime, slot)) {
                grownX = firstSlot.x + focusedColumn * columnPitch - state.store.shopScroll;
                grownY = slot.y + focusedRow * (slot.height / 2 + 5);
            }
        }
        // CMenuStore::Init :180297 gets the focus center from STORE_MENU region 0:
        // centerX = x + width/2 - width/16, centerY = y + height/2.
        const float targetX = content.x + content.width / 2 - static_cast<int>(content.width) / 16;
        const float targetY = content.y + content.height / 2;
        const float centerX = grownX + foldedBody.width / 2;
        const float centerY = grownY + foldedBody.height / 2;
        face.x = centerX + (targetX - centerX) * state.store.focused.shopFocusAmount - body.width / 2;
        face.y = centerY + (targetY - centerY) * state.store.focused.shopFocusAmount - body.height / 2;
        if (!CardRegion(view, shopBox, kCardBodyRegion, face, body)) { return false; }
        // The old implementation used the fully open rectangle for every frame.
        // Bind :181843 only uses chapter 2 to FORMAT text; callbacks paint at the
        // current region, including its alpha and clipping, throughout expansion.
        CMenuStoreOption::Action action = CMenuStoreOption::Action::None;
        if (!focused.Draw(view, toc, tables, profile, item, weapons, state.slot, face,
            options.buttons[state.selectedItem], action)) { return false; }
        if (action == CMenuStoreOption::Action::Upgrade) {
            state.masteryWeapon = ref.object;
            state.Navigate(26);
        } else if (action == CMenuStoreOption::Action::Purchase) {
            purchaseIndex = state.selectedItem;
            purchaseSlot = state.slot;
        }
    }
    if (purchaseIndex >= 0) {
        const ZStoreEntry &item = store[purchaseIndex];
        const ZPurchaseResult result = profile.AcquireItem(item.data, level);
        // CMenuAction::DoAction 0x38 :94606 uses the original three-button
        // funds prompt. Successful purchases refresh the card without a toast.
        if (result == ZPurchaseResult::InsufficientCoins) {
            ShowStoreFundsPrompt(state, store, profile, 0, item.data.commonPrice, false);
        } else if (result == ZPurchaseResult::InsufficientWarbucks) {
            ShowStoreFundsPrompt(state, store, profile, 1, item.data.rarePrice, false);
        }
        if (result == ZPurchaseResult::Purchased || result == ZPurchaseResult::Owned) {
            if (purchaseSlot < 2) { profile.configuration.SetGun(purchaseSlot, item.data.objects[0].object); }
            else if (purchaseSlot < 5) { CStoreAggregator::Equipped(profile, purchaseSlot) = item.data.objects[0].object; }
            if (item.data.singlePurchase != 0 && result == ZPurchaseResult::Purchased) {
                if (!CStoreAggregator::EquipStoreItem(profile, item.data, armors)) { return false; }
                state.store.focused.shopPreview = false;
            }
            if (!profile.SaveToDisk(savePath)) { return false; }
        }
    }
    return true;
}

} // namespace MenuDetail
