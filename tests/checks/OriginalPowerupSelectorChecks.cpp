/** @file OriginalPowerupSelector.cpp
 * @brief CPowerUpSelector single-player adapter; original source :184203-187670.
 * ui_movie.bt / powerup_template.bt / store_entry.bt describe the BIG inputs.
 */
#define NOMINMAX
#include "TestOutput.h"
#include "gun_bros_re/ui/SurvivalHud.h"
#include "gun_bros_re/ui/OriginalMenuData.h"
#include "gun_bros_re/ui/OriginalTextLayout.h"
#include "engine/platform/CWindow.h"
#include "engine/resources/CResTOCManager.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "Checks.h"

int RunOriginalPowerupSelectorCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CWindow window;
    if (!window.Open("Gun Bros - Original Powerup Selector", 1600, 1200)) { return 1; }
    SurvivalHud hud;
    if (!hud.Init(toc, tables)) { return 1; }
    for (const char *name : {"IDS_SHOP_MOMONEY_BODY", "IDS_SHOP_MOMONEY_IAP_BODY", "IDS_SHOP_UNAVAILABLE_BODY", "IDS_SHOP_MOMONEY_TITLE"}) {
        std::printf("[selector-check] %s=%s\n", name, hud.m_movies.NamedString(name).c_str());
    }
    CProfileManager profile;
    const auto directory = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) / ("selector-profile-" + std::to_string(window.GetTicksMs()));
    if (!LoadNativeProfile(toc, tables, profile, directory, TestOutput::Fixtures())) { return 1; }
    CPlayerProgress progress;
    progress.Bind(profile.nativeArchive->progression);
    progress.SetExperience(profile.experience);
    SurvivalHudState state;
    state.originalUi = true; state.shopOpen = true; state.paused = true;
    state.coins = profile.coins; state.warbucks = profile.warbucks;
    state.inventory = profile.powerups;
    unsigned failures = 0, purchases = 0, iconHits = 0, choices = 0;
    // Compare the real selector, including owned stock, with original STORE bits.
    for (unsigned mode = 0; mode < 3; ++mode) {
        for (bool afterDeath : {false, true}) {
            state.localLive = mode == 1;
            state.deathmatch = mode == 2;
            state.afterDeathShop = afterDeath;
            hud.ResetSelector();
            if (!hud.DrawOriginalSelector(state)) { ++failures; }
            unsigned expectedCount = 0;
            for (unsigned storeIndex : hud.m_selectorAllEntries) {
                const auto &store = hud.m_store[storeIndex].data;
                const auto &reference = store.objects.front().object;
                for (const auto &powerup : hud.m_powerups) {
                    if (powerup.resource.packHash != reference.packHash || powerup.resource.localIndex != reference.localIndex) { continue; }
                    const bool expected = (store.excludedGameModes & (1u << mode)) == 0 && (powerup.data.field112 != 0) == afterDeath;
                    const bool listed = std::find(hud.m_selectorEntries.begin(), hud.m_selectorEntries.end(), storeIndex) != hud.m_selectorEntries.end();
                    if (expected) { ++expectedCount; }
                    if (listed != expected) { ++failures; }
                }
            }
            std::printf("[powerup-mode-selector] mode=%u after-death=%d entries=%zu expected=%u failures=%u\n",
                mode, afterDeath, hud.m_selectorEntries.size(), expectedCount, failures);
        }
    }
    state.localLive = false;
    state.deathmatch = false;
    state.afterDeathShop = false;
    hud.ResetSelector();
    if (!hud.DrawOriginalSelector(state) || !hud.m_selectorHits.empty()) { ++failures; }
    for (const char *name : {"GLU_MOVIE_POWERUP_MENU_NEW", "GLU_MOVIE_POWER_UP_LAYOUT", "GLU_MOVIE_POWERUP_MENU_NEW_COPY"}) {
        const auto *movie = hud.m_movies.GetMovie(hud.m_movies.Ordinal(name));
        std::printf("[selector-check] %s duration=%u chapters=", name, movie->duration);
        for (unsigned chapter : movie->chapters) { std::printf("%u,", chapter); }
        std::printf("\n");
    }
    hud.AdvanceMenu(2000);
    const float beforeDrag = hud.m_selectorPosition;
    hud.ScrollMenuInput(state, 0, -12, 0);
    hud.AdvanceMenu(2000);
    const float dragged = hud.m_selectorPosition - beforeDrag;
    if (dragged <= 0 || dragged >= 1) { ++failures; }
    std::printf("[selector-drag-check] pixels=12 items=%.3f expected=(0,1)\n", dragged);
    if (!hud.DrawOriginalSelector(state)) { ++failures; }
    // Drag across an actual item, release, and wait: no click or snap-to-end.
    for (const auto &hit : hud.m_selectorHits) {
        if (hit.action != SurvivalHudAction::SelectItem && hit.action != SurvivalHudAction::BuyItem) { continue; }
        const float x = hit.area.x + hit.area.width / 2, y = hit.area.y + hit.area.height / 2;
        hud.Pointer(state, x, y, false);
        if (hud.Pointer(state, x, y, true) != SurvivalHudAction::None) { ++failures; }
        hud.ScrollMenuInput(state, 0, -24, 0);
        if (hud.Pointer(state, x - 24, y, true) != SurvivalHudAction::None ||
            hud.Pointer(state, x - 24, y, false) != SurvivalHudAction::None) { ++failures; }
        const float stopped = hud.m_selectorPosition;
        hud.AdvanceMenu(2000);
        if (std::abs(hud.m_selectorPosition - stopped) > 0.001f) { ++failures; }
        std::printf("[selector-drag-check] release-no-click=1 stopped=%.3f after-wait=%.3f failures=%u\n",
            stopped, hud.m_selectorPosition, failures);
        break;
    }
    for (unsigned index = 0; index < hud.m_selectorEntries.size(); ++index) {
        state.itemChoice = false;
        hud.m_selectorPosition = float(index);
        hud.m_selectorTarget = float(index);
        glClearColor(0.07f, 0.1f, 0.13f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.DrawOriginalSelector(state)) { ++failures; continue; }
        const unsigned storeIndex = hud.m_selectorEntries[index];
        bool bought = false;
        const auto hits = hud.m_selectorHits;
        for (const auto &hit : hits) {
            if (hit.storeIndex != static_cast<int>(storeIndex) || hit.action != SurvivalHudAction::BuyItem) { continue; }
            const float x = hit.area.x + hit.area.width / 2, y = hit.area.y + hit.area.height / 2;
            hud.Pointer(state, x, y, false);
            if (hud.Pointer(state, x, y, true) != SurvivalHudAction::None) { ++failures; }
            const auto action = hud.Pointer(state, x, y, false);
            if (action != SurvivalHudAction::BuyItem || hud.SelectedItem() != &hud.m_store[storeIndex]) { ++failures; break; }
            const auto result = profile.AcquireItem(hud.SelectedItem()->data, progress.GetLevel());
            if (result == PurchaseResult::Unsupported && !IsPlayablePowerup(hud.SelectedItem()->data.objects.front().object)) {
                std::printf("[selector-check] unsupported effect retained index=%u name=%s; no charge\n", index, hud.SelectedItem()->name.c_str());
                bought = true;
                break;
            }
            if (result != PurchaseResult::Purchased) { std::printf("[selector-check] purchase rejected index=%u result=%u\n", index, unsigned(result)); ++failures; break; }
            ++purchases; bought = true; break;
        }
        if (!bought) { ++failures; }
        state.inventory = profile.powerups; state.coins = profile.coins; state.warbucks = profile.warbucks;
        if (!hud.DrawOriginalSelector(state)) { ++failures; continue; }
        const auto ownedHits = hud.m_selectorHits;
        for (const auto &hit : ownedHits) {
            if (hit.storeIndex != static_cast<int>(storeIndex) || hit.action != SurvivalHudAction::SelectItem) { continue; }
            const float x = hit.area.x + hit.area.width / 2, y = hit.area.y + hit.area.height / 2;
            hud.Pointer(state, x, y, false);
            if (hud.Pointer(state, x, y, true) != SurvivalHudAction::None ||
                hud.Pointer(state, x, y, false) != SurvivalHudAction::SelectItem) { ++failures; break; }
            ++iconHits;
            state.itemChoice = true;
            if (!hud.DrawOriginalSelector(state)) { ++failures; break; }
            hud.AdvanceMenu(2000);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            if (!hud.DrawOriginalSelector(state)) { ++failures; break; }
            const auto choiceHits = hud.m_selectorHits;
            for (const auto &choice : choiceHits) {
                if (choice.action != SurvivalHudAction::EquipLeft && choice.action != SurvivalHudAction::EquipRight &&
                    choice.action != SurvivalHudAction::UseNow) { continue; }
                const float choiceX = choice.area.x + choice.area.width / 2, choiceY = choice.area.y + choice.area.height / 2;
                hud.Pointer(state, choiceX, choiceY, false);
                if (hud.Pointer(state, choiceX, choiceY, true) != choice.action) { ++failures; }
                ++choices;
            }
            break;
        }
        if (index == 0 || index == hud.m_selectorEntries.size() / 2) {
            if (!GB_SAVE_FRAME(window, TestOutput::Path("ui-original-2026-09-09/selector-original-") + std::to_string(index) + ".png")) { ++failures; }
        }
        window.Present();
    }
    if (!SaveNativeProfile(profile, directory)) { ++failures; }
    CProfileManager restored;
    if (!LoadNativeProfile(toc, tables, restored, directory) || restored.coins != profile.coins || restored.warbucks != profile.warbucks) { ++failures; }
    for (unsigned index : hud.m_selectorEntries) {
        const auto &reference = hud.m_store[index].data.objects.front().object;
        if (restored.GetPowerupCount(reference) != profile.GetPowerupCount(reference)) { ++failures; }
    }
    state.itemChoice = false;
    state.coins = 0; state.warbucks = 0; // Failure input only; original profile stays untouched.
    for (const auto result : {PurchaseResult::InsufficientCoins, PurchaseResult::InsufficientWarbucks, PurchaseResult::Unsupported}) {
        hud.ReportSelectorPurchase(result, state);
        if (!hud.DrawOriginalSelector(state) || !hud.m_selectorHits.empty() || !hud.m_selectorPromptHits.empty()) { ++failures; }
        hud.AdvanceMenu(2000);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.DrawOriginalSelector(state) || !hud.m_selectorPrompt.IsReady() || hud.m_selectorPromptHits.empty() ||
            hud.m_selectorPromptBody.find('%') != std::string::npos) { ++failures; }
        if (!GB_SAVE_FRAME(window, TestOutput::Path("ui-original-2026-09-09/selector-prompt-") + std::to_string(unsigned(result)) + ".png")) { ++failures; }
        const auto hits = hud.m_selectorPromptHits;
        for (const auto &hit : hits) {
            if (result != PurchaseResult::Unsupported && hit.second != 71) { continue; }
            const float x = hit.first.x + hit.first.width / 2, y = hit.first.y + hit.first.height / 2;
            hud.Pointer(state, x, y, false);
            hud.Pointer(state, x, y, true);
            if (result != PurchaseResult::Unsupported) {
                if (std::strcmp(hud.m_selectorPromptTable, "MDS_STORE_PROMPT_OFFLINE") != 0 || !hud.DrawOriginalSelector(state)) { ++failures; }
                hud.AdvanceMenu(2000);
                if (!hud.DrawOriginalSelector(state) || !hud.m_selectorPrompt.IsReady()) { ++failures; }
                if (!hud.BackFromSelectorPrompt()) { ++failures; }
            }
            break;
        }
        hud.AdvanceMenu(2000); hud.AdvanceMenu(2000);
        if (!hud.DrawOriginalSelector(state) || hud.m_selectorPrompt.IsActive()) { ++failures; }
    }
    hud.m_selectorPosition = 2; hud.m_selectorTarget = 2;
    if (!hud.DrawOriginalSelector(state)) { ++failures; }
    MovieRegion before, after;
    for (const auto &hit : hud.m_selectorHits) {
        if (hit.storeIndex == static_cast<int>(hud.m_selectorEntries[0]) && hit.action == SurvivalHudAction::BuyItem) { before = hit.area; break; }
    }
    auto *layout = hud.m_movies.GetMovie(hud.m_movies.Ordinal("GLU_MOVIE_POWER_UP_LAYOUT"));
    for (auto &object : layout->objects) {
        if (object.type != 6 || object.frames.empty() || object.frames.front().region != 3) { continue; }
        for (auto &frame : object.frames) { frame.x += 17; }
        if (!hud.DrawOriginalSelector(state)) { ++failures; }
        for (const auto &hit : hud.m_selectorHits) {
            if (hit.storeIndex == static_cast<int>(hud.m_selectorEntries[0]) && hit.action == SurvivalHudAction::BuyItem) { after = hit.area; break; }
        }
        for (auto &frame : object.frames) { frame.x -= 17; }
        break;
    }
    if (before.width == 0 || std::abs(after.x - before.x - 17) > 0.01f) { ++failures; }
    if (glGetError() != GL_NO_ERROR || iconHits == 0 || choices == 0) { ++failures; }
    std::printf("[selector-check] entries=%zu purchases=%u icon-hits=%u choices=%u prompts=3 region-mutation=1 native-reload=1 failures=%u\n",
        hud.m_selectorEntries.size(), purchases, iconHits, choices, failures);
    if (failures != 0) { return 1; }
    return 0;
}
