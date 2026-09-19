#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/menus/CMenuGreeting.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
namespace MenuDetail {
class ZGreetingCallbacks : public ZMovieRegionCallback {
public:
    ZGreetingCallbacks(ZMenuSurface &menu, CMenuSystem &state, CResTOCManager &toc, CGunBros &tables,
        const CDailyBonusTracking &daily, const CProfileManager &profile, bool interactive) :
        view(menu), state(state), toc(toc), tables(tables), daily(daily), profile(profile), interactive(interactive) {}
    bool DrawMovieRegion(const ZMovieRegion &region) override {
        if (region.index <= 2) {
            const auto *entry = CMenuDataProvider::Find("MDS_GREETING_STRINGS", region.index);
            if (entry == nullptr) { return false; }
            const auto text = view.movies.NamedString(entry->strings[0]);
            return view.movies.Text(text, region.x + (region.width - view.movies.TextWidth(text, 6)) / 2,
                region.y, 6, 1, 0, region.alpha);
        }
        if (region.index == 3 || region.index == 4) {
            const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_GREETING_REDIRECTS", region.index - 3);
            if (entry == nullptr) { return false; }
            ZMovieRegion origin = region;
            origin.x += region.width / 2;
            origin.y += region.height / 2;
            bool pressed = false;
            if (!CMenuMovieButton::DrawFrame(view, *entry, origin, {}, 0, interactive, pressed)) { return false; }
            if (pressed) {
                if (entry->parameter == 4) { state.Navigate(5, true); }
                else if (entry->parameter == 3) { state.Navigate(4, true); }
                else { return false; }
            }
        }
        if (region.index == 5 || region.index == 7) {
            unsigned index = 0;
            if (region.index == 7) { index = 1; }
            const auto *entry = CMenuDataProvider::Find("MDS_OFFLINE_GREETING", index);
            if (entry == nullptr) { return false; }
            ZMovieRegion box = region;
            box.height *= 5; // Original offline callback allocates five rows.
            DrawStoreTemplate(view, "^f4" + view.movies.NamedString(entry->strings[0]), box, {});
        }
        if (region.index >= 9 && region.index <= 13) {
            const unsigned index = region.index - 9;
            if (index >= daily.prizes.size()) { return false; }
            const auto &prize = daily.prizes[index];
            CStoreItem::Entry icon;
            icon.data.assets[1] = prize.image;
            if (!view.Icon(toc, tables, icon, region.x, region.y, region.width, region.height,
                region.alpha, false, true)) { return false; }
            // CreateRewardQuantityString :209417, currency priority and XP text.
            std::string quantity;
            if (prize.warbucks != 0) { quantity = "X" + std::to_string(prize.warbucks); }
            else if (prize.coins != 0) { quantity = "X" + std::to_string(prize.coins); }
            else if (prize.experience != 0) { quantity = std::to_string(prize.experience); }
            if (!quantity.empty() && !view.movies.Text(quantity,
                region.x + (region.width - view.movies.TextWidth(quantity, 0)) / 2,
                region.y + region.height - view.movies.TextHeight(0), 0, 1, 0, region.alpha)) { return false; }
        }
        if (region.index >= 14 && region.index <= 18 && profile.dailyConsecutiveDays != 0) {
            const unsigned reward = (profile.dailyConsecutiveDays - 1) % static_cast<unsigned>(daily.prizes.size());
            if (region.index - 14 <= reward && !view.movies.DrawSprite(7, 1, state.greeting.greetingElapsed,
                region.x + region.width / 2, region.y + region.height / 2, 1, region.alpha)) { return false; }
        }
        return true;
    }
    ZMenuSurface &view;
    CMenuSystem &state;
    CResTOCManager &toc;
    CGunBros &tables;
    const CDailyBonusTracking &daily;
    const CProfileManager &profile;
    bool interactive;
};
// Page callback implementations.

bool CMenuGreeting::Draw(ZMenuSurface &view, CMenuSystem &state, CResTOCManager &toc, CGunBros &tables,
    CProfileManager &profile, const CDailyBonusTracking &daily, const std::vector<CStoreItem::Entry> &store,
    CPlayerProgress &progress, const std::filesystem::path &savePath, std::int64_t seconds) {
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_WELCOME_NEW");
    const auto *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, start, end) || daily.prizes.empty()) { return false; }
    if (!state.greeting.greetingBound) {
        state.greeting.greetingBound = true;
        state.greeting.greetingClosing = false;
        state.greeting.greetingExitRequested = false;
        state.greeting.greetingTime = 0;
        state.greeting.greetingElapsed = 0;
        state.greeting.greetingLastTick = view.clock;
        daily.RefreshUsageData(profile, static_cast<std::uint32_t>(seconds));
    }
    const unsigned delta = static_cast<unsigned>(view.clock - state.greeting.greetingLastTick);
    state.greeting.greetingLastTick = view.clock;
    state.greeting.greetingElapsed += delta;
    if (state.greeting.greetingExitRequested && !state.greeting.greetingClosing) {
        // OnExit :208279 invokes action95 once. SetChapter(1,true) seeks the
        // chapter START before SetReverse(true); playback then ends at zero.
        if (daily.IsBonusAvailable(profile, seconds)) {
            if (!daily.CommitBonus(profile, seconds, store) || !profile.SaveToDisk(savePath)) { return false; }
            progress.SetExperience(profile.experience);
        }
        state.greeting.greetingClosing = true;
        state.greeting.greetingTime = start;
    } else if (state.greeting.greetingClosing) {
        state.greeting.greetingTime -= std::min(delta, state.greeting.greetingTime);
        if (state.greeting.greetingTime == 0) {
            state.greeting.greetingBound = false;
            state.Navigate(state.greeting.greetingTarget, true);
            return true;
        }
    } else {
        state.greeting.greetingTime += delta;
        if (state.greeting.greetingTime > end) { state.greeting.greetingTime = start + (state.greeting.greetingTime - start) % (end - start + 1); }
        if (!view.animateNavigation) { state.greeting.greetingTime = start; }
    }
    view.movies.Rectangle(0, 0, kMenuWidth, kMenuHeight, 0, 0, 0);
    const bool interactive = state.greeting.greetingTime >= start && !state.greeting.greetingClosing;
    ZGreetingCallbacks callback(view, state, toc, tables, daily, profile, interactive);
    if (!view.movies.Draw(ordinal, state.greeting.greetingTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callback)) { return false; }
    if (!interactive) { view.inputEnabled = false; }
    return true;
}

} // namespace MenuDetail
