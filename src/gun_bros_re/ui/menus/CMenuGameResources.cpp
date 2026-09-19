#include "gun_bros_re/ui/host/ZStorePurchase.h"
#include "gun_bros_re/ui/host/ZStoreRegionClip.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/menus/CMenuGameResources.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
namespace MenuDetail {
bool SetRefineryStatus(ZMenuSurface &view, CMenuSystem &state, unsigned slot, unsigned chapter);

bool IsRefinerySlotEnabled(const CRefinementManager::Template &data, unsigned slot) {
    // CResourceMeter::Enabled :173669 ORs connectivity with zero duration.
    return GameHostSettings().isConnected || data.minutes[slot] == 0;
}

/** Original Select completes its Movie before action73 starts the transfer. */
bool StartRefineryTransfer(ZMenuSurface &view, CMenuSystem &state, CProfileManager &profile,
    unsigned slot, unsigned count) {
    if (state.refinery.refineryTransfer >= 0) { return true; }
    const auto &record = profile.refinery.slots[slot];
    ZMovieRegion region;
    if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_EXPLODIUM"), slot % count,
        state.refinery.refineryTime, region)) { return false; }
    const float x = region.x + region.width / 2, y = region.y + region.height / 2;
    if ((record.state == 1 || (record.state == 3 && state.refinery.refineryStatusChapter[slot] == 3))) {
        if (record.state == 3 || profile.xplodium != 0) {
            const unsigned main = view.movies.Ordinal("GLU_MOVIE_EXPLODIUM");
            ZMovieRegion source, destination;
            unsigned icon = 0;
            if (record.state == 3) {
                source.x = x;
                source.y = y;
                if (!view.movies.Region(main, count * 2 + 2, state.refinery.refineryTime, destination)) { return false; }
                icon = 2;
                state.refinery.refineryTransferAmount = profile.refinery.GetRefinementSlotYield(slot);
            } else {
                if (!view.movies.Region(main, count * 2, state.refinery.refineryTime, source)) { return false; }
                const auto *image = CMenuDataProvider::Find("MDS_ICON_STANDARD", 0);
                ZMovieRegion bounds;
                if (image == nullptr || !view.movies.SpriteBounds(image->sprites[0] >> 16, image->sprites[0] & 255, bounds)) { return false; }
                source.x += bounds.width / 2;
                source.y += bounds.height / 2;
                destination.x = x;
                destination.y = y;
                state.refinery.refineryTransferAmount = profile.xplodium;
            }
            const auto *image = CMenuDataProvider::Find("MDS_ICON_STANDARD", icon);
            if (image == nullptr) { return false; }
            if (!view.refineryEffects.StartRefineryEffect(slot, icon, source.x, source.y)) { return false; }
            state.refinery.refineryTransfer = static_cast<int>(slot);
            state.refinery.refineryTransferTime = 0;
            state.refinery.refineryTransferSprite = image->sprites[0];
            state.refinery.refineryTransferX = source.x;
            state.refinery.refineryTransferY = source.y;
            state.refinery.refineryTargetX = destination.x;
            state.refinery.refineryTargetY = destination.y;
        }
    }
    return true;
}

class ZRefineryCallbacks : public ZMovieRegionCallback {
public:
    ZRefineryCallbacks(ZMenuSurface &menu, CMenuSystem &selection, CProfileManager &account,
        const CRefinementManager::Template &resources, unsigned cells, bool ready,
        const std::filesystem::path &path, std::int64_t seconds)
        : view(menu), state(selection), profile(account), data(resources), count(cells), interactive(ready), savePath(path), now(seconds) {}

    bool DrawMovieRegion(const ZMovieRegion &region) override {
        if (region.index < count) { return Meter(region); }
        if (region.index < count * 2) { return MeterInfo(region); }
        if (region.index == count * 2) { return Xplodium(region); }
        if (region.index == count * 2 + 1) {
            DrawMissionText(view, region, view.movies.NamedString("IDS_RESMAN_SIDEBARINFO"), 1);
        }
        if (region.index == count * 2 + 4) { return Categories(region); }
        return true;
    }

    bool Categories(const ZMovieRegion &region) {
        // CategoryButtonCallback :172186 draws entry1 then entry0, with 4px
        // between them and total width 2*graphicWidth+8, exactly as the source.
        const auto *first = CMenuDataProvider::Find("MDS_BUTTON_REFINE_SLOT_CATEGORY", 1);
        if (first == nullptr) { return false; }
        const unsigned movie = view.movies.Ordinal(first->movies[0]);
        ZMovieRegion graphic;
        if (!view.movies.Region(movie, 1, 0, graphic)) { return false; }
        float x = region.x + region.width / 2 - (graphic.width * 2 + 8) / 2;
        for (unsigned position = 0; position < 2; ++position) {
            const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_REFINE_SLOT_CATEGORY", 1 - position);
            if (entry == nullptr) { return false; }
            ZMovieRegion area = region;
            area.x = x;
            bool pressed = false;
            unsigned chapter = 2;
            if (state.refinery.refineryTab == entry->index) { chapter = 3; }
            if (!CMenuMovieButton::DrawFrame(view, *entry, area, view.movies.NamedString(entry->strings[0]), 5,
                interactive && state.refinery.refineryTransfer < 0, pressed, chapter, state.refinery.refineryElapsed, UINT32_MAX, true)) { return false; }
            if (pressed && state.refinery.refineryTab != entry->index) {
                // DoAction :95106 forwards argument4 (entry index), not the
                // table's parameter. Refresh(76) changes the category only.
                state.refinery.refineryTab = entry->index;
            }
            x += graphic.width + 4;
        }
        return true;
    }

    bool Xplodium(const ZMovieRegion &region) {
        const auto *entry = CMenuDataProvider::Find("MDS_ICON_STANDARD", 0);
        if (entry == nullptr) { return false; }
        const unsigned sprite = entry->sprites[0];
        ZMovieRegion bounds;
        if (!view.movies.SpriteBounds(sprite >> 16, sprite & 255, bounds)) { return false; }
        if (!view.movies.DrawSprite(sprite >> 16, sprite & 255, state.refinery.refineryElapsed,
            region.x + bounds.width / 2, region.y + region.height / 2, 1, region.alpha)) { return false; }
        const float x = region.x + bounds.width;
        view.movies.Text(view.movies.NamedString(entry->strings[0]), x, region.y, 0, 1, 0, region.alpha);
        std::string amount = std::to_string(static_cast<std::int32_t>(profile.xplodium));
        if (state.refinery.refineryTransfer >= 0 && profile.refinery.slots[state.refinery.refineryTransfer].state == 1) { amount = "0"; }
        view.movies.Text(amount, x, region.y + region.height - view.movies.TextHeight(0), 0, 1, 0, region.alpha);
        return true;
    }

    bool MeterInfo(const ZMovieRegion &region) {
        const unsigned slot = state.refinery.refineryTab * count + region.index - count;
        // CreateContentSprite(69,1) :149765 uses core archetype0,
        // SG_TEXTBOX_ANIM=138 at VA0x3c4946, independent of the slot state.
        if (!view.movies.DrawSprite(0, 138, 0, region.x, region.y, 1, region.alpha)) { return false; }
        std::string title;
        if (data.minutes[slot] == 0) { title = view.movies.NamedString("IDS_RESMAN_INTERVAL0"); }
        else if (data.minutes[slot] < 60) { title = std::to_string(data.minutes[slot]) + " MINS"; }
        else { title = std::to_string(data.minutes[slot] / 60) + " HRS"; }
        unsigned font = 1;
        std::string detail = RefineryNumber(view, "IDS_RESMAN_YIELD", data.efficiencyPercent[slot]);
        const auto &record = profile.refinery.slots[slot];
        std::uint64_t amount = profile.xplodium;
        if (record.state == 2 || record.state == 3) { amount = record.amount; }
        // The local service has no validated friend power. Provider 69/3
        // alternates only on enabled meters, and only for a nonzero payout.
        if (IsRefinerySlotEnabled(data, slot) && amount != 0 && (state.refinery.refineryElapsed / 2000) % 2 != 0) {
            const auto payout = static_cast<std::uint64_t>(std::floor(static_cast<float>(amount) *
                (data.efficiencyPercent[slot] + profile.refinery.friendEfficiencyBonus) / 100.0f + 0.5f));
            detail = RefineryNumber(view, "IDS_SHOP_COMMON", payout);
            font = 0;
        }
        if (title.empty() || detail.empty()) { return false; }
        view.movies.Text(title, region.x + (region.width - view.movies.TextWidth(title, 0)) / 2, region.y, 0, 1, 0, region.alpha);
        view.movies.Text(detail, region.x + (region.width - view.movies.TextWidth(detail, font)) / 2,
            region.y + region.height - view.movies.TextHeight(font), font, 1, 0, region.alpha);
        return true;
    }

    bool Meter(const ZMovieRegion &region) {
        const unsigned slot = state.refinery.refineryTab * count + region.index;
        const auto &record = profile.refinery.slots[slot];
        auto &chamber = state.refinery.chambers[slot];
        const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_XPLODIUM_METER", slot);
        if (entry == nullptr) { return false; }
        const float x = region.x + region.width / 2, y = region.y + region.height / 2;
        const bool enabled = IsRefinerySlotEnabled(data, slot);
        const unsigned fill = view.movies.Ordinal("GLU_MOVIE_BUCKET_FILL");
        const unsigned status = view.movies.Ordinal(entry->movies[1]);
        if (!view.movies.Draw(fill, state.refinery.refineryFillTime[slot], x, y)) { return false; }
        if (!chamber.opening) {
            // The native button movie is centered on the meter. Its own hit
            // region owns interaction; the 220px parent is not a click target.
            ZMovieRegion button = region;
            button.x = x;
            button.y = y;
            bool pressed = false;
            unsigned chapter = 0;
            if (record.state != 0 && enabled) { chapter = 2; }
            unsigned timeOverride = UINT32_MAX;
            if (chamber.clickPlaying) { chapter = 1; timeOverride = chamber.clickTime; }
            // Enabled(false) sets button state6, which still draws; Draw skips
            // only state8 (:144707). Keep the original circle behind the lock.
            if (!CMenuMovieButton::DrawFrame(view, *entry, button, {}, 0,
                enabled && record.state != 0 && !chamber.clickPlaying && interactive && state.refinery.refineryTransfer < 0,
                pressed, chapter, state.refinery.refineryElapsed * 2, timeOverride)) { return false; }
            if (pressed) {
                // CMenuMovieButton::Select :144661 plays chapter1 once even
                // for every enabled slot, regardless of ore or state. Geometry comes from BIG.
                const auto *movie = view.movies.GetMovie(view.movies.Ordinal(entry->movies[0]));
                unsigned end = 0;
                if (movie == nullptr || !movie->GetChapterRange(1, chamber.clickTime, end)) { return false; }
                chamber.clickPlaying = true;
            }

        }
        if (!view.movies.Draw(status, state.refinery.refineryStatusTime[slot], x, y)) { return false; }
        if (record.state == 0 || chamber.opening) {
            // CreateContentSprite(69,0) :149780, original SG constants:
            // archetype4, first animation24, count6. Index chooses lock art.
            // CResourceMeter::Update advances this sprite only AFTER unlock.
            // Its initial colored chamber stays frozen while state==0.
            if (!view.movies.DrawSpritePlayer(4, 24 + slot % 6, chamber.eye, x, y, region.alpha)) { return false; }
            const unsigned overlay = view.movies.Ordinal("GLU_MOVIE_CHAMBER_OVERLAY");
            const CMovie *movie = view.movies.GetMovie(overlay);
            unsigned start = 0, end = 0;
            unsigned chapter = 0;
            if (chamber.opening) { chapter = 1; }
            if (movie == nullptr || !movie->GetChapterRange(chapter, start, end)) { return false; }
            unsigned time = start + state.refinery.refineryElapsed % (end - start + 1);
            if (chamber.opening) { time = start + std::min(chamber.overlayTime, end - start); }
            if (!view.movies.Draw(overlay, time, x, y)) { return false; }
        }
        std::string text;
        if (!enabled) { text = view.movies.NamedString("IDS_FRIEND_OFFLINE"); }
        else if (record.state == 2) {
            // GetRemainingTimeString :177971 -> TimeToString, hours/minutes/seconds.
            const auto seconds = std::max<std::int64_t>(0, record.finishTime - now);
            char remaining[32];
            std::snprintf(remaining, sizeof(remaining), "%02lld:%02lld:%02lld", seconds / 3600, seconds / 60 % 60, seconds % 60);
            text = remaining;
        }
        else if (record.state == 3) { text = view.movies.NamedString("IDS_RESMAN_COLLECT"); }
        else if (record.state == 1 && profile.xplodium != 0) { text = view.movies.NamedString("IDS_RESMAN_READY"); }
        if (!text.empty() && !chamber.opening) {
            view.movies.Text(text, x - view.movies.TextWidth(text, 0) / 2, y - view.movies.TextHeight(0) / 2, 0, 1, 0, region.alpha);
        }
        if (record.state == 0 && !profile.refinery.IsGated(slot)) {
            // Dynamic provider158: CreateContentMovie :149176, Sprite0:64,
            // GetElementAction :153522 -> UnlockSlot action74. Draw centers it.
            const CMenuDataProvider::Entry unlock{"MDC_REFINE_UNLOCK", slot, {"", "", "", ""},
                {64, UINT32_MAX, 0, 0}, {"GLU_MOVIE_BUTTON_SMALL", ""}, 74, 0};
            ZMovieRegion graphic;
            if (!view.movies.Region(view.movies.Ordinal(unlock.movies[0]), 1, 0, graphic)) { return false; }
            ZMovieRegion button = region;
            button.x = x - graphic.width / 2;
            button.y = y;
            std::string price = view.movies.NamedString("IDS_SHOP_FREE");
            if (data.commonPrice[slot]) { price = RefineryNumber(view, "IDS_SHOP_COMMON", data.commonPrice[slot]); }
            else if (data.rarePrice[slot]) { price = RefineryNumber(view, "IDS_SHOP_RARE", data.rarePrice[slot]); }
            // MeterCallback :173873 draws the currency in font0 BELOW the
            // button; provider158's button caption itself is IDS_SHOP_BUY.
            if (!view.movies.Text(price, x - view.movies.TextWidth(price, 0) / 2,
                y + graphic.height + view.movies.TextHeight(0) / 2, 0, 1, 0, region.alpha)) { return false; }
            if (!enabled) { return true; }
            bool pressed = false;
            if (!CMenuMovieButton::DrawFrame(view, unlock, button, view.movies.NamedString("IDS_SHOP_BUY"), 5, interactive && state.refinery.refineryTransfer < 0,
                pressed, 2, state.refinery.refineryElapsed)) { return false; }
            if (pressed) {
                if (profile.refinery.UnlockSlot(slot, profile.coins, profile.warbucks)) {
                    if (!profile.SaveToDisk(savePath) || !SetRefineryStatus(view, state, slot, 1)) { return false; }
                    std::printf("[refinery] unlocked slot=%u coins=%u warbucks=%u\n", slot, data.commonPrice[slot], data.rarePrice[slot]);
                } else {
                    // Original action74 opens the insufficient-funds prompt.
                    std::vector<ZStoreEntry> store;
                    if (!profile.nativeArchive || !LoadStoreCatalog(*profile.nativeArchive->toc, *profile.nativeArchive->tables, store)) { return false; }
                    unsigned currency = 0;
                    unsigned cost = data.commonPrice[slot];
                    if (cost == 0) { currency = 1; cost = data.rarePrice[slot]; }
                    ShowStoreFundsPrompt(state, store, profile, currency, cost, false);
                    state.storePromptTable = "MDS_REFINE_PROMPT_MOMONEY";
                }
            }
        }
        return true;
    }
    ZMenuSurface &view;
    CMenuSystem &state;
    CProfileManager &profile;
    const CRefinementManager::Template &data;
    unsigned count;
    bool interactive;
    const std::filesystem::path &savePath;
    std::int64_t now;
};
// Page callback implementations.

bool SetRefineryStatus(ZMenuSurface &view, CMenuSystem &state, unsigned slot, unsigned chapter) {
    const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_XPLODIUM_METER", slot);
    if (entry == nullptr) { return false; }
    const auto *movie = view.movies.GetMovie(view.movies.Ordinal(entry->movies[1]));
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(chapter, start, end)) { return false; }
    state.refinery.refineryStatusChapter[slot] = chapter;
    state.refinery.refineryStatusTime[slot] = start;
    return true;
}

bool CMenuGameResources::Draw(ZMenuSurface &view, CMenuSystem &state, CProfileManager &profile,
    const CRefinementManager::Template &data, const std::filesystem::path &savePath, std::int64_t now) {
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_EXPLODIUM");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    const CMovie *fill = view.movies.GetMovie(view.movies.Ordinal("GLU_MOVIE_BUCKET_FILL"));
    unsigned idleStart = 0, idleEnd = 0;
    if (movie == nullptr || fill == nullptr || !movie->GetChapterRange(1, idleStart, idleEnd)) { return false; }
    const auto regions = view.movies.Regions(ordinal, idleStart);
    if (regions.size() < 5 || (regions.size() - 5) % 2 != 0) { return false; }
    const unsigned count = static_cast<unsigned>((regions.size() - 5) / 2);
    if (count * 2 != data.minutes.size()) { return false; }
    if (!state.refinery.refineryBound) {
        view.refineryEffects.ResetRefineryEffects();
        state.refinery.refineryBound = true;
        state.refinery.refineryTab = 1;
        state.refinery.refineryTime = 0;
        state.refinery.refineryElapsed = 0;
        state.refinery.refineryLastTick = view.clock;
        for (unsigned slot = 0; slot < count * 2; ++slot) {
            unsigned chapter = 0;
            if (IsRefinerySlotEnabled(data, slot)) { chapter = profile.refinery.slots[slot].state; }
            if (!SetRefineryStatus(view, state, slot, chapter)) { return false; }
            state.refinery.refineryFillTime[slot] = 0;
            auto &chamber = state.refinery.chambers[slot];
            chamber.locked = profile.refinery.slots[slot].state == 0;
            chamber.opening = false;
            chamber.overlayTime = 0;
            chamber.clickPlaying = false;
            if (!view.movies.BindSpritePlayer(4, 24 + slot % count, chamber.eye)) { return false; }
        }
    }
    const unsigned delta = static_cast<unsigned>(view.clock - state.refinery.refineryLastTick);
    state.refinery.refineryLastTick = view.clock;
    view.refineryEffects.AdvanceRefineryEffects(delta);
    state.refinery.refineryElapsed += delta;
    // Update :173301 advances the shared background; Bind does not rewind it.
    backgroundTime += delta;
    const std::uint64_t next = static_cast<std::uint64_t>(state.refinery.refineryTime) + delta;
    state.refinery.refineryTime = static_cast<unsigned>(next);
    if (next > idleEnd) { state.refinery.refineryTime = idleStart + static_cast<unsigned>((next - idleStart) % (idleEnd - idleStart + 1)); }
    if (!view.animateNavigation) { state.refinery.refineryTime = idleStart; }
    if (state.refinery.refineryCancelTransfer) {
        // A lock cheat can cancel a pending 375ms transfer before it commits.
        view.refineryEffects.ResetRefineryEffects();
        state.refinery.refineryTransfer = -1;
        state.refinery.refineryCancelTransfer = false;
    }
    if (state.refinery.refineryTransfer >= 0) {
        state.refinery.refineryTransferTime += delta;
        const float fraction = std::min(1.0f, state.refinery.refineryTransferTime / 375.0f);
        view.refineryEffects.MoveRefineryEffect(static_cast<unsigned>(state.refinery.refineryTransfer),
            std::trunc(state.refinery.refineryTransferX + (state.refinery.refineryTargetX - state.refinery.refineryTransferX) * fraction),
            std::trunc(state.refinery.refineryTransferY + (state.refinery.refineryTargetY - state.refinery.refineryTransferY) * fraction));
        // CTransferEffect::Setup :174380: original linear x/y duration375ms.
        if (state.refinery.refineryTransferTime >= 375) {
            const unsigned slot = static_cast<unsigned>(state.refinery.refineryTransfer);
            view.refineryEffects.StopRefineryEffect(slot);
            const unsigned status = profile.refinery.slots[slot].state;
            bool changed = false;
            if (status == 1) {
                changed = profile.refinery.BeginRefinement(slot, slot, profile.xplodium, profile.xplodium, now);
                if (!SetRefineryStatus(view, state, slot, 2)) { return false; }
            } else if (status == 3) {
                changed = profile.refinery.CollectResources(slot, profile.coins);
                if (!SetRefineryStatus(view, state, slot, 1)) { return false; }
            }
            if (!changed || !profile.SaveToDisk(savePath)) { return false; }
            state.refinery.refineryFillTime[slot] = 0;
            state.refinery.refineryTransfer = -1;
            bool hasReady = false;
            for (unsigned index = 0; index < count * 2; ++index) {
                if (IsRefinerySlotEnabled(data, index) && profile.refinery.slots[index].state == 3) { hasReady = true; }
            }
            if (profile.xplodium == 0 && !hasReady && state.refinementRequired) {
                // TransferComplete :174075 calls Dismiss :172275 after collection.
                // Update :173392 waits for IsNavBarBusy before entering the store.
                state.refinementRequired = false;
                state.refinery.refineryExitPending = true;
            }
            std::printf("[refinery] transfer complete slot=%u old-state=%u new-state=%u\n", slot, status, profile.refinery.slots[slot].state);
        }
    }
    for (unsigned slot = 0; slot < count * 2; ++slot) {
        const auto &record = profile.refinery.slots[slot];
        auto &chamber = state.refinery.chambers[slot];
        // CResourceMeter::Update :173928 keeps the eye alive after state 0 -> 1.
        // Its native player signals completion; only then is the main button shown.
        if (chamber.locked && record.state != 0) {
            chamber.opening = true;
            chamber.overlayTime = 0;
            state.refinery.refineryFillTime[slot] = 0;
        }
        if (!chamber.locked && record.state == 0) {
            chamber.opening = false;
            chamber.overlayTime = 0;
            state.refinery.refineryFillTime[slot] = 0;
            if (!view.movies.BindSpritePlayer(4, 24 + slot % count, chamber.eye)) { return false; }
        }
        chamber.locked = record.state == 0;
        if (chamber.opening) {
            chamber.overlayTime += delta;
            chamber.eye.Update(static_cast<std::uint16_t>(std::min(delta, unsigned(UINT16_MAX))));
            if (chamber.eye.HasFinished()) { chamber.opening = false; }
        }
        unsigned desired = record.state;
        if (!IsRefinerySlotEnabled(data, slot)) { desired = 0; }
        const unsigned previousChapter = state.refinery.refineryStatusChapter[slot];
        // A newly completed transfer finishes its fill animation before COLLECT.
        if (desired != previousChapter && !(desired == 3 && previousChapter == 2)) {
            if (!SetRefineryStatus(view, state, slot, desired)) { return false; }
        }
        const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_XPLODIUM_METER", slot);
        if (entry == nullptr) { return false; }
        if (record.state == 0 || !IsRefinerySlotEnabled(data, slot)) { chamber.clickPlaying = false; }
        if (chamber.clickPlaying) {
            const auto *button = view.movies.GetMovie(view.movies.Ordinal(entry->movies[0]));
            unsigned start = 0, end = 0;
            if (button == nullptr || !button->GetChapterRange(1, start, end)) { return false; }
            // CResourceMeter::Update :173945 advances its main button at 2x.
            chamber.clickTime += std::min(delta * 2, end - chamber.clickTime);
            if (chamber.clickTime == end) {
                chamber.clickPlaying = false;
                // CMenuMovieButton::Update :144715 dispatches action73 here.
                // Empty and unfinished chambers animate but have no transfer.
                if (!StartRefineryTransfer(view, state, profile, slot, count)) { return false; }
            }
        }
        const auto *status = view.movies.GetMovie(view.movies.Ordinal(entry->movies[1]));
        unsigned start = 0, end = 0;
        const unsigned chapter = state.refinery.refineryStatusChapter[slot];
        if (status == nullptr || !status->GetChapterRange(chapter, start, end)) { return false; }
        unsigned time = state.refinery.refineryStatusTime[slot];
        if (chapter >= 2) { time = start + (time - start + delta) % (end - start + 1); }
        else { time += std::min(delta, end - time); }
        state.refinery.refineryStatusTime[slot] = time;
        if (record.state == 2 && record.totalDurationMs > 0) {
            const auto remaining = std::max<std::int64_t>(0, record.finishTimeMs - now * 1000);
            const auto elapsed = record.totalDurationMs - std::min<std::int64_t>(record.totalDurationMs, remaining);
            state.refinery.refineryFillTime[slot] = static_cast<unsigned>(fill->duration * elapsed / record.totalDurationMs);
        }
        if (profile.refinery.slots[slot].state == 3) {
            state.refinery.refineryFillTime[slot] += std::min(delta * 2, fill->duration - state.refinery.refineryFillTime[slot]);
            if (chapter == 2 && state.refinery.refineryFillTime[slot] == fill->duration &&
                !SetRefineryStatus(view, state, slot, 3)) { return false; }
        }
    }
    if (!view.movies.DrawNamed("GLU_MOVIE_EXPLODIUM_BG", backgroundTime)) { return false; }
    ZRefineryCallbacks callbacks(view, state, profile, data, count,
        state.refinery.refineryTime >= idleStart && !state.storePromptRequested && !state.storePopup.IsActive(), savePath, now);
    if (!view.movies.Draw(ordinal, state.refinery.refineryTime, 512, 384, 1024, 768, 0, 1, &callbacks)) { return false; }
    return true;
}

bool CMenuGameResources::DrawOverlay(ZMenuSurface &view, const CMenuSystem &state) const {
    // CMenuSystem::Draw :96837 calls the current menu overlay after the header.
    view.refineryEffects.DrawRefineryEffects();
    if (state.refinery.refineryTransfer >= 0) {
        const float fraction = std::min(1.0f, state.refinery.refineryTransferTime / 375.0f);
        const float x = std::trunc(state.refinery.refineryTransferX + (state.refinery.refineryTargetX - state.refinery.refineryTransferX) * fraction);
        const float y = std::trunc(state.refinery.refineryTransferY + (state.refinery.refineryTargetY - state.refinery.refineryTransferY) * fraction);
        const unsigned sprite = state.refinery.refineryTransferSprite;
        ZMovieRegion bounds;
        float alpha = 1;
        if (state.refinery.refineryTransferTime > 188) { alpha -= std::min(0.5f, (state.refinery.refineryTransferTime - 188) / 374.0f); }
        if (!view.movies.SpriteBounds(sprite >> 16, sprite & 255, bounds) ||
            !view.movies.DrawSprite(sprite >> 16, sprite & 255, state.refinery.refineryTransferTime, x, y, 1, alpha)) { return false; }
        view.movies.Text(std::to_string(static_cast<std::int32_t>(state.refinery.refineryTransferAmount)), x + bounds.width / 2, y, 0, 1, 0, alpha);
    }
    return true;
}

/** Original menu provider 69; strings are BIG resources except the native
 * GetTimeIntervalString printf patterns at ARM VA 0x3c4980/0x3c49c4. */
std::string RefineryNumber(ZMenuSurface &view, const char *name, std::uint64_t value) {
    std::string text = view.movies.NamedString(name);
    std::size_t offset = text.find("%i");
    if (offset == std::string::npos) { offset = text.find("%d"); }
    if (offset == std::string::npos) {
        std::printf("[refinery] unsupported number format resource=%s text=%s\n", name, text.c_str());
        return {};
    }
    text.replace(offset, 2, std::to_string(value));
    offset = text.find("%%");
    if (offset != std::string::npos) { text.replace(offset, 2, "%"); }
    return text;
}
} // namespace MenuDetail
