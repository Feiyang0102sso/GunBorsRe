#include "gun_bros_re/ui/StoreRegionClip.h"
#include "gun_bros_re/ui/MenuInternal.h"
namespace MenuDetail {
class RefineryCallbacks : public IMovieRegionCallback {
public:
    RefineryCallbacks(GameMenu &menu, MenuState &selection, CProfileManager &account,
        const CRefinementManager::Template &resources, unsigned cells, bool ready)
        : view(menu), state(selection), profile(account), data(resources), count(cells), interactive(ready) {}

    bool DrawMovieRegion(const MovieRegion &region) override {
        if (region.index < count) { return Meter(region); }
        if (region.index < count * 2) { return MeterInfo(region); }
        if (region.index == count * 2) { return Xplodium(region); }
        if (region.index == count * 2 + 1) {
            DrawMissionText(view, region, view.movies.NamedString("IDS_RESMAN_SIDEBARINFO"), 1);
        }
        if (region.index == count * 2 + 4) { return Categories(region); }
        return true;
    }

    bool Categories(const MovieRegion &region) {
        // CategoryButtonCallback :172186 draws entry1 then entry0, with 4px
        // between them and total width 2*graphicWidth+8, exactly as the source.
        const auto *first = OriginalMenuData("MDS_BUTTON_REFINE_SLOT_CATEGORY", 1);
        if (first == nullptr) { return false; }
        const unsigned movie = view.movies.Ordinal(first->movies[0]);
        MovieRegion graphic;
        if (!view.movies.Region(movie, 1, 0, graphic)) { return false; }
        float x = region.x + region.width / 2 - (graphic.width * 2 + 8) / 2;
        for (unsigned position = 0; position < 2; ++position) {
            const auto *entry = OriginalMenuData("MDS_BUTTON_REFINE_SLOT_CATEGORY", 1 - position);
            if (entry == nullptr) { return false; }
            MovieRegion area = region;
            area.x = x;
            bool pressed = false;
            unsigned chapter = 2;
            if (state.refinery.refineryTab == entry->index) { chapter = 3; }
            if (!DrawOriginalMovieButton(view, *entry, area, view.movies.NamedString(entry->strings[0]), 5,
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

    bool Xplodium(const MovieRegion &region) {
        const auto *entry = OriginalMenuData("MDS_ICON_STANDARD", 0);
        if (entry == nullptr) { return false; }
        const unsigned sprite = entry->sprites[0];
        MovieRegion bounds;
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

    bool MeterInfo(const MovieRegion &region) {
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
        // Native offline profile has no validated friend power. Provider 69/3
        // alternates only on enabled meters, and only for a nonzero payout.
        if (data.minutes[slot] == 0 && amount != 0 && (state.refinery.refineryElapsed / 2000) % 2 != 0) {
            const auto payout = static_cast<std::uint64_t>(std::floor(static_cast<float>(amount) * data.efficiencyPercent[slot] / 100.0f + 0.5f));
            detail = RefineryNumber(view, "IDS_SHOP_COMMON", payout);
            font = 0;
        }
        if (title.empty() || detail.empty()) { return false; }
        view.movies.Text(title, region.x + (region.width - view.movies.TextWidth(title, 0)) / 2, region.y, 0, 1, 0, region.alpha);
        view.movies.Text(detail, region.x + (region.width - view.movies.TextWidth(detail, font)) / 2,
            region.y + region.height - view.movies.TextHeight(font), font, 1, 0, region.alpha);
        return true;
    }

    bool Meter(const MovieRegion &region) {
        const unsigned slot = state.refinery.refineryTab * count + region.index;
        const auto &record = profile.refinery.slots[slot];
        const auto *entry = OriginalMenuData("MDS_BUTTON_XPLODIUM_METER", slot);
        if (entry == nullptr) { return false; }
        const float x = region.x + region.width / 2, y = region.y + region.height / 2;
        const bool enabled = data.minutes[slot] == 0;
        const unsigned fill = view.movies.Ordinal("GLU_MOVIE_BUCKET_FILL");
        const unsigned status = view.movies.Ordinal(entry->movies[1]);
        if (!view.movies.Draw(fill, state.refinery.refineryFillTime[slot], x, y)) { return false; }
        {
            // The native button movie is centered on the meter. Its own hit
            // region owns interaction; the 220px parent is not a click target.
            MovieRegion button = region;
            button.x = x;
            button.y = y;
            bool pressed = false;
            unsigned chapter = 0;
            if (record.state != 0 && enabled) { chapter = 2; }
            // Enabled(false) sets button state6, which still draws; Draw skips
            // only state8 (:144707). Keep the original circle behind the lock.
            if (!DrawOriginalMovieButton(view, *entry, button, {}, 0,
                enabled && record.state != 0 && interactive && state.refinery.refineryTransfer < 0,
                pressed, chapter, state.refinery.refineryElapsed * 2)) { return false; }
            if (pressed && (record.state == 1 || (record.state == 3 && state.refinery.refineryStatusChapter[slot] == 3))) {
                if (record.state == 3 || profile.xplodium != 0) {
                    const unsigned main = view.movies.Ordinal("GLU_MOVIE_EXPLODIUM");
                    MovieRegion source, destination;
                    unsigned icon = 0;
                    if (record.state == 3) {
                        source.x = x;
                        source.y = y;
                        if (!view.movies.Region(main, count * 2 + 2, state.refinery.refineryTime, destination)) { return false; }
                        icon = 2;
                        state.refinery.refineryTransferAmount = profile.refinery.GetRefinementSlotYield(slot);
                    } else {
                        if (!view.movies.Region(main, count * 2, state.refinery.refineryTime, source)) { return false; }
                        const auto *image = OriginalMenuData("MDS_ICON_STANDARD", 0);
                        MovieRegion bounds;
                        if (image == nullptr || !view.movies.SpriteBounds(image->sprites[0] >> 16, image->sprites[0] & 255, bounds)) { return false; }
                        source.x += bounds.width / 2;
                        source.y += bounds.height / 2;
                        destination.x = x;
                        destination.y = y;
                        state.refinery.refineryTransferAmount = profile.xplodium;
                    }
                    const auto *image = OriginalMenuData("MDS_ICON_STANDARD", icon);
                    if (image == nullptr) { return false; }
                    if (!view.StartRefineryEffect(slot, icon, source.x, source.y)) { return false; }
                    state.refinery.refineryTransfer = static_cast<int>(slot);
                    state.refinery.refineryTransferTime = 0;
                    state.refinery.refineryTransferSprite = image->sprites[0];
                    state.refinery.refineryTransferX = source.x;
                    state.refinery.refineryTransferY = source.y;
                    state.refinery.refineryTargetX = destination.x;
                    state.refinery.refineryTargetY = destination.y;
                }
            }
        }
        if (!view.movies.Draw(status, state.refinery.refineryStatusTime[slot], x, y)) { return false; }
        if (record.state == 0) {
            // CreateContentSprite(69,0) :149780, original SG constants:
            // archetype4, first animation24, count6. Index chooses lock art.
            // CResourceMeter::Update advances this sprite only AFTER unlock.
            // Its initial colored chamber stays frozen while state==0.
            if (!view.movies.DrawSprite(4, 24 + slot % 6, 0, x, y, 1, region.alpha)) { return false; }
            const unsigned overlay = view.movies.Ordinal("GLU_MOVIE_CHAMBER_OVERLAY");
            const CMovie *movie = view.movies.GetMovie(overlay);
            unsigned start = 0, end = 0;
            if (movie == nullptr || !movie->GetChapterRange(0, start, end) ||
                !view.movies.Draw(overlay, start + state.refinery.refineryElapsed % (end - start + 1), x, y)) { return false; }
        }
        std::string text;
        if (!enabled) { text = view.movies.NamedString("IDS_FRIEND_OFFLINE"); }
        else if (record.state == 3) { text = view.movies.NamedString("IDS_RESMAN_COLLECT"); }
        else if (record.state == 1 && profile.xplodium != 0) { text = view.movies.NamedString("IDS_RESMAN_READY"); }
        if (!text.empty()) {
            view.movies.Text(text, x - view.movies.TextWidth(text, 0) / 2, y - view.movies.TextHeight(0) / 2, 0, 1, 0, region.alpha);
        }
        return true;
    }
    GameMenu &view;
    MenuState &state;
    CProfileManager &profile;
    const CRefinementManager::Template &data;
    unsigned count;
    bool interactive;
};
// Page callback implementations.

bool SetRefineryStatus(GameMenu &view, MenuState &state, unsigned slot, unsigned chapter) {
    const auto *entry = OriginalMenuData("MDS_BUTTON_XPLODIUM_METER", slot);
    if (entry == nullptr) { return false; }
    const auto *movie = view.movies.GetMovie(view.movies.Ordinal(entry->movies[1]));
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(chapter, start, end)) { return false; }
    state.refinery.refineryStatusChapter[slot] = chapter;
    state.refinery.refineryStatusTime[slot] = start;
    return true;
}

bool DrawRefinery(GameMenu &view, MenuState &state, CProfileManager &profile,
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
        view.ResetRefineryEffects();
        state.refinery.refineryBound = true;
        state.refinery.refineryTab = 1;
        state.refinery.refineryTime = 0;
        state.refinery.refineryElapsed = 0;
        state.refinery.refineryLastTick = view.clock;
        for (unsigned slot = 0; slot < count * 2; ++slot) {
            unsigned chapter = 0;
            if (data.minutes[slot] == 0) { chapter = profile.refinery.slots[slot].state; }
            if (!SetRefineryStatus(view, state, slot, chapter)) { return false; }
            state.refinery.refineryFillTime[slot] = 0;
        }
    }
    const unsigned delta = static_cast<unsigned>(view.clock - state.refinery.refineryLastTick);
    state.refinery.refineryLastTick = view.clock;
    view.AdvanceRefineryEffects(delta);
    state.refinery.refineryElapsed += delta;
    const std::uint64_t next = static_cast<std::uint64_t>(state.refinery.refineryTime) + delta;
    state.refinery.refineryTime = static_cast<unsigned>(next);
    if (next > idleEnd) { state.refinery.refineryTime = idleStart + static_cast<unsigned>((next - idleStart) % (idleEnd - idleStart + 1)); }
    if (!view.animateNavigation) { state.refinery.refineryTime = idleStart; }
    if (state.refinery.refineryTransfer >= 0) {
        state.refinery.refineryTransferTime += delta;
        const float fraction = std::min(1.0f, state.refinery.refineryTransferTime / 375.0f);
        view.MoveRefineryEffect(static_cast<unsigned>(state.refinery.refineryTransfer),
            std::trunc(state.refinery.refineryTransferX + (state.refinery.refineryTargetX - state.refinery.refineryTransferX) * fraction),
            std::trunc(state.refinery.refineryTransferY + (state.refinery.refineryTargetY - state.refinery.refineryTransferY) * fraction));
        // CTransferEffect::Setup :174380: original linear x/y duration375ms.
        if (state.refinery.refineryTransferTime >= 375) {
            const unsigned slot = static_cast<unsigned>(state.refinery.refineryTransfer);
            view.StopRefineryEffect(slot);
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
                if (data.minutes[index] == 0 && profile.refinery.slots[index].state == 3) { hasReady = true; }
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
        const auto *entry = OriginalMenuData("MDS_BUTTON_XPLODIUM_METER", slot);
        if (entry == nullptr) { return false; }
        const auto *status = view.movies.GetMovie(view.movies.Ordinal(entry->movies[1]));
        unsigned start = 0, end = 0;
        const unsigned chapter = state.refinery.refineryStatusChapter[slot];
        if (status == nullptr || !status->GetChapterRange(chapter, start, end)) { return false; }
        unsigned time = state.refinery.refineryStatusTime[slot];
        if (chapter >= 2) { time = start + (time - start + delta) % (end - start + 1); }
        else { time += std::min(delta, end - time); }
        state.refinery.refineryStatusTime[slot] = time;
        if (profile.refinery.slots[slot].state == 3) {
            state.refinery.refineryFillTime[slot] += std::min(delta * 2, fill->duration - state.refinery.refineryFillTime[slot]);
            if (chapter == 2 && state.refinery.refineryFillTime[slot] == fill->duration &&
                !SetRefineryStatus(view, state, slot, 3)) { return false; }
        }
    }
    if (!view.movies.DrawNamed("GLU_MOVIE_EXPLODIUM_BG", state.refinery.refineryElapsed)) { return false; }
    RefineryCallbacks callbacks(view, state, profile, data, count, state.refinery.refineryTime >= idleStart);
    if (!view.movies.Draw(ordinal, state.refinery.refineryTime, 512, 384, 1024, 768, 0, 1, &callbacks)) { return false; }
    return true;
}

bool DrawRefineryOverlay(GameMenu &view, const MenuState &state) {
    // CMenuSystem::Draw :96837 calls the current menu overlay after the header.
    view.DrawRefineryEffects();
    if (state.refinery.refineryTransfer >= 0) {
        const float fraction = std::min(1.0f, state.refinery.refineryTransferTime / 375.0f);
        const float x = std::trunc(state.refinery.refineryTransferX + (state.refinery.refineryTargetX - state.refinery.refineryTransferX) * fraction);
        const float y = std::trunc(state.refinery.refineryTransferY + (state.refinery.refineryTargetY - state.refinery.refineryTransferY) * fraction);
        const unsigned sprite = state.refinery.refineryTransferSprite;
        MovieRegion bounds;
        float alpha = 1;
        if (state.refinery.refineryTransferTime > 188) { alpha -= std::min(0.5f, (state.refinery.refineryTransferTime - 188) / 374.0f); }
        if (!view.movies.SpriteBounds(sprite >> 16, sprite & 255, bounds) ||
            !view.movies.DrawSprite(sprite >> 16, sprite & 255, state.refinery.refineryTransferTime, x, y, 1, alpha)) { return false; }
        view.movies.Text(std::to_string(static_cast<std::int32_t>(state.refinery.refineryTransferAmount)), x + bounds.width / 2, y, 0, 1, 0, alpha);
    }
    return true;
}

/** CMenuFriends::Bind :197028 and CMenuChallenges::Bind :236612 select
 * chapter 1 while profile validity is false. Region 0 owns button 165/0,
 * region 1 owns centered font-0 text. No host flag can validate an NGS user.
 * ui_movie.bt and MENU_CHALLENGES VA 0x402eb0 identify the original Movie. */
bool DrawOriginalSocialOffline(GameMenu &view, MenuState &state, bool hasCredentials) {
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_OFFLINE_BROHOOD");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, start, end)) { return false; }
    if (!state.social.socialBound) {
        state.social.socialBound = true;
        state.social.socialTime = start;
        state.social.socialLastTick = view.clock;
    }
    const auto elapsed = view.clock - state.social.socialLastTick;
    state.social.socialLastTick = view.clock;
    state.social.socialTime = start + static_cast<unsigned>((state.social.socialTime - start + elapsed) % (end - start + 1));
    if (!view.movies.Draw(ordinal, state.social.socialTime)) { return false; }
    const char *table = "MDS_OFFLINE_CHALLENGES";
    if (state.page == 4) { table = "MDS_OFFLINE_FRIENDS"; }
    // GetElementValueInt32(81) :150866: invalid+credentials=>1, absent=>2;
    // each menu subtracts one to index its two original description strings.
    unsigned description = 1;
    if (hasCredentials) { description = 0; }
    const auto *entry = OriginalMenuData(table, description);
    const auto *button = OriginalMenuData("MDS_BUTTON_CONNECTIVITY", 0);
    if (entry == nullptr || button == nullptr) { return false; }
    for (const auto &region : view.movies.Regions(ordinal, state.social.socialTime)) {
        if (region.index == 0) {
            bool pressed = false;
            if (!DrawOriginalMovieButton(view, *button, region, view.movies.NamedString(button->strings[0]), 6, true, pressed)) { return false; }
            if (pressed) {
                // Action 86 requests NGS connectivity. The absent remote service
                // cannot mint a validated identity, friendship or reward locally.
                std::printf("[social] connectivity action=%u unavailable; profile remains offline\n", button->action);
            }
        }
        if (region.index == 1) {
            const auto lines = FormatStoreText(view.movies, view.movies.NamedString(entry->strings[0]), region.width, {0, 0, 0, 0, 0});
            StoreRegionClip clip(view, region);
            float y = region.y;
            for (const auto &line : lines) {
                const float x = region.x + (region.width - line.width) / 2;
                for (const auto &run : line.runs) {
                    view.movies.Text(run.text, x + run.x, y, run.font, 1, 0, region.alpha);
                }
                y += line.height;
            }
        }
    }
    return true;
}
} // namespace MenuDetail
