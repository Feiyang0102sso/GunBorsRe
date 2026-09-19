#include "gun_bros_re/ui/menus/CMenuPostGameOption.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
namespace MenuDetail {
bool DrawLivePostGameList(ZMenuSurface &view, CMenuSystem &state, const ZMovieRegion &region, bool interactive);

class ZPostGameListCallbacks : public ZMovieRegionCallback {
public:
    ZPostGameListCallbacks(ZMenuSurface &menu, CMenuSystem &selection, CResTOCManager &manager, CGunBros &resources)
        : view(menu), state(selection), toc(manager), tables(resources) {}
    bool DrawMovieRegion(const ZMovieRegion &region) override {
        if (state.ContentPage() == 28) {
            if (region.index < 1 || region.index > 4) { return true; }
            const int index = static_cast<int>(std::floor(state.postGame.postGameGalleryPosition)) + static_cast<int>(region.index) - 1;
            if (index < 0 || index >= static_cast<int>(state.result.casualties.size())) { return true; }
            class CasualtyCallback : public ZMovieRegionCallback {
            public:
                CasualtyCallback(ZPostGameListCallbacks &owner, const CEnemyCasualty &value) : list(owner), casualty(value) {}
                bool DrawMovieRegion(const ZMovieRegion &area) override {
                    return list.view.DrawCasualty(list.tables, list.toc, casualty, 0, &area);
                }
                ZPostGameListCallbacks &list;
                const CEnemyCasualty &casualty;
            } callback(*this, state.result.casualties[index]);
            const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_MODEL_GALLERY_ITEM");
            const auto *movie = view.movies.GetMovie(ordinal);
            if (movie == nullptr) { return false; }
            return view.movies.Draw(ordinal, std::min(state.postGame.postGameItemTime, movie->duration),
                region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &callback);
        }
        if (region.index < 1 || region.index > 2) { return true; }
        const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_WRAPUP_BOX");
        ZMovieRegion bounds;
        const auto *movie = view.movies.GetMovie(ordinal);
        if (movie == nullptr || !view.movies.Region(ordinal, 0, 0, bounds)) { return false; }
        const unsigned first = (region.index - 1) * 2;
        for (unsigned index = first; index < std::min(3u, first + 2); ++index) {
            unsigned icon = index;
            std::uint64_t amount = state.result.xplodium;
            if (index == 1) { amount = state.result.experience; }
            if (index == 2) {
                icon = 4;
                amount = state.result.perfectWaves;
                if (state.result.horde) { icon = 5; amount = state.result.bestKillStreak; }
            }
            const auto *entry = CMenuDataProvider::Find("MDS_ICON_POSTGAME", icon);
            if (entry == nullptr) { return false; }
            float x = region.x;
            if (index == 1) { x += region.width - bounds.width; }
            if (index == 2) { x += static_cast<int>(region.width) / 2 - static_cast<int>(bounds.width) / 2; }
            const std::string value = std::to_string(amount);
            CMenuPostGameOption callback(view, *entry, value, state.postGame.postGameIconTime);
            if (!view.movies.Draw(ordinal, std::min(state.postGame.postGameItemTime, movie->duration),
                x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &callback)) { return false; }
        }
        return true;
    }
    ZMenuSurface &view;
    CMenuSystem &state;
    CResTOCManager &toc;
    CGunBros &tables;
};
// Page callback implementations.

void CMenuPostGame::Refresh(CMenuSystem &state, const CGameFlow &context, const std::vector<CGun::Entry> &weapons) {
    state.result = context.result;
    state.postGame.liveReplay = false;
    state.postGame.liveReplayAt = 0;
    state.postGame.livePosition = 0;
    state.postGame.postGameMusic = true;
    state.refinery.casualtyPage = 0;
    state.refinery.refineryTab = 0;
    state.refinementRequired = context.profile.xplodium != 0;
    state.feedback.Clear();
    state.Navigate(27, true);
    state.postGame.postGameBound = false;
    state.postGame.postGameClosing = false;
    state.postGame.postGameUpgradePending = false;
    if (context.result.deathmatch) { return; }
    // ShowForGuns :394260 prefers the active gun, then the other eligible gun.
    const unsigned activeSlot = context.profile.activeWeaponSlot;
    for (unsigned offset = 0; offset < context.profile.configuration.guns.size(); ++offset) {
        const unsigned slot = (activeSlot + offset) % context.profile.configuration.guns.size();
        const auto &ref = context.profile.configuration.guns[slot];
        const CGun::Entry *weapon = FindMasteryWeapon(weapons, ref);
        if (weapon != nullptr && weapon->data.GetMasteryLevel(context.profile.GetWeaponExperience(ref)) < 3) {
            state.masteryWeapon = ref;
            if (context.profile.nativeArchive) { state.postGame.postGameUpgradePending = true; }
            else { state.Navigate(26); }
            break;
        }
    }
}

// The old fixed fill interval is replaced by CMenuUpgradePopup's original 1x playback.

/** Resource printf substitution for the original CGame result strings. */
std::string PostGameFormat(ZMenuSurface &view, const char *name, const std::vector<std::string> &values) {
    std::string text = view.movies.NamedString(name);
    std::size_t cursor = 0;
    for (const auto &value : values) {
        cursor = text.find('%', cursor);
        if (cursor == std::string::npos || cursor + 1 >= text.size()) { return {}; }
        const char type = text[cursor + 1];
        if (type != 'i' && type != 'd' && type != 'u' && type != 's') {
            std::printf("[postgame] unsupported format resource=%s value=%s\n", name, text.c_str());
            return {};
        }
        text.replace(cursor, 2, value);
        cursor += value.size();
    }
    return text;
}

/** MENU_POST_GAME_WRAPUP VA0x403350, CMenuPostGame :164559..166204.
 * Native menu/provider logic below; layouts, fonts and artwork stay in BIG. */
bool CMenuPostGame::Draw(ZMenuSurface &view, CMenuSystem &state, CResTOCManager &toc, CGunBros &tables,
    const CProfileManager &profile) {
    const char *screen = "GLU_MOVIE_WRAPUP_SCREEN";
    if (state.result.live) { screen = "GLU_MOVIE_WRAPUP_SCREEN_MP"; }
    const unsigned ordinal = view.movies.Ordinal(screen);
    const auto *movie = view.movies.GetMovie(ordinal);
    unsigned idleStart = 0, idleEnd = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, idleStart, idleEnd)) { return false; }
    if (!state.postGame.postGameBound) {
        state.postGame.postGameBound = true;
        state.postGame.postGameTime = 0;
        state.postGame.postGameItemTime = 0;
        state.postGame.postGameIconTime = 0;
        view.postGameEffects.ResetPostGameEffects();
        state.postGame.postGameCloseTime = 0;
        state.postGame.postGameLastTick = view.clock;
        state.postGame.postGameGalleryPosition = 0;
        if (state.result.casualties.size() <= 2) { state.postGame.postGameGalleryPosition = -1; }
        state.postGame.postGameGalleryVelocity = 0;
        state.postGame.livePosition = 0;
        // Provider74 walks flattened ENEMY order, not the order of first kills.
        for (std::size_t item = 1; item < state.result.casualties.size(); ++item) {
            std::size_t cursor = item;
            while (cursor > 0) {
                const auto &left = state.result.casualties[cursor - 1].resource;
                const auto &right = state.result.casualties[cursor].resource;
                const int leftPack = toc.GetPackIndexFromHash(left.packHash);
                const int rightPack = toc.GetPackIndexFromHash(right.packHash);
                if (leftPack < rightPack || (leftPack == rightPack && left.localIndex <= right.localIndex)) { break; }
                std::swap(state.result.casualties[cursor - 1], state.result.casualties[cursor]);
                --cursor;
            }
        }
    }
    const unsigned delta = static_cast<unsigned>(view.clock - state.postGame.postGameLastTick);
    state.postGame.postGameDelta = delta;
    state.postGame.postGameLastTick = view.clock;
    state.postGame.postGameTime += delta;
    state.postGame.postGameItemTime += delta;
    // CMenuPostGame::UpdateCurrentView :165242 updates active controls only.
    if (state.ContentPage() == 27) {
        state.postGame.postGameIconTime += delta;
        unsigned lastIcon = 4;
        if (state.result.horde) { lastIcon = 5; }
        if (state.result.live) {
            for (unsigned icon : {0u, 1u, 2u, 3u, 4u, 6u, 7u}) {
                if (!view.postGameEffects.AdvancePostGameEffect(7 + icon, delta)) { return false; }
            }
        } else {
            for (unsigned icon : {0u, 1u, lastIcon}) {
                if (!view.postGameEffects.AdvancePostGameEffect(icon, delta)) { return false; }
            }
        }
    }
    if (state.postGame.postGameTime > idleEnd) { state.postGame.postGameTime = idleStart + (state.postGame.postGameTime - idleStart) % (idleEnd - idleStart + 1); }
    const bool ready = state.postGame.postGameTime >= idleStart && !state.postGame.postGameClosing;
    const auto *back = CMenuDataProvider::Find("MDS_BUTTON_POSTGAME_BACK", 0);
    if (back == nullptr) { return false; }
    const auto *backMovie = view.movies.GetMovie(view.movies.Ordinal(back->movies[0]));
    unsigned exitStart = 0, exitEnd = 0;
    unsigned hideStart = 0, hideEnd = 0;
    if (backMovie == nullptr || !backMovie->GetChapterRange(1, exitStart, exitEnd) ||
        !backMovie->GetChapterRange(0, hideStart, hideEnd)) { return false; }
    const unsigned pressedDuration = exitEnd - exitStart + 1;
    if (state.postGame.postGameClosing) {
        state.postGame.postGameCloseTime += delta;
        if (state.postGame.postGameCloseTime >= pressedDuration + hideEnd - hideStart + 1) {
            // DoAction38 :94446 chooses original menu20 if ore remains, 19 otherwise.
            unsigned target = 0;
            if (profile.xplodium != 0) { target = 3; }
            state.postGame.postGameMusic = false;
            state.Navigate(target, true);
            return true;
        }
    }
    class MainCallbacks : public ZMovieRegionCallback {
    public:
        MainCallbacks(ZMenuSurface &menu, CMenuSystem &selection, CResTOCManager &manager, CGunBros &resources, bool enabled)
            : view(menu), state(selection), toc(manager), tables(resources), interactive(enabled) {}
        bool DrawMovieRegion(const ZMovieRegion &region) override {
            if (region.index == 1) {
                if (state.result.live) {
                    // CategoryCallback :165166: overview + original MP buttons.
                    std::vector<const CMenuDataProvider::Entry *> buttons{CMenuDataProvider::Find("MDS_BUTTON_POSTGAME_INFO", 0),
                        CMenuDataProvider::Find("MDS_BUTTON_POSTGAME_LEADERBOARD_MP", 0)};
                    if (state.result.deathmatch) { buttons.push_back(CMenuDataProvider::Find("MDS_BUTTON_POSTGAME_ADD_BRO_MP", 0)); }
                    float totalWidth = 0;
                    std::vector<ZMovieRegion> bounds;
                    for (const auto *entry : buttons) {
                        ZMovieRegion touch;
                        if (entry == nullptr || !view.movies.Region(view.movies.Ordinal(entry->movies[0]), 1, 0, touch)) { return false; }
                        totalWidth += touch.width + 2;
                        bounds.push_back(touch);
                    }
                    float x = region.x + (region.width - totalWidth) / 2;
                    for (unsigned index = 0; index < buttons.size(); ++index) {
                        ZMovieRegion origin = region; origin.x = x;
                        bool pressed = false;
                        unsigned chapter = 2;
                        if (index == 0) { chapter = 3; }
                        // Remote account actions are unavailable to a local opponent.
                        if (!CMenuMovieButton::DrawFrame(view, *buttons[index], origin, view.movies.NamedString(buttons[index]->strings[0]),
                            5, interactive && index == 0, pressed, chapter, state.postGame.postGameItemTime)) { return false; }
                        x += bounds[index].width + 2;
                    }
                    return true;
                }
                const auto *first = CMenuDataProvider::Find("MDS_BUTTON_POSTGAME_INFO", 0);
                if (first == nullptr) { return false; }
                ZMovieRegion bounds;
                if (!view.movies.Region(view.movies.Ordinal(first->movies[0]), 1, 0, bounds)) { return false; }
                // CategoryCallback :165166 uses region1 artwork widths (+48), gap=2.
                float x = region.x + static_cast<int>(region.width) / 2 - static_cast<int>((bounds.width + 2) * 2) / 2;
                for (unsigned index = 0; index < 2; ++index) {
                    const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_POSTGAME_INFO", index);
                    if (entry == nullptr) { return false; }
                    ZMovieRegion origin = region;
                    origin.x = x;
                    bool pressed = false;
                    unsigned chapter = 2;
                    if (state.ContentPage() == 27 + index) { chapter = 3; }
                    unsigned time = UINT32_MAX;
                    if (state.postGame.postGameClosing) {
                        // OnExit -> Hide reverses chapter0, not chapter1's press burst.
                        const auto *button = view.movies.GetMovie(view.movies.Ordinal(entry->movies[0]));
                        const auto *back = CMenuDataProvider::Find("MDS_BUTTON_POSTGAME_BACK", 0);
                        if (back == nullptr) { return false; }
                        const auto *backMovie = view.movies.GetMovie(view.movies.Ordinal(back->movies[0]));
                        unsigned pressStart = 0, pressEnd = 0;
                        if (backMovie == nullptr || !backMovie->GetChapterRange(1, pressStart, pressEnd)) { return false; }
                        unsigned begin = 0, end = 0;
                        if (button == nullptr || !button->GetChapterRange(0, begin, end)) { return false; }
                        if (state.postGame.postGameCloseTime > pressEnd - pressStart) {
                            chapter = 0;
                            const unsigned elapsed = state.postGame.postGameCloseTime - (pressEnd - pressStart + 1);
                            time = end - std::min(elapsed, end - begin);
                        }
                    }
                    if (!CMenuMovieButton::DrawFrame(view, *entry, origin, view.movies.NamedString(entry->strings[0]), 5,
                        interactive, pressed, chapter, state.postGame.postGameItemTime, time, true)) { return false; }
                    if (pressed) { state.Navigate(27 + index); }
                    x += bounds.width + 2;
                }
            } else if (region.index == 2) {
                std::string title;
                const auto &result = state.result;
                if (result.deathmatch) {
                    const char *name = "IDS_WRAPUP_DEATH_MATCH_DRAW";
                    if (result.matchResult == 1) { name = "IDS_WRAPUP_DEATH_MATCH_WIN"; }
                    if (result.matchResult == 2) { name = "IDS_WRAPUP_DEATH_MATCH_LOSS"; }
                    title = view.movies.NamedString(name);
                } else if (result.horde) {
                    const char *name = "IDS_WRAPUP_SCORE";
                    if (result.score != 0 && result.score == result.highScore) { name = "IDS_WRAPUP_NEW_HIGH_SCORE"; }
                    title = PostGameFormat(view, name, {std::to_string(result.score)});
                } else if (result.wavesPerRevolution > 0) {
                    unsigned revolution = result.wave / result.wavesPerRevolution + 1;
                    unsigned wave = result.wave % result.wavesPerRevolution + 1;
                    if (result.wave == result.waveLimit) { revolution = result.waveLimit / result.wavesPerRevolution + 1; wave = result.wavesPerRevolution; }
                    title = PostGameFormat(view, "IDS_WRAPUP_REVOLUTION_WAVE", {std::to_string(revolution), std::to_string(wave)});
                }
                UpgradeCenteredText(view, region, title, 6);
            } else if (region.index == 3) {
                std::string text;
                if (state.result.horde) {
                    const unsigned seconds = state.result.stopwatchMs / 1000;
                    char time[32];
                    // CUtility::TimeToString flags1,1; ARM string VA0x3c3048.
                    std::snprintf(time, sizeof(time), "%.2u:%.2u:%.2u", seconds / 3600, seconds / 60 % 60, seconds % 60);
                    text = PostGameFormat(view, "IDS_WRAPUP_SURVIVAL_TIME", {time});
                } else if (state.result.deathmatch) {
                    text = PostGameFormat(view, "IDS_HUD_DEATH_MATCH_START1", {std::to_string(state.result.matchKillLimit)});
                    if (state.result.matchKillLimit == 0) {
                        text = PostGameFormat(view, "IDS_HUD_DEATH_MATCH_START2", {std::to_string(state.result.matchTimeLimitSeconds / 60)});
                    }
                } else { text = PostGameFormat(view, "IDS_WRAPUP_WAVE_CLEARED", {std::to_string(state.result.waves)}); }
                UpgradeCenteredText(view, region, text, 0);
            } else if (state.result.live && region.index == 7) {
                unsigned index = 0;
                if (!state.online.IsConnected()) { index = 1; }
                const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_POSTGAME_REPLAY_MP", index);
                if (entry == nullptr) { return false; }
                ZMovieRegion origin = region;
                origin.x += region.width / 2; origin.y += region.height / 2;
                bool pressed = false;
                std::string label = view.movies.NamedString(entry->strings[0]);
                if (state.postGame.liveReplay) { label = view.movies.NamedString("IDS_WRAPUP_MP_REPLAY_REQUESTED"); }
                if (!CMenuMovieButton::DrawFrame(view, *entry, origin, label, 1,
                    interactive && state.online.IsConnected() && !state.postGame.liveReplay, pressed)) { return false; }
                if (pressed) { state.postGame.liveReplay = true; state.postGame.liveReplayAt = view.clock; }
            } else if (state.result.live && (region.index == 5 || region.index == 6)) {
                std::string name = "PLAYER";
                if (region.index == 6) { name = state.result.peerName; }
                // PlayerNameCallback :164995 places the status sprite beside
                // the name and alternates ready text every two seconds.
                unsigned status = 0;
                if (state.postGame.liveReplay) { status = 1; }
                const auto *entry = CMenuDataProvider::Find("MDS_POSTGAME_REPLAY_MP", status);
                if (entry == nullptr) { return false; }
                const unsigned sprite = entry->sprites[0];
                ZMovieRegion bounds;
                if (!view.movies.SpriteBounds(sprite >> 16, sprite & 255, bounds)) { return false; }
                if (status == 1 && (state.postGame.postGameIconTime / 2000) % 2 != 0) { name = view.movies.NamedString(entry->strings[0]); }
                const float gap = bounds.width * 1.5f;
                const float x = region.x + region.width / 2 + gap - view.movies.TextWidth(name) / 2;
                if (!view.movies.Text(name, x, region.y + (region.height - view.movies.TextHeight()) / 2, 0, 1, 0, region.alpha) ||
                    !view.movies.DrawSprite(sprite >> 16, sprite & 255, state.postGame.postGameIconTime, x - gap, region.y, 1, region.alpha)) { return false; }
            } else if (region.index == 5) {
                const std::string text = view.movies.NamedString("IDS_WRAPUP_TOTAL_KILLS") + std::to_string(static_cast<std::uint16_t>(state.result.kills));
                view.movies.Text(text, region.x, region.y + (region.height - view.movies.TextHeight(0)) / 2, 0, 1, 0, region.alpha);
            } else if (region.index == 4) {
                if (state.result.live && state.ContentPage() == 27) { return DrawLivePostGameList(view, state, region, interactive); }
                ZPostGameListCallbacks callback(view, state, toc, tables);
                const char *name = "GLU_MOVIE_WRAPUP_MENU_SCROLL";
                if (state.ContentPage() == 28) { name = "GLU_MOVIE_WRAPUP_GALLERY"; }
                const unsigned list = view.movies.Ordinal(name);
                const auto *listMovie = view.movies.GetMovie(list);
                unsigned start = 0, end = 0;
                if (listMovie == nullptr || !listMovie->GetChapterRange(1, start, end)) { return false; }
                // Show selects the final overview row then settles on row0.
                unsigned time = std::min(state.postGame.postGameItemTime, start);
                if (state.ContentPage() == 27) { view.Clip(0, region.y, kMenuWidth, region.height); }
                else {
                    unsigned secondStart = 0, secondEnd = 0;
                    if (!listMovie->GetChapterRange(2, secondStart, secondEnd) || secondStart <= start) { return false; }
                    const unsigned duration = secondStart - start;
                    // CalculateBaseVelocity :141749 averages the authored travel
                    // of ALL type>=2 regions between chapter1 and chapter2.
                    float distance = 0;
                    unsigned changed = 0;
                    for (const auto &first : view.movies.Regions(list, start)) {
                        if (first.type < 2) { continue; }
                        ZMovieRegion last;
                        if (!view.movies.Region(list, first.index, secondStart, last)) { return false; }
                        const int travel = static_cast<int>(first.x + first.width / 2 - last.x - last.width / 2);
                        if (travel != 0) { distance += travel; ++changed; }
                    }
                    if (changed == 0 || distance == 0) { return false; }
                    distance = static_cast<float>(std::abs(static_cast<int>(distance) / static_cast<int>(changed)));
                    const float seconds = state.postGame.postGameDelta / 1000.0f;
                    if (interactive && view.MouseIn(region.x, region.y, region.width, region.height)) {
                        // Wheel is the Windows adapter for one original list option.
                        state.postGame.postGameGalleryPosition -= view.window.TakeWheelDelta();
                        if (view.dragX != 0 && seconds > 0) {
                            const float speed = view.dragX / seconds / (distance * 1000 / duration);
                            state.postGame.postGameGalleryVelocity = std::clamp(-speed, -5.0f, 5.0f);
                        }
                    }
                    if (interactive && seconds > 0) {
                        state.postGame.postGameGalleryPosition += state.postGame.postGameGalleryVelocity * state.postGame.postGameDelta / duration;
                        if (!view.window.IsLeftMouseDown()) {
                            const float slowing = 250000.0f / duration * seconds * seconds / 2;
                            if (state.postGame.postGameGalleryVelocity > 0) { state.postGame.postGameGalleryVelocity = std::max(0.0f, state.postGame.postGameGalleryVelocity - slowing); }
                            else { state.postGame.postGameGalleryVelocity = std::min(0.0f, state.postGame.postGameGalleryVelocity + slowing); }
                        }
                    }
                    // SetBoundsOptions(1,1), Init offset=1; a single enemy is centered.
                    const float maximum = static_cast<float>(std::max(0, static_cast<int>(state.result.casualties.size()) - 2) - 1);
                    const float minimum = std::min(0.0f, maximum);
                    state.postGame.postGameGalleryPosition = std::clamp(state.postGame.postGameGalleryPosition, minimum, maximum);
                    const float fraction = state.postGame.postGameGalleryPosition - std::floor(state.postGame.postGameGalleryPosition);
                    time = start + static_cast<unsigned>(fraction * duration);
                }
                const bool drawn = view.movies.Draw(list, time, region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &callback);
                if (state.ContentPage() == 27) { view.EndClip(); }
                return drawn;
            }
            return true;
        }
        ZMenuSurface &view;
        CMenuSystem &state;
        CResTOCManager &toc;
        CGunBros &tables;
        bool interactive;
    } callback(view, state, toc, tables, ready);
    if (!view.movies.Draw(ordinal, state.postGame.postGameTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callback)) { return false; }
    ZMovieRegion position;
    if (!view.movies.Region(ordinal, 0, state.postGame.postGameTime, position)) { return false; }
    position.x += static_cast<int>(position.width) / 2;
    position.y += static_cast<int>(position.height) / 2;
    bool pressed = false;
    unsigned time = UINT32_MAX;
    unsigned chapter = 0;
    if (ready) { chapter = 2; }
    if (state.postGame.postGameClosing) {
        chapter = 1;
        time = exitStart + state.postGame.postGameCloseTime;
        if (state.postGame.postGameCloseTime >= pressedDuration) {
            chapter = 0;
            time = hideEnd - std::min(state.postGame.postGameCloseTime - pressedDuration, hideEnd - hideStart);
        }
    }
    if (!CMenuMovieButton::DrawFrame(view, *back, position, {}, 0, ready, pressed, chapter, state.postGame.postGameItemTime, time)) { return false; }
    if (pressed) { state.postGame.postGameClosing = true; state.postGame.postGameCloseTime = 0; }
    if (ready && state.postGame.postGameUpgradePending) {
        state.postGame.postGameUpgradePending = false;
        state.Navigate(26);
    }
    return true;
}

/** Standard intervals occupy 6..11; premium intervals occupy 0..5. */

} // namespace MenuDetail
