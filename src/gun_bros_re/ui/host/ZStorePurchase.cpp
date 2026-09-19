#include "gun_bros_re/ui/host/ZStorePurchase.h"
#include "gun_bros_re/ui/host/ZStoreRegionClip.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
namespace MenuDetail {

/** GetLastFailPurchaseInfo :156610; ARM 0xD25A8/0xD25F8 confirms the total
 * price and missing balance arguments omitted by the decompiler. */
bool StoreFailureText(ZMenuSurface &view, const CMenuSystem &state, std::string &body) {
    const char *currencyName = "IDS_SHOP_COMMON";
    if (state.failedCurrency == 1) { currencyName = "IDS_SHOP_RARE"; }
    const unsigned amounts[] = {state.failedPrice, state.failedMissing};
    for (unsigned amount : amounts) {
        std::string currency = view.movies.NamedString(currencyName);
        const auto number = currency.find("%i");
        const auto text = body.find("%s");
        if (number == std::string::npos || text == std::string::npos) { return false; }
        currency.replace(number, 2, std::to_string(amount));
        body.replace(text, 2, currency);
    }
    return body.find('%') == std::string::npos;
}

/** The original store draws two cards per column (ItemCallback :178878) on a
 * horizontal belt and expands the focused card in place. Item identity and
 * purchases still come directly from the BIG catalog. */

void ShowStoreFundsPrompt(CMenuSystem &state, const std::vector<ZStoreEntry> &store,
    const CProfileManager &profile, unsigned currency, unsigned price, bool inGame ) {
    state.ShowStorePrompt("MDS_STORE_PROMPT_MOMONEY", false, false);
    state.storePromptButtons = "MDS_BUTTON_STORE_INGAME_PROMPT";
    if (!inGame) { state.storePromptButtons = "MDS_BUTTON_STORE_PROMPT"; }
    state.failedCurrency = currency;
    state.failedPrice = price;
    std::uint64_t balance = profile.coins;
    if (currency == 1) { balance = profile.warbucks; }
    state.failedMissing = 0;
    if (price > balance) { state.failedMissing = static_cast<unsigned>(price - balance); }
    state.currencyOffer = FindCurrencyOffer(store, currency, state.failedMissing);
    state.currencyOfferProduct.clear();
    if (state.currencyOffer >= 0) { state.currencyOfferProduct = store[state.currencyOffer].productId; }
}

bool CompleteOfflineIAP(std::uint64_t clock, CMenuSystem &state, CProfileManager &profile,
    const std::vector<ZStoreEntry> &store, const std::filesystem::path &savePath) {
    if (!state.currencyPending) { return true; }
    int itemIndex = state.currencyItem;
    if (state.currencySimulated) {
        state.online.SetConnected(GameHostSettings().isConnected);
        state.online.UpdatePurchase(clock);
        const auto status = state.online.GetPurchaseState();
        if (status == ZLocalOnlineServices::PurchaseState::Cancelled) {
            state.currencyPending = false;
            state.online.FinishPurchase();
            state.ShowStorePrompt("MDS_STORE_PROMPT_UNAVAILABLE", false, true);
            std::printf("[local-iap] cancelled before delivery\n");
            return true;
        }
        if (status != ZLocalOnlineServices::PurchaseState::Completed) { return true; }
        // AcquireIAP :158280 resolves the callback product ID against the store,
        // independently of the selected card or the current catalog order.
        itemIndex = -1;
        for (unsigned index = 0; index < store.size(); ++index) {
            if (store[index].data.value32 == 1 && store[index].productId == state.online.GetProduct()) {
                itemIndex = static_cast<int>(index);
                break;
            }
        }
        state.online.FinishPurchase();
    } else if (clock < state.currencyReadyAt) { return true; }
    state.currencyPending = false;
    state.storePromptRequested = false;
    state.storePopup.Hide();
    if (itemIndex < 0 || itemIndex >= static_cast<int>(store.size())) {
        state.ShowStorePrompt("MDS_STORE_PROMPT_UNAVAILABLE", false, true);
        std::printf("[local-iap] product not found; no delivery\n");
        return true;
    }
    const auto previousCoins = profile.coins;
    const auto previousBucks = profile.warbucks;
    const ZPurchaseResult result = profile.AcquireCurrency(store[itemIndex].data);
    if (result == ZPurchaseResult::Purchased && !profile.SaveToDisk(savePath)) {
        profile.coins = previousCoins;
        profile.warbucks = previousBucks;
        return false;
    }
    if (result != ZPurchaseResult::Purchased) { state.ShowStorePrompt("MDS_STORE_PROMPT_UNAVAILABLE", false, true); }
    std::printf("[offline-iap] completed result=%u simulated-validation=%u\n", static_cast<unsigned>(result), state.currencySimulated);
    return true;
}

/** IAP is a standard modal prompt, layout mode 1 (visual left), no buttons.
 * CMenuSystem::ShowPopup :96455 selects fonts 0/0/1/5 and GLU_MOVIE_POPUP.
 * BindContent :207403 derives its target size from fonts and sprite bounds. */
bool DrawStorePrompt(ZMenuSurface &view, CMenuSystem &state) {
    if (!state.storePromptRequested && !state.storePopup.IsActive()) { return true; }
    const auto *entry = CMenuDataProvider::Find(state.storePromptTable, state.storePromptIndex);
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_POPUP");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned smallStart = 0, smallEnd = 0, largeStart = 0, largeEnd = 0;
    ZMovieRegion compactRegion, expandedRegion, visual;
    if (entry == nullptr || movie == nullptr || !movie->GetChapterRange(1, smallStart, smallEnd) ||
        !movie->GetChapterRange(2, largeStart, largeEnd) ||
        !view.movies.Region(ordinal, 1, smallStart, compactRegion) || !view.movies.Region(ordinal, 1, largeStart, expandedRegion)) { return false; }
    const bool hasVisual = entry->sprites[0] != UINT32_MAX;
    if (hasVisual && !view.movies.SpriteBounds(entry->sprites[0] >> 16, entry->sprites[0] & 255, visual)) { return false; }
    std::string title = view.movies.NamedString(entry->strings[1]);
    std::string body = view.movies.NamedString(entry->strings[0]);
    if (!state.challengeRewardTitle.empty()) { title = state.challengeRewardTitle; body = state.challengeRewardBody; }
    if (state.matchingPrompt) {
        // The original uses GKMatchmakerViewController (:260057), not BIG.
        // This labelled desktop adapter borrows only the native popup geometry.
        title = "LOCAL MATCHMAKING";
        body = "Waiting for another player.\nLocal simulation has no peer connected.";
        if (state.gameMode == 1) {
            body = "Joining LOCAL BOT for Live co-op...";
        } else {
            body = "Joining LOCAL BOT for Deathmatch...\nLocal player versus Bot.";
        }
    }
    if (!state.matchingPrompt && state.storePromptButtons != nullptr && !StoreFailureText(view, state, body)) { return false; }
    const float bodyHeight = view.movies.TextHeight(0);
    float titleSpace = view.movies.TextHeight(0) + bodyHeight;
    if (!state.storePromptSideVisual) { titleSpace = view.movies.TextHeight(0) + static_cast<unsigned>(bodyHeight) / 2; }
    // GetVisualContentBounds :207100 pads by the body font's even line height.
    float visualPadding = 0;
    if (hasVisual) { visualPadding = static_cast<float>(static_cast<unsigned>(bodyHeight) & ~1u); }
    const float visualWidth = visual.width + visualPadding;
    const float visualHeight = visual.height + visualPadding;
    float textWidth = compactRegion.width;
    if (state.storePromptSideVisual) { textWidth -= visualWidth; }
    const auto lines = CTextBox::Format(view.movies, body, textWidth, {0, 1, 0, 0, 0});
    float textHeight = 0;
    for (const CTextBox::Line &line : lines) { textHeight += line.height; }
    if (!state.storePopup.IsActive()) {
        float contentHeight = titleSpace + textHeight + visualHeight;
        if (state.storePromptSideVisual) { contentHeight = std::max(visualHeight, titleSpace + textHeight); }
        if (!state.storePopup.Bind(*movie, compactRegion.height, expandedRegion.height, contentHeight)) { return false; }
        state.storePromptRequested = false;
        state.storePopupLastTick = view.clock;
        state.storePromptSpriteTime = 0;
        std::printf("[iap-prompt] BIG target=%u compactRegion=%.0f expandedRegion=%.0f image=%.0fx%.0f body-lines=%zu\n",
            state.storePopup.TargetTime(), compactRegion.height, expandedRegion.height, visual.width, visual.height, lines.size());
    }
    unsigned delta = 0;
    if (view.clock >= state.storePopupLastTick) { delta = static_cast<unsigned>(view.clock - state.storePopupLastTick); }
    state.storePopupLastTick = view.clock;
    state.storePopup.Update(delta);
    if (!state.storePopup.IsActive()) { return true; }
    const unsigned time = state.storePopup.MovieTime();
    if (!view.movies.Draw(ordinal, time)) { return false; }
    ZMovieRegion area;
    if (!view.movies.Region(ordinal, 1, time, area)) { return true; }
    const float alpha = area.alpha * state.storePopup.ContentAlpha();
    {
        ZStoreRegionClip clip(view, area);
        view.movies.Text(title, area.x + (area.width - view.movies.TextWidth(title, 0)) / 2, area.y, 0, 1, 0, alpha);
        float y = area.y + titleSpace;
        if (!state.storePromptSideVisual) { y += visualHeight; }
        for (const CTextBox::Line &line : lines) {
            float x = area.x + (area.width - line.width) / 2;
            if (state.storePromptSideVisual) { x = area.x + visualWidth; }
            for (const CTextBox::Run &run : line.runs) {
                view.movies.Text(run.text, x + run.x, y + (line.height - run.height) / 2, run.font, 1, 0, alpha);
            }
            y += line.height;
        }
        if (hasVisual && time == state.storePopup.TargetTime()) {
            // Update :207249 advances the visual only after the container reaches
            // its size target; it must not inherit time spent opening the box.
            state.storePromptSpriteTime += delta;
            float visualX = area.x + visualWidth / 2;
            float visualY = area.y + area.height / 2;
            if (!state.storePromptSideVisual) {
                visualX = area.x + area.width / 2;
                visualY = area.y + titleSpace + visualHeight / 2;
            }
            if (!view.movies.DrawSprite(entry->sprites[0] >> 16, entry->sprites[0] & 255, state.storePromptSpriteTime,
                visualX, visualY, 1, alpha)) { return false; }
        }
    }
    if (state.storePromptDismiss) {
        ZMovieRegion dismissal;
        const auto *dismiss = CMenuDataProvider::Find("MDS_BUTTON_POPUP_PROMPT", 0);
        if (dismiss == nullptr) { return false; }
        if (!view.movies.Region(ordinal, 2, time, dismissal)) { return true; }
        const std::string text = view.movies.NamedString(dismiss->strings[1]);
        view.movies.Text(text, dismissal.x + (dismissal.width - view.movies.TextWidth(text, 5)) / 2,
            dismissal.y + dismissal.height / 2, 5, 1, 0, alpha);
        if (state.storePopup.IsReady()) {
            ZMovieRegion touch;
            if (!view.movies.Region(ordinal, 0, time, touch)) { return false; }
            const bool previousInput = view.inputEnabled;
            view.inputEnabled = true;
            if (view.Hit(touch.x, touch.y, touch.width, touch.height)) { state.storePopup.Hide(); }
            view.inputEnabled = previousInput;
        }
    }
    if (state.storePromptButtons != nullptr) {
        unsigned first = 1;
        if (std::strcmp(state.storePromptButtons, "MDS_BUTTON_STORE_PROMPT") == 0) { first = 0; }
        unsigned last = 2;
        if (state.matchingPrompt) { first = 0; last = 0; }
        for (unsigned index = first; index <= last; ++index) {
            ZMovieRegion region, buttonBounds, popupBounds;
            const auto *button = CMenuDataProvider::Find(state.storePromptButtons, index);
            if (button == nullptr) { return false; }
            if (!view.movies.Region(ordinal, index + 2, time, region)) { continue; }
            if (!view.movies.Region(view.movies.Ordinal(button->movies[0]), 0, 0, buttonBounds) ||
                !view.movies.Region(ordinal, 0, time, popupBounds)) { return false; }
            // ButtonCallback :206404 centers within the assigned region and
            // clamps to its edges if the button would cross the popup's bounds.
            ZMovieRegion placed = region;
            placed.x += region.width / 2 - buttonBounds.width / 2;
            if (placed.x < popupBounds.x) { placed.x = region.x; }
            else if (placed.x + buttonBounds.width > popupBounds.x + popupBounds.width) {
                placed.x = region.x + region.width - buttonBounds.width;
            }
            placed.y += region.height / 2 - buttonBounds.height / 2;
            placed.alpha *= state.storePopup.ContentAlpha();
            const bool previousInput = view.inputEnabled;
            view.inputEnabled = state.storePopup.IsReady();
            bool pressed = false;
            if (!CMenuMovieButton::DrawFrame(view, *button, placed, view.movies.NamedString(button->strings[0]), 5,
                state.storePopup.IsReady(), pressed)) { return false; }
            view.inputEnabled = previousInput;
            if (pressed) {
                if (button->action == 139 && state.matchingPrompt) {
                    state.online.CancelMatch();
                    state.storePopup.Hide();
                }
                if (button->action == 45) { state.storePopup.Hide(); }
                if (button->action == 70) {
                    state.storePopup.Hide();
                    std::printf("[store] Tapjoy offers unavailable on host; no reward issued\n");
                }
                if (button->action == 71 && state.currencyOffer >= 0) {
                    state.BeginOfflineIAP(state.currencyOffer, view.clock, state.currencyOfferProduct);
                }
                return true;
            }
        }
    }
    return true;
}
}
