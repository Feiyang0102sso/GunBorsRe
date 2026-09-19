#include "gun_bros_re/debug/SurvivalDebug.h"
/** @file CInputPad.cpp
 * @brief CInputPad::Base::Bind (:88320) binds meters to regions 0/1 and guns to 2/3.
 */
#define NOMINMAX
#include "gun_bros_re/ui/CInputPad.h"
#include "gun_bros_re/ui/ZMenuData.h"
#include "gun_bros_re/ui/ZTextLayout.h"
#include "gun_bros_re/host/ZHostSettings.h"
#include "gun_bros_re/data/ZPowerupCatalog.h"
#include "engine/platform/ZWindow.h"
#include "engine/resources/CResTOCManager.h"
#include "gun_bros_re/gameplay/level/CLevel.h"
#include "engine/graphics/ZPNG.h"
#include <algorithm>
#include <cstdio>
#include <sstream>

bool CInputPad::Init(CResTOCManager &toc, ZPackTables &tables) {
    m_resources.m_tables = &tables;
    m_resources.m_toc = &toc;
    CResPackTOC &core = *toc.GetPack(toc.GetCorePackIndex());
    if (!m_resources.m_movies.Init(core, core) || !LoadStoreCatalog(toc, tables, m_resources.m_store)) { return false; }
    if (!LoadPowerupCatalog(toc, tables, m_resources.m_powerups)) { return false; }
    for (const ZPowerupEntry &entry : m_resources.m_powerups) {
        const unsigned hash = entry.data.sprite.packHash;
        if (m_resources.m_powerupRenderers.count(hash) != 0) { continue; }
        auto renderer = std::make_unique<ZMovieRenderer>();
        CResPackTOC *pack = toc.GetPack(toc.GetPackIndexFromHash(hash));
        if (pack == nullptr || !renderer->Init(*pack, core)) { return false; }
        m_resources.m_powerupRenderers[hash] = std::move(renderer);
    }
    // First-use font and atlas uploads belong to loading, not the first shot
    m_selector.InitEntries();
    // First-use font and atlas uploads belong to loading, not the first shot
    // or a wave-clear frame. No gameplay or notification state is advanced.
    constexpr unsigned movies[] = {1, 3, 7, 34, 87, 116, 130, 131, 134, 135};
    for (unsigned movie : movies) {
        if (!m_resources.m_movies.Draw(movie, 600)) { return false; }
    }
    constexpr unsigned sprites[] = {6, 7, 8, 27, 28, 35, 36, 39, 87};
    for (unsigned sprite : sprites) { m_resources.m_movies.SpriteDuration(1, sprite); }
    for (unsigned font : {0u, 1u, 5u, 6u, 7u, 9u}) { m_resources.m_movies.TextWidth("0123456789", font); }
    for (const char *name : {"IDS_HUD_EXPERIENCE_UP", "IDS_HUD_POINTS_UP"}) {
        if (m_resources.m_movies.NamedString(name).empty()) { return false; }
    }
    // Composite sticks and badges have their own lazy-expanded frame caches.
    // Draw a neutral snapshot while the loading screen still owns presentation.
    if (!DrawControls(ZInputPadState{})) { return false; }
    return true;
}

std::vector<CInputPad::Button> CInputPad::Buttons(const ZInputPadState &state) const {
    // Every entry, including isolated map studies, uses original input geometry.
    if (state.shopOpen || state.paused || (state.dead && !state.deathmatch) || state.cleared) { return {}; }
    auto buttons = ControlButtons(state);
    if (state.dead) {
        buttons.erase(std::remove_if(buttons.begin(), buttons.end(), [](const Button &button) {
            return button.action != ZInputPadAction::Pause;
        }), buttons.end());
    }
    return buttons;
}

ZInputPadAction CInputPad::Pointer(const ZInputPadState &state, float x, float y, bool down) {
    if (state.remoteShop) { m_previousDown = down; return ZInputPadAction::None; }
    m_mouseX = x;
    m_mouseY = y;
    bool clicked = down && !m_previousDown;
    if (state.shopOpen) {
        const auto action = m_selector.Pointer(state, x, y, down, m_previousDown);
        m_previousDown = down;
        m_challengeHeld = false;
        return action;
    }
    if (!down || !HasChallenges() || state.paused || state.shopOpen || state.dead || state.cleared) { m_challengeHeld = false; }
    if (clicked && !HasInterstitial()) {
        ZMovieRegion button;
        if (FindActionRegion(state, ZInputPadAction::BroOps, button) && button.Contains(x, y)) {
            m_challengeHeld = true;
            m_challengeTime = 0;
        }
    }
    m_previousDown = down;
    if (!clicked) { return ZInputPadAction::None; }

    if (state.paused && !state.shopOpen && (!state.dead || state.deathmatch) && !state.cleared) {
        return PausePointer(state, x, y);
    }
    for (const Button &button : Buttons(state)) {
        if (button.rect.Contains(x, y)) { return button.action; }
    }
    return ZInputPadAction::None;
}

void CInputPad::Centre(const std::string &text, float y, unsigned font, float scale) {
    const float width = m_resources.m_movies.TextWidth(text, font, scale);
    m_resources.m_movies.Text(text, 512 - width * 0.5f, y, font, scale);
}

bool CInputPad::DrawTutorialDebugNotice(std::uint64_t ticks) {
    ::DrawTutorialDebugNotice(m_resources.m_movies, ticks);
    return m_resources.m_movies.Failures() == 0;
}

bool CInputPad::Draw(const ZInputPadState &state) {
    if (m_matchWrapUp) {
        return m_resources.m_movies.Draw(m_resources.m_movies.Ordinal("GLU_MOVIE_MISSION_END"), m_matchWrapUpTime, 512, 384, 1024, 768);
    }
    for (const auto &bar : state.enemyHealthBars) {
        // Original Utility::DrawRect border 0xFF7F8C98 and red fill.
        m_resources.m_movies.Rectangle(bar.x, bar.y, bar.width, 1, 127 / 255.0f, 140 / 255.0f, 152 / 255.0f, 1);
        m_resources.m_movies.Rectangle(bar.x, bar.y + bar.height - 1, bar.width, 1, 127 / 255.0f, 140 / 255.0f, 152 / 255.0f, 1);
        m_resources.m_movies.Rectangle(bar.x, bar.y, 1, bar.height, 127 / 255.0f, 140 / 255.0f, 152 / 255.0f, 1);
        m_resources.m_movies.Rectangle(bar.x + bar.width - 1, bar.y, 1, bar.height, 127 / 255.0f, 140 / 255.0f, 152 / 255.0f, 1);
        m_resources.m_movies.Rectangle(bar.x + bar.border, bar.y + bar.border,
            (bar.width - 2 * bar.border) * bar.fraction, bar.height - 2 * bar.border,
            bar.red, bar.green + std::max(0.0f, bar.red - 200 / 255.0f), bar.blue + std::max(0.0f, bar.red - 200 / 255.0f), 1);
    }
    if (!state.shopOpen) { m_selector.Close(); }
    if (!state.paused && m_pauseBound) { m_pauseHelp = false; ResetPauseList(); }
    ObserveProgress(state);
    if (state.withBrother && state.brotherHealth > 0 && state.brotherLabelAlpha > 0) {
        m_resources.m_movies.Text(state.brotherName, state.brotherLabelX - m_resources.m_movies.TextWidth(state.brotherName, 0) * 0.5f,
            state.brotherLabelY - m_resources.m_movies.TextHeight(0) / 2, 0, 1, 0, state.brotherLabelAlpha);
    }
    if (state.localLive && state.reviveProgress > 0) {
        // CLevel::DrawReviveBar :120152: native rectangles, not a Movie.
        const auto &bar = state.reviveBar;
        m_resources.m_movies.Rectangle(bar.x, bar.y, bar.width, bar.height, 127.0f/255, 140.0f/255, 152.0f/255, 1);
        m_resources.m_movies.Rectangle(bar.x + state.revivePaddingX, bar.y + state.revivePaddingY,
            std::max(0.0f, bar.width - 2 * state.revivePaddingX) * state.reviveProgress,
            std::max(0.0f, bar.height - 2 * state.revivePaddingY), 100.0f/255, 182.0f/255, 253.0f/255, 1);
    }
    // Preserve the authored iPad bottom rail and original top-left pause control.
    // Exact ARMv7 CLevelIndicator::INDICATOR_ANIMS bytes at 0x3C4AB0.
    constexpr unsigned indicatorAnimations[7][3] = {{31,31,255}, {67,67,68}, {53,54,255},
        {69,69,70}, {64,64,65}, {64,64,66}, {69,69,70}};
    // CLevelIndicator::Init :191653 uses 25/25/100 camera viewport units.
    // Convert those physical-screen margins into the host's logical HUD canvas.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const float viewportScale = std::min(viewport[2] / 480.0f, viewport[3] / 320.0f);
    const float horizontalMargin = int(25 * viewportScale) * 1024.0f / viewport[2];
    const float topMargin = int(25 * viewportScale) * 768.0f / viewport[3];
    const float bottomMargin = int(100 * viewportScale) * 768.0f / viewport[3];
    for (const CLevelIndicator &indicator : state.indicators) {
        if (indicator.type >= 7 || indicator.IsDone()) { continue; }
        const float x = std::clamp(indicator.x, horizontalMargin, 1024.0f - horizontalMargin);
        const float y = std::clamp(indicator.y, topMargin, 768.0f - bottomMargin);
        const float dx = indicator.x - x, dy = indicator.y - y;
        float angle = 0;
        if (std::hypot(dx, dy) >= 1) { angle = std::atan2(dx, -dy) * 180 / 3.14159265f; }
        unsigned animation = indicatorAnimations[indicator.type][0];
        unsigned elapsed = indicator.elapsedMs;
        // Type 2 has a distinct appearing animation before its idle loop.
        if (indicator.type == 2) {
            const unsigned introDuration = m_resources.m_movies.SpriteDuration(1, animation);
            if (introDuration > 0 && elapsed >= introDuration) { animation = indicatorAnimations[2][1]; elapsed -= introDuration; }
        }
        m_resources.m_movies.DrawSprite(1, animation, elapsed, x, y, 1, indicator.Alpha(), angle);
        const unsigned icon = indicatorAnimations[indicator.type][2];
        if (icon != 255) { m_resources.m_movies.DrawSprite(1, icon, elapsed, x, y, 1, indicator.Alpha()); }
    }
    if (!DrawControls(state)) { return false; }
    m_challengeRows = 0;
    if ((m_challengeHeld || m_challengeTime != 0) && !state.paused && !state.shopOpen && !state.dead && !state.cleared) {
        // ShowChallengeInfoOverlay uses HUD region2's bottom-center as origin.
        ZMovieRegion origin;
        const auto peripheral = m_resources.m_movies.Ordinal("GLU_MOVIE_HUD_PAUSE");
        unsigned start = 0, end = 0;
        if (!m_resources.m_movies.GetMovie(peripheral)->GetChapterRange(5, start, end) || !m_resources.m_movies.Region(peripheral, 2, start, origin)) { return false; }
        if (!DrawChallengeOverlay(origin.x + origin.width / 2, origin.y + origin.height, m_challengeTime)) { return false; }
    }
    DrawNotice();
    DrawSurvivalDebugInfo(m_resources.m_movies, state);
    if (state.shopOpen) { return m_selector.DrawSelector(state); }
    if (state.paused && (!state.dead || state.deathmatch) && !state.cleared) { return DrawPause(state); }
    // End-of-run navigation is owned by the original postgame menu in the shell.
    // CDialogPopup draws the original radio portrait beside its text region.
    if (!m_dialog.Draw()) { return false; }
    return m_resources.m_movies.Failures() == 0;
}

std::string CInputPad::NoticeNumber(const char *name, unsigned number) {
    // OnLevelUp/OnLevelStart/OnWaveClear pass one signed integer to SWPrintF_S.
    // ARM: 0x62A40, 0x63588, 0x62714 and 0x627B0 (LEVEL reward percent).
    std::string text = m_resources.m_movies.NamedString(name);
    std::size_t position = text.find("%i");
    if (position == std::string::npos) { position = text.find("%d"); }
    if (position != std::string::npos) { text.replace(position, 2, std::to_string(number)); }
    std::size_t percent = text.find("%%");
    while (percent != std::string::npos) {
        text.replace(percent, 2, "%");
        percent = text.find("%%", percent + 1);
    }
    return text;
}

std::string CInputPad::PauseText(const ZInputPadState &state, unsigned index, unsigned slot) {
    const char *table = "MDS_PAUSE_ROOT";
    if (m_pauseHelp) { table = "MDS_HELP"; }
    const auto *entry = FindMenuData(table, index);
    if (entry == nullptr || slot > 1) { return {}; }
    if (entry->strings[slot][0] != 0) { return m_resources.m_movies.NamedString(entry->strings[slot]); }
    // CMenuAction::ResolveActionString :95837..95911.
    if (entry->action == 9) {
        if (state.soundEnabled) { return m_resources.m_movies.NamedString("IDS_SOUND_ON_GAME"); }
        return m_resources.m_movies.NamedString("IDS_SOUND_OFF_GAME");
    }
    if (entry->action == 10) {
        if (state.musicEnabled) { return m_resources.m_movies.NamedString("IDS_MUSIC_ON_GAME"); }
        return m_resources.m_movies.NamedString("IDS_MUSIC_OFF_GAME");
    }
    if (entry->action == 16) {
        if (state.dockedSticks) { return m_resources.m_movies.NamedString("IDS_DOCKED_STICKS_ON_GAME"); }
        return m_resources.m_movies.NamedString("IDS_DOCKED_STICKS_OFF_GAME");
    }
    return {};
}

ZInputPadAction CInputPad::PausePointer(const ZInputPadState &state, float x, float y) {
    for (const auto &hit : m_pauseHits) {
        if (!hit.first.Contains(x, y)) { continue; }
        if (hit.second == UINT32_MAX) { BackFromHelp(); return ZInputPadAction::None; }
        const unsigned item = hit.second;
        if (item != m_pauseFocus) {
            m_pauseButtonTimes[m_pauseFocus] = 0;
            m_pauseFocus = item;
            m_pauseBodyTime = 0;
            m_pauseBodyPosition = 0;
            unsigned start = 0, end = 0;
            m_resources.m_movies.GetMovie(m_resources.m_movies.Ordinal("GLU_MOVIE_LIST_MENU_BUTTON"))->GetChapterRange(2, start, end);
            m_pauseButtonTimes[item] = start;
            m_pauseTarget = std::clamp(float(item) - 1, 0.0f, float(m_pauseItems.size()) - 3);
        }
        const char *table = "MDS_PAUSE_ROOT";
        if (m_pauseHelp) { table = "MDS_HELP"; }
        const auto *entry = FindMenuData(table, m_pauseItems[item]);
        switch (entry->action) {
        case 31: return ZInputPadAction::Resume;
        case 40: return ZInputPadAction::Exit;
        case 9: return ZInputPadAction::Sound;
        case 10: return ZInputPadAction::Music;
        case 16: return ZInputPadAction::DockedSticks;
        case 1: m_pauseHelp = true; ResetPauseList(); break;
        }
        return ZInputPadAction::None;
    }
    return ZInputPadAction::None;
}

std::vector<CInputPad::Button> CInputPad::ControlButtons(const ZInputPadState &state) const {
    std::vector<Button> buttons;
    const unsigned base = m_resources.m_movies.Ordinal("GLU_MOVIE_HUD_PAD_IPAD");
    const unsigned peripheral = m_resources.m_movies.Ordinal("GLU_MOVIE_HUD_PAUSE");
    unsigned idle = 0, end = 0;
    unsigned chapter = 3;
    if (HasChallenges()) { chapter = 5; }
    if (!m_resources.m_movies.GetMovie(peripheral)->GetChapterRange(chapter, idle, end)) { return buttons; }
    if (HasChallenges()) { idle = end; }
    ZMovieRegion region;
    if (HasChallenges() && m_resources.m_movies.Region(peripheral, 4, idle, region)) { buttons.push_back({region, ZInputPadAction::BroOps, ""}); }
    if (m_resources.m_movies.Region(peripheral, 3, idle, region)) { buttons.push_back({region, ZInputPadAction::Pause, ""}); }
    if (m_resources.m_movies.Region(base, 2, 0, region)) { buttons.push_back({region, ZInputPadAction::OpenShop, ""}); }
    if (m_resources.m_movies.Region(base, 3, 0, region)) { buttons.push_back({region, ZInputPadAction::SwapWeapon, ""}); }
    for (unsigned slot = 0; slot < 2; ++slot) {
        if (!m_resources.m_movies.Region(base, 4 + slot, 0, region)) { continue; }
        const float x = region.x + int(region.width) / 2, y = region.y + int(region.height) / 2;
        const char *name = "GLU_MOVIE_POWERUP_BUTTON";
        ZInputPadAction action = ZInputPadAction::UseLeft;
        if (slot == 1) { name = "GLU_MOVIE_GRENADE_BUTTON"; action = ZInputPadAction::UseItem; }
        const unsigned movie = m_resources.m_movies.Ordinal(name);
        if (!m_resources.m_movies.GetMovie(movie)->GetChapterRange(2, idle, end)) { continue; }
        for (const auto &part : m_resources.m_movies.Regions(movie, idle, x, y, true)) {
            if (part.index == 0) { buttons.push_back({part, action, ""}); }
        }
    }
    return buttons;
}
