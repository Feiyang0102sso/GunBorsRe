#include "gun_bros_re/debug/Capture.h"
/** @file CInputPad.cpp
 * @brief CInputPad::Base::Bind (:88320) binds meters to regions 0/1 and guns to 2/3.
 */
#define NOMINMAX
#include "TestOutput.h"
#include "gun_bros_re/ui/CInputPad.h"
#include "gun_bros_re/ui/ZMenuData.h"
#include "gun_bros_re/ui/ZTextLayout.h"
#include "gun_bros_re/ZHostSettings.h"
#include "gun_bros_re/data/ZPowerupCatalog.h"
#include "engine/platform/ZWindow.h"
#include "engine/resources/CResTOCManager.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "engine/graphics/ZPNG.h"
#include <algorithm>
#include <cstdio>
#include <sstream>
#include "Checks.h"

/** Original pack12 LEVEL string references exercise the radio popup end to end. */
int RunOriginalDialogCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    ZWindow window;
    if (!window.Open("Gun Bros", 1024, 768)) { return 1; }
    CInputPad hud;
    if (!hud.Init(toc, tables)) { return 1; }
    const int packIndex = toc.GetPackIndexFromName("pack12");
    if (packIndex < 0) { return 1; }
    const unsigned hash = toc.GetPack(packIndex)->GetPackHash();
    unsigned tested = 0, failures = 0;
    const unsigned count = tables.GetObjectPack(packIndex).GetObjectCount(ZGameSection::Level);
    for (unsigned ordinal = 0; ordinal < count; ++ordinal) {
        std::vector<std::uint8_t> bytes;
        if (!tables.ReadSectionResource(hash, ZGameSection::Level, ordinal, bytes)) { return 1; }
        CArrayInputStream input(bytes);
        CLevel::Template data;
        if (!data.Init(input) || input.Available() != 0) { return 1; }
        for (const auto &resource : data.script.GetResources()) {
            if (resource.sectionOrType != 254) { continue; }
            CGameAssetRef ref;
            ref.packHash = resource.packHash;
            ref.assetId = resource.resourceId;
            const auto text = ReadGameString(toc, ref);
            if (text.empty()) {
                // LEVEL resource 6 is not consumed by its ShowDialog call.
                // Keep the unresolved reference visible; do not create text.
                std::printf("[dialog-check] unreadable string=%08x:%u length=%zu\n", ref.packHash, ref.assetId, text.size());
                continue;
            }
            if (!hud.ShowDialog(text, true, 0)) { ++failures; continue; }
            ZInputPadState state;
            state.dialog = text;
            if (hud.Pointer(state, 800, 600, true) == ZInputPadAction::Continue ||
                hud.CapturesPointer(state, 800, 600)) { std::printf("[dialog-check] invented input capture\n"); ++failures; }
            unsigned elapsed = 0;
            while (!hud.IsDialogDone() && elapsed < 60000) {
                hud.UpdateDialog(16);
                elapsed += 16;
                if (elapsed == 3008) {
                    glViewport(0, 0, 1024, 768);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    if (!hud.m_dialog.Draw() || !Capture::SaveFrame(window, TestOutput::Path("original-dialog-") + std::to_string(tested) + ".png")) { ++failures; }
                }
            }
            if (!hud.IsDialogDone() || elapsed <= 1000) { std::printf("[dialog-check] auto close failed\n"); ++failures; }
            // Script-owned dialogs remain until native70 requests the outro.
            if (!hud.ShowDialog(text, false, 0)) { ++failures; }
            for (unsigned tick = 0; tick < 4000; ++tick) { hud.UpdateDialog(16); }
            if (hud.IsDialogDone()) { std::printf("[dialog-check] manual closed early\n"); ++failures; }
            hud.ClearDialog(false);
            if (hud.IsDialogDone()) { ++failures; }
            for (unsigned tick = 0; tick < 1000; ++tick) { hud.UpdateDialog(16); }
            if (!hud.IsDialogDone()) { std::printf("[dialog-check] outro not completed\n"); ++failures; }
            std::printf("[dialog-check] LEVEL=%u string=%08x:%u pages=%u elapsed=%u text=%s\n",
                ordinal, ref.packHash, ref.assetId, hud.m_dialog.PageCount(), elapsed, text.c_str());
            ++tested;
        }
    }
    if (tested == 0) { ++failures; }
    std::printf("[dialog-check] original-strings=%u failures=%u\n", tested, failures);
    return failures != 0;
}

/** Retained HUD milestone now checks original controls, selector and pause tree. */
int RunSurvivalHudCheck(const std::string &bigDirectory) {
    if (RunOriginalHudCheck(bigDirectory) != 0 || RunOriginalPowerupSelectorCheck(bigDirectory) != 0 ||
        RunOriginalPauseCheck(bigDirectory) != 0) { return 1; }
    return 0;
}

int RunOriginalPauseCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    ZWindow window;
    if (!window.Open("Gun Bros - Original Pause Check", 1600, 1200)) { return 1; }
    CInputPad hud;
    ZPackTables tables(toc);
    if (!hud.Init(toc, tables)) { return 1; }
    CProfileManager profile;
    const auto directory = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) / ("pause-profile-" + std::to_string(window.GetTicksMs()));
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!LoadProfile(toc, tables, profile, directory, TestOutput::Fixtures())) { return 1; }
    CPlayerProgress progress;
    progress.Bind(profile.nativeArchive->progression);
    progress.SetExperience(profile.experience);
    ZInputPadState state;
    // Exercise the standalone-map caller too: pause UI must not depend on
    // whether a native archive was attached to the combat context.
    state.paused = true;
    state.health = 1;
    state.maximumHealth = 1;
    state.coins = profile.coins;
    state.warbucks = profile.warbucks;
    state.level = progress.GetLevel();
    state.experience = progress.GetExperienceInLevel();
    state.experienceDelta = progress.GetExperienceDelta();
    state.soundEnabled = profile.soundEnabled;
    state.musicEnabled = profile.musicEnabled;
    state.dockedSticks = profile.options.DockedSticks();
    unsigned failures = 0;
    if (!hud.Draw(state) || !hud.m_pauseHits.empty()) { ++failures; }
    hud.AdvanceMenu(3000);
    if (!hud.Draw(state) || hud.m_pauseItems.size() != 6) { ++failures; }
    unsigned hits = 0;
    for (unsigned index = 0; index < hud.m_pauseItems.size(); ++index) {
        hud.m_pauseTarget = std::clamp(float(index) - 1, 0.0f, float(hud.m_pauseItems.size()) - 3);
        hud.AdvanceMenu(3000);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.Draw(state)) { ++failures; }
        bool found = false;
        for (const auto &hit : hud.m_pauseHits) {
            if (hit.second != index) { continue; }
            const float x = hit.first.x + hit.first.width / 2, y = hit.first.y + hit.first.height / 2;
            hud.Pointer(state, x, y, false);
            const auto action = hud.Pointer(state, x, y, true);
            ++hits;
            found = true;
            const unsigned originalAction = FindMenuData("MDS_PAUSE_ROOT", hud.m_pauseItems[index])->action;
            if (originalAction == 31 && action != ZInputPadAction::Resume) { ++failures; }
            if (originalAction == 40 && action != ZInputPadAction::Exit) { ++failures; }
            if (originalAction == 9) {
                if (action != ZInputPadAction::Sound) { ++failures; }
                profile.soundEnabled = !profile.soundEnabled;
                state.soundEnabled = profile.soundEnabled;
            }
            if (originalAction == 10) {
                if (action != ZInputPadAction::Music) { ++failures; }
                profile.musicEnabled = !profile.musicEnabled;
                state.musicEnabled = profile.musicEnabled;
            }
            if (originalAction == 16) {
                if (action != ZInputPadAction::DockedSticks) { ++failures; }
                profile.options.ToggleDockedSticks();
                state.dockedSticks = profile.options.DockedSticks();
            }
            break;
        }
        if (!found) { ++failures; }
        if (!hud.Draw(state)) { ++failures; }
        if (index == 0 && !Capture::SaveFrame(window, TestOutput::Path("ui-original-2026-09-09/pause-original-ready.png"))) { ++failures; }
        if (hud.m_pauseHelp) { break; }
    }
    if (!hud.m_pauseHelp) { ++failures; }
    hud.AdvanceMenu(3000);
    if (!hud.Draw(state) || hud.m_pauseItems.size() != 14) { ++failures; }
    for (unsigned index = 0; index < hud.m_pauseItems.size(); ++index) {
        hud.m_pauseFocus = index;
        hud.m_pauseBodyTime = 0;
        hud.m_pauseTarget = std::clamp(float(index) - 1, 0.0f, float(hud.m_pauseItems.size()) - 3);
        hud.AdvanceMenu(3000);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.Draw(state) || hud.PauseText(state, index, 0).empty() || hud.PauseText(state, index, 1).empty()) { ++failures; }
        if (index == 0 && !Capture::SaveFrame(window, TestOutput::Path("ui-original-2026-09-09/pause-original-help.png"))) { ++failures; }
    }
    bool returned = false;
    const auto backHits = hud.m_pauseHits;
    for (const auto &hit : backHits) {
        if (hit.second != UINT32_MAX) { continue; }
        const float x = hit.first.x + hit.first.width / 2, y = hit.first.y + hit.first.height / 2;
        hud.Pointer(state, x, y, false);
        hud.Pointer(state, x, y, true);
        returned = !hud.m_pauseHelp;
        break;
    }
    if (!returned || !profile.SaveToDisk(directory)) { ++failures; }
    CProfileManager reloaded;
    if (!LoadProfile(toc, tables, reloaded, directory) || reloaded.soundEnabled != profile.soundEnabled ||
        reloaded.musicEnabled != profile.musicEnabled || reloaded.options.DockedSticks() != profile.options.DockedSticks() ||
        reloaded.coins != state.coins || reloaded.warbucks != state.warbucks) { ++failures; }
    std::printf("[pause-check] native-hits=%u help-items=14 back=%d preference-reload=3 failures=%u\n", hits, returned, failures);
    return failures != 0;
}

int RunOriginalHudCheck(const std::string &bigDirectory) {
    unsigned failures = 0;
    // SetValue must preserve the original linear retarget, cosine draw and rate.
    CInputPadMeter meter;
    meter.SnapValue(1);
    meter.SetValue(0);
    meter.Update(10);
    if (std::abs(meter.GetDrawValue() - 0.5f) > 0.0001f || meter.GetHighlight() != 100) { ++failures; }
    meter.SetValue(0.75f);
    if (std::abs(meter.GetDrawValue() - 0.5f) > 0.0001f) { ++failures; }
    meter.Update(80);
    if (meter.GetDrawValue() != 0.75f || meter.GetHighlight() != 0) { ++failures; }
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    ZPackTables tables(toc);
    ZWindow window;
    if (!window.Open("Gun Bros - Original HUD Check", 1600, 1200)) { return 1; }
    CInputPad hud;
    if (!hud.Init(toc, tables)) { return 1; }
    ZInputPadState state;
    state.health = state.maximumHealth = 1;
    unsigned hits = 0;
    const auto buttons = hud.ControlButtons(state);
    if (buttons.size() != 5) { ++failures; }
    for (const auto &button : buttons) {
        const float x = button.rect.x + button.rect.width / 2, y = button.rect.y + button.rect.height / 2;
        hud.Pointer(state, x, y, false);
        if (hud.Pointer(state, x, y, true) != button.action) { ++failures; }
        else { ++hits; }
    }
    // Exercise the actual input/draw path: both authored pressed sprites must
    // differ on screen, stay pressed while active, then restore exactly.
    unsigned pressedChecks = 0;
    int width = 0, height = 0;
    window.GetDrawableSize(width, height);
    std::vector<unsigned char> idlePixels(width * height * 4);
    std::vector<unsigned char> pressedPixels(idlePixels.size());
    std::vector<unsigned char> currentPixels(idlePixels.size());
    for (const auto &button : buttons) {
        if (button.action != ZInputPadAction::OpenShop && button.action != ZInputPadAction::SwapWeapon) { continue; }
        const float x = button.rect.x + button.rect.width / 2;
        const float y = button.rect.y + button.rect.height / 2;
        hud.Pointer(state, x, y, false);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.DrawControls(state)) { ++failures; }
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, idlePixels.data());
        if (!Capture::SaveFrame(window, TestOutput::Path("ui-original-2026-09-09/hud-button-idle-") + std::to_string(pressedChecks) + ".png")) { ++failures; }
        if (hud.Pointer(state, x, y, true) != button.action) { ++failures; }
        if (button.action == ZInputPadAction::OpenShop) { state.shopOpen = true; }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.DrawControls(state)) { ++failures; }
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pressedPixels.data());
        const bool changed = idlePixels != pressedPixels;
        if (!changed) { ++failures; }
        if (!Capture::SaveFrame(window, TestOutput::Path("ui-original-2026-09-09/hud-button-pressed-") + std::to_string(pressedChecks) + ".png")) { ++failures; }
        if (hud.Pointer(state, x, y, true) != ZInputPadAction::None) { ++failures; }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.DrawControls(state)) { ++failures; }
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, currentPixels.data());
        if (currentPixels != pressedPixels) { ++failures; }
        if (button.action == ZInputPadAction::SwapWeapon) {
            // Keyboard state must select the exact same authored pressed art.
            hud.Pointer(state, -1, -1, false);
            state.swapKeyDown = true;
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!hud.DrawControls(state)) { ++failures; }
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, currentPixels.data());
            if (currentPixels != pressedPixels) { ++failures; }
            state.swapKeyDown = false;
            hud.Pointer(state, x, y, true);
            // Moving off restores the artwork; moving back does not fire a
            // second switch. A paused menu must not leave the HUD held down.
            if (hud.Pointer(state, -1, -1, true) != ZInputPadAction::None) { ++failures; }
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!hud.DrawControls(state)) { ++failures; }
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, currentPixels.data());
            if (currentPixels != idlePixels) { ++failures; }
            if (hud.Pointer(state, x, y, true) != ZInputPadAction::None) { ++failures; }
            state.paused = true;
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!hud.DrawControls(state)) { ++failures; }
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, currentPixels.data());
            if (currentPixels != idlePixels) { ++failures; }
            state.paused = false;
        }
        hud.Pointer(state, x, y, false);
        if (state.shopOpen) {
            // The selector remains active after mouse release, matching
            // CInputPad::Base state 7 rather than a transient hover effect.
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!hud.DrawControls(state)) { ++failures; }
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, currentPixels.data());
            if (currentPixels != pressedPixels) { ++failures; }
        }
        state.shopOpen = false;
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.DrawControls(state)) { ++failures; }
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, currentPixels.data());
        if (currentPixels != idlePixels) { ++failures; }
        std::printf("[hud-button-check] action=%u pressed-pixels-changed=%d restored=%d\n",
            unsigned(button.action), changed, currentPixels == idlePixels);
        ++pressedChecks;
    }
    // Perturb the in-memory parsed Movie: hit geometry must follow its bytes.
    const unsigned base = hud.m_resources.m_movies.Ordinal("GLU_MOVIE_HUD_PAD_IPAD");
    CMovie *movie = hud.m_resources.m_movies.GetMovie(base);
    unsigned regionIndex = 0;
    bool mutated = false;
    for (auto &object : movie->objects) {
        if (object.type != 6) { continue; }
        if (regionIndex++ != 2) { continue; }
        const auto original = object;
        for (auto &frame : object.frames) { frame.x += 23; }
        const auto changed = hud.ControlButtons(state);
        for (const auto &button : changed) {
            if (button.action != ZInputPadAction::OpenShop) { continue; }
            for (const auto &before : buttons) {
                if (before.action == button.action && std::abs(button.rect.x - before.rect.x - 23) < 0.001f) { mutated = true; }
            }
        }
        object = original;
        break;
    }
    if (!mutated) { ++failures; }
    unsigned icons = 0;
    // Stationary controls must not replay the press/fade chapter forever.
    // Compare the real rendered buttons at several idle times.
    unsigned idleChanges = 0;
    state.leftPowerup = hud.m_resources.m_powerups[5].resource;
    state.rightPowerup = hud.m_resources.m_powerups[13].resource;
    state.leftCount = 84;
    state.rightCount = 140;
    for (unsigned time = 1000; time <= 2700; time += 37) {
        hud.m_controlTime = time;
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.DrawControls(state)) { ++failures; }
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, currentPixels.data());
        if (time == 1000) { idlePixels = currentPixels; }
        else if (currentPixels != idlePixels) { ++idleChanges; }
    }
    if (idleChanges != 0) { ++failures; }
    std::printf("[powerup-idle-check] changed-frames=%u expected=0\n", idleChanges);
    hud.m_controlTime = 0;
    for (const auto &powerup : hud.m_resources.m_powerups) {
        state.leftPowerup = state.rightPowerup = powerup.resource;
        state.leftCount = 9;
        state.rightCount = 10;
        state.moveX = 1;
        state.aimY = -1;
        if (!hud.DrawControls(state)) { ++failures; }
        else { ++icons; }
    }
    for (unsigned mode = 0; mode < 3; ++mode) {
        state.horde = mode == 1;
        state.dockedSticks = mode != 2;
        state.kills = 196;
        state.score = 21035;
        state.killStreak = 101;
        state.moveX = 0;
        state.aimY = 0;
        glClearColor(0.04f, 0.07f, 0.09f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.DrawControls(state) || !Capture::SaveFrame(window, TestOutput::Path("ui-original-2026-09-09/hud-original-") + std::to_string(mode) + ".png")) { ++failures; }
    }
    // Original notifications: source strings, two level-up Movies, independent
    // perfect title/body and completion on the last actual resource duration.
    hud.ResetNotices();
    state.level = 12;
    hud.ObserveProgress(state);
    ++state.level;
    hud.ObserveProgress(state);
    if (hud.NoticeCount() != 2 || hud.HasInterstitial()) { ++failures; }
    unsigned noticeChecks = 0;
    for (unsigned index = 0; index < 2; ++index) {
        if (hud.m_notices.empty()) { ++failures; break; }
        const auto &notice = hud.m_notices.front();
        const unsigned duration = hud.m_resources.m_movies.GetMovie(notice.movie)->duration;
        if (notice.title.empty() || notice.title.find("%d") != std::string::npos || notice.title.find("%i") != std::string::npos) { ++failures; }
        std::printf("[original-overlay-check] level step=%u movie=%u duration=%u text=%s\n", index, notice.movie, duration, notice.title.c_str());
        hud.Advance(duration / 2);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        hud.DrawNotice();
        if (!Capture::SaveFrame(window, TestOutput::Path("ui-original-2026-09-09/hud-level-original-") + std::to_string(index) + ".png")) { ++failures; }
        hud.Advance(duration - duration / 2);
        ++noticeChecks;
    }
    for (unsigned mode = 0; mode < 4; ++mode) {
        if (mode == 0) { hud.BeginLevel(23, false, false); }
        if (mode == 1) { hud.BeginLevel(2, true, false); }
        if (mode == 2) { hud.BeginLevel(1, false, true); }
        if (mode == 3) { hud.OnWaveClear(23, true, 10, false); }
        if (!hud.HasInterstitial() || hud.TakeInterstitialCompletion()) { ++failures; }
        unsigned step = 0;
        while (!hud.m_notices.empty()) {
            const auto &notice = hud.m_notices.front();
            const unsigned duration = hud.m_resources.m_movies.GetMovie(notice.movie)->duration;
            const bool final = notice.releaseLevel;
            if (notice.title.empty() || notice.title.find("%d") != std::string::npos || notice.footer.find("%d") != std::string::npos ||
                notice.title.find("%i") != std::string::npos || notice.footer.find("%i") != std::string::npos) { ++failures; }
            std::printf("[original-overlay-check] mode=%u movie=%u duration=%u title=%s footer=%s\n", mode, notice.movie, duration, notice.title.c_str(), notice.footer.c_str());
            hud.Advance(duration / 2);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            hud.DrawNotice();
            if (!Capture::SaveFrame(window, TestOutput::Path("ui-original-2026-09-09/hud-notice-original-") + std::to_string(mode) + "-" + std::to_string(step++) + ".png")) { ++failures; }
            hud.Advance(duration - duration / 2 - 1);
            if (hud.TakeInterstitialCompletion()) { ++failures; }
            hud.Advance(1);
            if (hud.TakeInterstitialCompletion() != final || hud.TakeInterstitialCompletion()) { ++failures; }
            ++noticeChecks;
        }
    }
    hud.OnWaveClear(1, false, 10, true);
    if (hud.NoticeCount() != 1 || !hud.m_notices.front().title.empty()) { ++failures; }
    if (hud.m_resources.m_movies.Failures() != 0) { ++failures; }
    std::printf("[original-overlay-check] timelines=%u failures=%u\n", noticeChecks, failures);
    // Sidebar visibility is independent of the FPS overlay and never takes input.
    hud.ResetNotices();
    const ZHostSettings originalSettings = GameHostSettings();
    GameHostSettings().debugMode = true;
    state.debugMap = "PACK2 / MAP 7";
    state.weapon = "WHIPPERSNAPPERS";
    state.showCollisions = true;
    state.buffs = "SHIELD 20S ATTACK 10S DEFENSE 10S SPEED 10S AUTO AIM 10S TANTRUM 10S";
    std::vector<unsigned char> sidebarPixels(idlePixels.size());
    std::vector<unsigned char> hiddenPixels(idlePixels.size());
    for (unsigned visible = 0; visible < 2; ++visible) {
        GameHostSettings().drawDebugInfo = visible != 0;
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.Draw(state)) { ++failures; }
        if (visible != 0) {
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, sidebarPixels.data());
        } else { glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, hiddenPixels.data()); }
        if (!Capture::SaveFrame(window, TestOutput::Path("debug-sidebar-") + std::to_string(visible) + ".png")) { ++failures; }
    }
    if (sidebarPixels == hiddenPixels) { ++failures; }
    GameHostSettings().debugMode = false;
    GameHostSettings().drawDebugInfo = true;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!hud.Draw(state)) { ++failures; }
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, sidebarPixels.data());
    if (sidebarPixels != hiddenPixels) { ++failures; }
    GameHostSettings() = originalSettings;
    const unsigned errors = glGetError();
    if (errors != 0) { ++failures; }
    std::printf("[original-hud-check] hits=%u alternate-icons=%u region-mutation=%d meter-interpolation=3 gl=%u failures=%u\n", hits, icons, mutated, errors, failures);
    return failures != 0;
}
