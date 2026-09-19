#include "gun_bros_re/ui/host/ZStorePurchase.h"
#include "gun_bros_re/debug/Capture.h"
/** Exercise the original paid chambers through real Movie hit regions. */
#include "ui/MenuChecks.h"
#include "TestOutput.h"
#include "gun_bros_re/cheats/CheatCodes.h"

bool FinishRefineryClick(ZMenuSurface &view, CMenuSystem &state, CProfileManager &profile,
    const CRefinementManager::Template &data, const std::filesystem::path &path, std::int64_t now, unsigned slot) {
    const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_XPLODIUM_METER", slot);
    const auto *button = view.movies.GetMovie(view.movies.Ordinal(entry->movies[0]));
    unsigned start = 0, end = 0;
    if (!button || !button->GetChapterRange(1, start, end) || !state.refinery.chambers[slot].clickPlaying ||
        state.refinery.refineryTransfer >= 0) { return false; }
    const auto ore = profile.xplodium;
    const auto coins = profile.coins;
    const auto duration = (end - state.refinery.chambers[slot].clickTime + 1) / 2;
    if (duration > 1) {
        view.clock += duration - 1;
        view.Begin();
        if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state) || !state.refinery.chambers[slot].clickPlaying ||
            state.refinery.refineryTransfer >= 0 || profile.xplodium != ore || profile.coins != coins) { return false; }
    }
    ++view.clock;
    view.Begin();
    return FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state) && !state.refinery.chambers[slot].clickPlaying &&
        profile.xplodium == ore && profile.coins == coins;
}

int CheckOnlineRefinery(CResTOCManager &toc, ZPackTables &tables, ZMenuSurface &view,
    const CRefinementManager::Template &data) {
    struct RestoreSettings {
        ZMenuSurface &view;
        bool connected = GameHostSettings().isConnected;
        bool animated;
        ~RestoreSettings() { GameHostSettings().isConnected = connected; view.animateNavigation = animated; }
    } restore{view, GameHostSettings().isConnected, view.animateNavigation};
    GameHostSettings().isConnected = true;
    view.animateNavigation = false;
    CProfileManager profile;
    if (!CreateTransientProfile(toc, tables, profile)) { return 1; }
    profile.refinery.Bind(data);
    profile.coins = 0;
    profile.warbucks = 0;
    profile.xplodium = 1000;
    unsigned slot = 0;
    while (slot < data.minutes.size() && data.rarePrice[slot] == 0) { ++slot; }
    if (slot >= data.minutes.size()) { return 1; }
    const auto path = std::filesystem::path(TestOutput::Path("refinery-online"));
    std::filesystem::create_directories(path);
    const auto now = CurrentSeconds();
    const auto main = view.movies.Ordinal("GLU_MOVIE_EXPLODIUM");
    const auto button = view.movies.Ordinal("GLU_MOVIE_BUTTON_SMALL");
    unsigned start = 0, end = 0;
    ZMovieRegion graphic, meter, touch;
    if (!view.movies.GetMovie(button)->GetChapterRange(2, start, end) || !view.movies.Region(button, 1, end, graphic)) { return 1; }
    CMenuSystem state;
    state.stack.page = 3;
    view.Begin();
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state)) { return 1; }
    state.refinery.refineryTab = 0;
    if (!view.movies.Region(main, slot, state.refinery.refineryTime, meter)) { return 1; }
    const float x = meter.x + meter.width / 2, y = meter.y + meter.height / 2;
    for (const auto &region : view.movies.Regions(button, end, x - graphic.width / 2, y, true)) {
        if (region.index == 0) { touch = region; }
    }
    view.Begin();
    view.InjectTap({touch.x + touch.width / 2, touch.y + touch.height / 2});
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state) || !state.storePromptRequested ||
        profile.refinery.slots[slot].state || profile.warbucks) {
        std::printf("[refinery-online] insufficient prompt failed slot=%u\n", slot); return 1;
    }
    std::string body = view.movies.NamedString("IDS_RESMAN_MOMONEY_BODY");
    if (!StoreFailureText(view, state, body) || state.failedMissing != data.rarePrice[slot]) { return 1; }
    if (!DrawStorePrompt(view, state)) { return 1; }
    view.clock += 1000;
    view.Begin();
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state) || !DrawStorePrompt(view, state) ||
        !Capture::SaveFrame(view.window, (path / "insufficient.png").string())) { return 1; }
    state = CMenuSystem{};
    state.stack.page = 3;
    profile.warbucks = data.rarePrice[slot] * 2;
    view.Begin();
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state)) { return 1; }
    state.refinery.refineryTab = 0;
    view.Begin();
    view.InjectTap({touch.x + touch.width / 2, touch.y + touch.height / 2});
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state) || profile.refinery.slots[slot].state != 1 ||
        profile.warbucks != data.rarePrice[slot] || !ReloadProfile(profile, path) ||
        profile.refinery.slots[slot].state != 1) { return 1; }
    if (profile.refinery.UnlockSlot(slot, profile.coins, profile.warbucks) || profile.warbucks != data.rarePrice[slot]) { return 1; }
    view.Begin();
    view.InjectTap({x, y});
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state) || !state.refinery.chambers[slot].opening ||
        state.refinery.refineryTransfer >= 0 ||
        !Capture::SaveFrame(view.window, (path / "opening-start.png").string())) { return 1; }
    // The native player advances at most one step per update, even for a slow frame.
    unsigned openingFrames = 0;
    while (state.refinery.chambers[slot].opening && openingFrames < 1000) {
        view.clock += 16;
        view.Begin();
        if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state)) { return 1; }
        ++openingFrames;
        if (openingFrames == 12 && !Capture::SaveFrame(view.window, (path / "opening-middle.png").string())) { return 1; }
    }
    if (openingFrames == 0 || openingFrames == 1000 ||
        !Capture::SaveFrame(view.window, (path / "opening-finished.png").string())) { return 1; }
    profile.xplodium = 0;
    view.Begin();
    view.InjectTap({x, y});
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state) || !state.refinery.chambers[slot].clickPlaying ||
        state.refinery.refineryTransfer >= 0) {
        std::printf("[refinery-online] empty active chamber did not animate\n"); return 1;
    }
    view.clock += 100;
    view.Begin();
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state) ||
        !Capture::SaveFrame(view.window, (path / "empty-click.png").string()) ||
        !FinishRefineryClick(view, state, profile, data, path, now, slot) || state.refinery.refineryTransfer >= 0) { return 1; }
    profile.xplodium = 1000;
    view.Begin();
    view.InjectTap({x, y});
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state) ||
        !FinishRefineryClick(view, state, profile, data, path, now, slot) || state.refinery.refineryTransfer != static_cast<int>(slot)) { return 1; }
    view.clock += 375;
    view.Begin();
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state) || profile.refinery.slots[slot].state != 2 || profile.xplodium) { return 1; }
    const auto finish = profile.refinery.slots[slot].finishTime;
    const auto middle = now + (finish - now) / 2;
    profile.refinery.UpdateRefinement(middle);
    view.clock += 1000;
    view.Begin();
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, middle), state) || !profile.SaveToDisk(path) ||
        !Capture::SaveFrame(view.window, (path / "refining.png").string()) || !ReloadProfile(profile, path) ||
        profile.refinery.slots[slot].state != 2 || profile.refinery.slots[slot].finishTime != finish) { return 1; }
    const auto *fill = view.movies.GetMovie(view.movies.Ordinal("GLU_MOVIE_BUCKET_FILL"));
    if (!fill || state.refinery.refineryFillTime[slot] != fill->duration / 2) { return 1; }
    // Compare clicked and untouched meters at the same UI and refinery clocks.
    // A refining chamber must pulse visually without collecting or restarting work.
    CMenuSystem untouched = state;
    view.Begin();
    view.InjectTap({x, y});
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, middle), state)) { return 1; }
    view.clock += 100;
    view.Begin();
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, middle), state) ||
        !Capture::SaveFrame(view.window, (path / "refining-click.png").string())) { return 1; }
    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    std::vector<unsigned char> clickedPixels(viewport[2] * viewport[3] * 4);
    auto untouchedPixels = clickedPixels;
    glReadPixels(0, 0, viewport[2], viewport[3], GL_RGBA, GL_UNSIGNED_BYTE, clickedPixels.data());
    view.Begin();
    if (!FinishMenuFrame(untouched.refinery.Draw(view, untouched, profile, data, path, middle), untouched) ||
        !Capture::SaveFrame(view.window, (path / "refining-untouched.png").string())) { return 1; }
    glReadPixels(0, 0, viewport[2], viewport[3], GL_RGBA, GL_UNSIGNED_BYTE, untouchedPixels.data());
    if (clickedPixels == untouchedPixels || state.refinery.refineryTransfer >= 0 ||
        profile.refinery.slots[slot].state != 2 || profile.refinery.slots[slot].finishTime != finish || profile.coins) {
        std::printf("[refinery-online] refining click pulse missing or changed resources\n"); return 1;
    }
    const auto *meterEntry = CMenuDataProvider::Find("MDS_BUTTON_XPLODIUM_METER", slot);
    const auto *meterButton = view.movies.GetMovie(view.movies.Ordinal(meterEntry->movies[0]));
    unsigned pulseStart = 0, pulseEnd = 0;
    if (!meterButton || !meterButton->GetChapterRange(1, pulseStart, pulseEnd) ||
        !state.refinery.chambers[slot].clickPlaying || state.refinery.chambers[slot].clickTime != pulseStart + 200) { return 1; }
    // Busy clicks do not restart the pulse. It returns to idle at the BIG boundary.
    view.Begin();
    view.InjectTap({x, y});
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, middle), state) || state.refinery.chambers[slot].clickTime != pulseStart + 200) { return 1; }
    view.clock += (pulseEnd - pulseStart + 1) / 2;
    view.Begin();
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, middle), state) || state.refinery.chambers[slot].clickPlaying) { return 1; }
    view.InjectTap({x, y});
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, middle), state) || !state.refinery.chambers[slot].clickPlaying) { return 1; }
    GameHostSettings().isConnected = false;
    view.Begin();
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, middle), state) || state.refinery.refineryStatusChapter[slot] != 0) { return 1; }
    if (state.refinery.chambers[slot].clickPlaying) { return 1; }
    GameHostSettings().isConnected = true;
    profile.refinery.UpdateRefinement(finish - 1);
    if (profile.refinery.slots[slot].state != 2 || profile.refinery.CollectResources(slot, profile.coins)) { return 1; }
    profile.refinery.UpdateRefinement(finish);
    view.clock += fill->duration;
    view.Begin();
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, finish), state) || state.refinery.refineryStatusChapter[slot] != 3) { return 1; }
    const auto yield = profile.refinery.GetRefinementSlotYield(slot);
    view.Begin();
    view.InjectTap({x, y});
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, finish), state) ||
        !FinishRefineryClick(view, state, profile, data, path, finish, slot) || state.refinery.refineryTransfer != static_cast<int>(slot)) { return 1; }
    view.clock += 375;
    view.Begin();
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, finish), state) || profile.coins != yield ||
        !ReloadProfile(profile, path) || profile.refinery.slots[slot].state != 1 || profile.coins != yield ||
        profile.refinery.CollectResources(slot, profile.coins)) { return 1; }
    // Cheat unlock includes standard timed chambers; both instant first slots stay open.
    if (!GameCheats::ApplyRefineryCheat(GameCheats::ToggleRefineryLocks, profile, now)) { return 1; }
    view.Begin();
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state)) { return 1; }
    for (unsigned index = 0; index < data.minutes.size(); ++index) {
        if (data.minutes[index] != 0 && index != slot && !state.refinery.chambers[index].opening) { return 1; }
    }
    if (!GameCheats::ApplyRefineryCheat(GameCheats::ToggleRefineryLocks, profile, now)) { return 1; }
    view.Begin();
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state)) { return 1; }
    for (unsigned index = 0; index < data.minutes.size(); ++index) {
        if (data.minutes[index] != 0 &&
            (state.refinery.chambers[index].opening || state.refinery.chambers[index].eye.GetStep() || profile.refinery.slots[index].state)) { return 1; }
        if (data.minutes[index] == 0 && (profile.refinery.slots[index].state != 1 || state.refinery.chambers[index].opening)) { return 1; }
    }
    // Every unlocked chamber, including both instant slots, animates with no ore.
    if (!GameCheats::ApplyRefineryCheat(GameCheats::ToggleRefineryLocks, profile, now)) { return 1; }
    state = CMenuSystem{};
    state.stack.page = 3;
    profile.xplodium = 0;
    view.Begin();
    if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state)) { return 1; }
    const unsigned count = static_cast<unsigned>(data.minutes.size() / 2);
    for (unsigned index = 0; index < data.minutes.size(); ++index) {
        state.refinery.refineryTab = index / count;
        ZMovieRegion area;
        if (!view.movies.Region(main, index % count, state.refinery.refineryTime, area)) { return 1; }
        view.Begin();
        view.InjectTap({area.x + area.width / 2, area.y + area.height / 2});
        if (!FinishMenuFrame(state.refinery.Draw(view, state, profile, data, path, now), state) ||
            !FinishRefineryClick(view, state, profile, data, path, now, index) ||
            state.refinery.refineryTransfer >= 0 || profile.refinery.slots[index].state != 1) { return 1; }
    }
    std::printf("[refinery-online] all-active-empty=%zu ready=1 refining=1 collect=1 animation-before-transfer=1\n", data.minutes.size());
    // The standard chain opens when its prerequisite is USED, not purchased.
    profile.refinery.Bind(data);
    for (unsigned index = 0; index < data.gate.size(); ++index) {
        if (!profile.refinery.IsGated(index)) { continue; }
        const unsigned previous = data.gate[index];
        profile.xplodium = 100;
        if (profile.refinery.slots[index].state != 0 ||
            !profile.refinery.BeginRefinement(previous, previous, 100, profile.xplodium, now) ||
            profile.refinery.slots[index].state != 1) { return 1; }
    }
    std::printf("[refinery-online] eye-opening frames=%u cheat-unlock=1 relock=1\n", openingFrames);
    std::printf("[refinery-online] refining-pulse=1 repeat=1 offline-cancel=1 standard-lock=1 instant-preserved=1\n");
    std::printf("[refinery-online] slot=%u cost=%u minutes=%u yield=%u unlock=1 timer=1 reconnect=1 reload=1 collect-once=1 standard-chain=1\n",
        slot, data.rarePrice[slot], data.minutes[slot], data.efficiencyPercent[slot]);
    return 0;
}
