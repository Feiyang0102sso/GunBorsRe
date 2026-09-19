#include "gun_bros_re/ui/host/ZStoreRegionClip.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/menus/CMenuList.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
namespace MenuDetail {

/** CMenuDataProvider::CreateContentString :151507 resolves the action only
 * when the corresponding original MDS string slot is null. */
std::string OptionsText(ZMenuSurface &view, const CProfileManager &profile, unsigned index, unsigned slot, const char *table ) {
    const auto *entry = CMenuDataProvider::Find(table, index);
    if (entry == nullptr || slot >= 4) { return {}; }
    if (entry->strings[slot][0] != 0) { return view.movies.NamedString(entry->strings[slot]); }
    const char *name = nullptr;
    switch (entry->action) {
    case 9:
        name = "IDS_SOUND_OFF_GAME";
        if (profile.soundEnabled) { name = "IDS_SOUND_ON_GAME"; }
        break;
    case 10:
        name = "IDS_MUSIC_OFF_GAME";
        if (profile.musicEnabled) { name = "IDS_MUSIC_ON_GAME"; }
        break;
    case 17:
        if (profile.options.AutoBro() == 0) { name = "IDS_BROTHER_AUTOSELECT_OFF"; }
        if (profile.options.AutoBro() == 1) { name = "IDS_BROTHER_AUTOSELECT_ON"; }
        if (profile.options.AutoBro() == 2) { name = "IDS_BROTHER_AUTOSELECT_ASK"; }
        break;
    case 18:
        // Utility::LoadAboutText :105469 and GetTimestampString :52021.
        // Bundle version metadata and remote identity are absent from this set.
        return view.movies.NamedString("IDS_ABOUT_TEXT_FORMATTED") +
            view.movies.NamedString("IDS_COPYRIGHT_TEXT_FORMATTED") +
            "\n\n\n\n\nName: GUNBROS_20121104-163000 \nNov  4 2012 16:52:55";
    case 19:
        name = "IDS_SAVE_STATUS_BODY1"; // Offline profile has not synced to NGS.
        break;
    case 20:
        name = "IDS_FACEBOOK_LOGIN"; // No remote login is synthesized.
        break;
    case 77:
        name = "IDS_NOTIF_TOGGLE_OFF_GAME";
        if (profile.options.NotificationsEnabled()) { name = "IDS_NOTIF_TOGGLE_ON_GAME"; }
        break;
    case 113:
        name = "IDS_CHALLENGE_PUSH_TOGGLE_OFF_GAME";
        if (profile.pushChallenges) { name = "IDS_CHALLENGE_PUSH_TOGGLE_ON_GAME"; }
        break;
    }
    if (name == nullptr) { return {}; }
    return view.movies.NamedString(name);
}

/** MENU_OPTIONS at original VA 0x402e50 selects LIST_MENU, list offset 2,
 * bounds 1/1, LIST_MENU_BUTTON, LIST_MENU_TEXT; all geometry stays in BIG.
 * CMenuList :140175..140657, CMenuListOption :144029..144317, ui_movie.bt. */
bool CMenuList::Draw(ZMenuSurface &view, CMenuSystem &state, CProfileManager &profile, bool &saveChanged) {
    const char *table = "MDS_OPTIONS";
    if (state.stack.page == 8) { table = "MDS_HELP"; }
    const unsigned list = view.movies.Ordinal("GLU_MOVIE_LIST_MENU");
    const unsigned button = view.movies.Ordinal("GLU_MOVIE_LIST_MENU_BUTTON");
    const unsigned textMovie = view.movies.Ordinal("GLU_MOVIE_LIST_MENU_TEXT");
    const CMovie *listData = view.movies.GetMovie(list);
    const CMovie *buttonData = view.movies.GetMovie(button);
    const CMovie *textData = view.movies.GetMovie(textMovie);
    unsigned start = 0, end = 0, focusStart = 0, focusEnd = 0, bodyStart = 0, bodyEnd = 0;
    unsigned restStart = 0, restEnd = 0;
    if (listData == nullptr || buttonData == nullptr || textData == nullptr ||
        !listData->GetChapterRange(1, start, end) || !buttonData->GetChapterRange(2, focusStart, focusEnd) ||
        !buttonData->GetChapterRange(0, restStart, restEnd) ||
        !textData->GetChapterRange(0, bodyStart, bodyEnd) || end <= start) { return false; }
    unsigned count = 0;
    while (CMenuDataProvider::Find(table, count) != nullptr) { ++count; }
    if (count < 3) { return false; }
    unsigned elapsed = 0;
    if (!state.settings.optionsBound) {
        state.settings.optionsBound = true;
        state.settings.optionsOpening = 0;
        if (!view.animateNavigation) { state.settings.optionsOpening = start; }
        state.settings.optionsButtonTimes.assign(count, 0);
        state.settings.optionsFocus = std::min(state.settings.optionsFocus, count - 1);
        state.settings.optionsButtonTimes[state.settings.optionsFocus] = focusStart;
        state.settings.optionsTarget = std::clamp(state.settings.optionsScroll, 0.0f, float(count - 3));
    } else if (view.clock >= state.settings.optionsLastTick) {
        elapsed = static_cast<unsigned>(view.clock - state.settings.optionsLastTick);
    }
    state.settings.optionsLastTick = view.clock;
    // CMenuList::Update :140398 advances shared background 9 by local delta.
    backgroundTime += elapsed;
    state.settings.optionsOpening = std::min(start, state.settings.optionsOpening + elapsed);
    state.settings.optionsBodyTime = std::min(bodyEnd, state.settings.optionsBodyTime + elapsed);
    ZMovieRegion input, content, scrollBar;
    if (!view.movies.Region(list, 0, start, input) || !view.movies.Region(list, 8, start, content) ||
        !view.movies.Region(list, 9, start, scrollBar)) { return false; }
    // CalculateBaseVelocity :141749 averages only the moving region centers.
    // Wheel movement is a Windows input adapter expressed in original options.
    float distance = 0;
    unsigned moving = 0;
    const auto before = view.movies.Regions(list, start);
    const auto after = view.movies.Regions(list, end);
    for (const auto &region : before) {
        if (region.type < 2) { continue; }
        for (const auto &last : after) {
            if (last.index != region.index) { continue; }
            const float difference = region.y + region.height / 2 - last.y - last.height / 2;
            if (difference != 0) { distance += difference; ++moving; }
        }
    }
    if (moving == 0 || distance == 0) { return false; }
    distance = std::abs(distance / moving);
    const float wheel = view.window.TakeWheelDelta();
    if (view.inputEnabled && state.settings.optionsOpening == start && view.MouseIn(input.x, input.y, input.width, input.height)) {
        if (view.dragY != 0) {
            state.settings.optionsScroll = std::clamp(state.settings.optionsScroll - view.dragY / distance, 0.0f, float(count - 3));
            state.settings.optionsTarget = std::round(state.settings.optionsScroll);
        }
        if (wheel != 0) { state.settings.optionsTarget = std::clamp(state.settings.optionsTarget - wheel, 0.0f, float(count - 3)); }
    }
    if (view.dragY == 0) {
        const float step = float(elapsed) / (end - start + 1);
        if (state.settings.optionsScroll < state.settings.optionsTarget) { state.settings.optionsScroll = std::min(state.settings.optionsTarget, state.settings.optionsScroll + step); }
        else { state.settings.optionsScroll = std::max(state.settings.optionsTarget, state.settings.optionsScroll - step); }
    }
    state.settings.optionsScroll = std::clamp(state.settings.optionsScroll, 0.0f, float(count - 3));
    const int base = static_cast<int>(std::floor(state.settings.optionsScroll)) - 1;
    unsigned time = state.settings.optionsOpening;
    if (time == start) { time += static_cast<unsigned>((state.settings.optionsScroll - std::floor(state.settings.optionsScroll)) * (end - start)); }
    if (!view.movies.DrawNamed("GLU_MOVIE_BG_OPTIONS", backgroundTime) ||
        !view.movies.Draw(list, time)) { return false; }
    if (state.stack.page == 8) {
        // MENU_HELP uses the same LIST_MENU but binds BACK at the widget slot.
        const auto regions = view.movies.Regions(list, time, 512, 384, true);
        if (regions.size() < 3) { return false; }
        const auto *back = CMenuDataProvider::Find("MDS_BUTTON_BACK", 0);
        for (auto region : regions) {
            if (region.index != regions.size() - 3) { continue; }
            region.x += region.width / 2;
            region.y += region.height / 2;
            bool pressed = false;
            if (!back || !CMenuMovieButton::DrawFrame(view, *back, region, {}, 0,
                state.settings.optionsOpening == start, pressed)) { return false; }
            if (pressed) { state.Back(); return true; }
        }
    }
    int selected = -1;
    for (const auto &region : view.movies.Regions(list, time)) {
        if (region.type < 2) { continue; }
        const int index = base + static_cast<int>(region.type) - 2;
        if (index < 0 || index >= static_cast<int>(count)) { continue; }
        unsigned &buttonTime = state.settings.optionsButtonTimes[index];
        if (state.settings.optionsFocus == static_cast<unsigned>(index)) {
            buttonTime = focusStart + (buttonTime - focusStart + elapsed) % (focusEnd - focusStart + 1);
        } else { buttonTime = std::min(restEnd, buttonTime + elapsed); }
        const float x = region.x + static_cast<int>(region.width) / 2;
        const float y = region.y + static_cast<int>(region.height) / 2;
        if (!view.movies.Draw(button, buttonTime, x, y)) { return false; }
        for (const auto &part : view.movies.Regions(button, buttonTime, x, y)) {
            if (part.index == 1) {
                view.movies.Text(OptionsText(view, profile, index, 0, table), part.x,
                    part.y + static_cast<int>(part.height) / 2 - static_cast<int>(view.movies.TextHeight(0)) / 2,
                    0, 1, 0, region.alpha * part.alpha);
            }
            if (part.index == 0 && state.settings.optionsOpening == start &&
                view.Hit(part.x, part.y, part.width, part.height)) { selected = index; }
        }
    }
    if (selected >= 0) {
        if (state.settings.optionsFocus != static_cast<unsigned>(selected)) {
            state.settings.optionsButtonTimes[state.settings.optionsFocus] = 0;
            state.settings.optionsFocus = selected;
            state.settings.optionsButtonTimes[selected] = focusStart;
            state.settings.optionsTarget = std::clamp(float(selected - 1), 0.0f, float(count - 3));
            state.settings.optionsBodyTime = 0;
            state.settings.optionsScrollbarTime = 0;
            state.settings.optionsBodyScroll = 0;
        }
        const auto *entry = CMenuDataProvider::Find(table, selected);
        switch (entry->action) {
        case 9: profile.soundEnabled = !profile.soundEnabled; saveChanged = true; break;
        case 10: profile.musicEnabled = !profile.musicEnabled; saveChanged = true; break;
        case 17:
            profile.options.CycleAutoBro();
            profile.brotherEnabled = profile.options.AutoBro() != 0;
            saveChanged = true;
            break;
        case 77: profile.options.ToggleNotifications(); saveChanged = true; break;
        case 113: profile.pushChallenges = !profile.pushChallenges; saveChanged = true; break;
        case 19: saveChanged = true; break;
        case 1: state.Navigate(8); return true;
        case 79: state.Navigate(29); break;
        case 20:
            // DoAction 20 :93872 requests the platform login/logout; no account page.
            std::printf("[options] Facebook platform unavailable; profile unchanged\n");
            break;
        }
    }
    // CMenuList::Bind :140481 installs fonts 0 and 6. The original body already
    // contains its title and font controls; never synthesize a second heading.
    const auto lines = CTextBox::Format(view.movies, OptionsText(view, profile, state.settings.optionsFocus, 1, table),
        content.width - scrollBar.width, {0, 6, 0, 0, 0});
    ZMovieRegion pageBounds;
    if (!view.movies.Region(textMovie, 2, 0, pageBounds) || pageBounds.height <= 0) { return false; }
    std::vector<std::vector<CTextBox::Line>> pages(1);
    float pageHeight = 0;
    for (const auto &line : lines) {
        if (!pages.back().empty() && pageHeight + line.height > pageBounds.height) {
            pages.push_back({});
            pageHeight = 0;
        }
        pages.back().push_back(line);
        pageHeight += line.height;
    }
    const float maxPage = static_cast<float>(pages.size() - 1);
    if (view.inputEnabled && view.MouseIn(content.x, content.y, content.width, content.height)) {
        state.settings.optionsBodyScroll = std::clamp(state.settings.optionsBodyScroll - view.dragY / pageBounds.height - wheel,
            0.0f, maxPage);
    }
    state.settings.optionsBodyScroll = std::clamp(state.settings.optionsBodyScroll, 0.0f, maxPage);
    const int firstPage = static_cast<int>(std::floor(state.settings.optionsBodyScroll));
    unsigned textStart = 0, textEnd = 0;
    if (!textData->GetChapterRange(1, textStart, textEnd)) { return false; }
    unsigned textTime = state.settings.optionsBodyTime;
    if (state.settings.optionsBodyTime == bodyEnd) {
        textTime = textStart + static_cast<unsigned>((state.settings.optionsBodyScroll - firstPage) * (textEnd - textStart));
    }
    if (!view.movies.Draw(textMovie, textTime, content.x, content.y)) { return false; }
    ZMovieRegion animatedContent;
    if (!view.movies.Region(list, 8, time, animatedContent)) { return false; }
    {
        ZStoreRegionClip clip(view, animatedContent);
        for (const auto &region : view.movies.Regions(textMovie, textTime, content.x, content.y)) {
            if (region.type < 2) { continue; }
            const int pageIndex = firstPage + static_cast<int>(region.type) - 2;
            if (pageIndex < 0 || pageIndex >= static_cast<int>(pages.size())) { continue; }
            float y = region.y;
            for (const auto &line : pages[pageIndex]) {
                for (const auto &run : line.runs) {
                    view.movies.Text(run.text, region.x + run.x, y + (line.height - run.height) / 2,
                        run.font, 1, 0, animatedContent.alpha * region.alpha);
                }
                y += line.height;
            }
        }
    }
    if (pages.size() > 1) {
        // ScrollBarCallback :140195 centers the original scrollbar by its
        // own region-0 height. SetProgress :221526 scales by Movie duration.
        const auto *entry = CMenuDataProvider::Find("MDS_SCROLLBARS", 1);
        if (entry == nullptr) { return false; }
        const unsigned ordinal = view.movies.Ordinal(entry->movies[0]);
        const CMovie *bar = view.movies.GetMovie(ordinal);
        ZMovieRegion bounds;
        if (bar == nullptr || !view.movies.Region(ordinal, 0, 0, bounds)) { return false; }
        const unsigned target = static_cast<unsigned>(bar->duration * state.settings.optionsBodyScroll / maxPage);
        const unsigned advance = static_cast<unsigned>(elapsed * 1.2f); // SetItemCount(1) :221427 => 2 - 4/5.
        if (state.settings.optionsScrollbarTime < target) { state.settings.optionsScrollbarTime = std::min(target, state.settings.optionsScrollbarTime + advance); }
        else { state.settings.optionsScrollbarTime -= std::min(advance, state.settings.optionsScrollbarTime - target); }
        if (!view.movies.Draw(ordinal, state.settings.optionsScrollbarTime, scrollBar.x,
            scrollBar.y + static_cast<int>(scrollBar.height) / 2 - static_cast<int>(bounds.height) / 2)) { return false; }
    }
    return true;
}

} // namespace MenuDetail
