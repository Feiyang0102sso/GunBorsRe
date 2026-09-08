/** @file SurvivalHud.cpp
 * @brief CInputPad::Base::Bind (:88320) binds meters to regions 0/1 and guns to 2/3.
 */
#define NOMINMAX
#include "runtime/SurvivalHud.h"
#include "engine/platform/CWindow.h"
#include "gun_bros/CResTOCManager.h"
#include "engine/CPNG.h"
#include <algorithm>
#include <cstdio>
#include <sstream>

bool SurvivalHud::Init(CResTOCManager &toc, PackTables &tables) {
    m_tables = &tables;
    CResPackTOC &core = *toc.GetPack(toc.GetCorePackIndex());
    return m_movies.Init(core, core) && LoadStoreCatalog(toc, tables, m_store);
}

void SurvivalHud::Icon(unsigned type, const GameObjectRef &object, const MovieRegion &region) {
    if (object.IsNull()) { return; }
    for (const StoreEntry &entry : m_store) {
        for (const GameObjectTypeRef &reference : entry.data.objects) {
            if (reference.type != type || reference.object.packHash != object.packHash || reference.object.localIndex != object.localIndex) { continue; }
            const CGameAssetRef &image = entry.data.assets[1];
            if (image.IsNull() || image.assetId < 0) { continue; }
            const std::uint64_t key = (static_cast<std::uint64_t>(image.packHash) << 32) | image.assetId;
            if (m_icons.count(key) == 0) {
                std::vector<std::uint8_t> bytes;
                PNGImage decoded;
                auto texture = std::make_unique<CTexture>();
                if (!m_tables->ReadSectionResource(image.packHash, GameSection::Png, image.assetId, bytes) ||
                    !PNGDecode(bytes, decoded) || !texture->Create(decoded)) { return; }
                m_icons[key] = std::move(texture);
            }
            m_movies.Image(*m_icons[key], region.x, region.y, region.width, region.height);
            return;
        }
    }
}

std::vector<SurvivalHud::Button> SurvivalHud::Buttons(const SurvivalHudState &state) const {
    if (!state.dialog.empty()) {
        return {{{0, 0, 716, 572, 256, 56}, SurvivalHudAction::Continue, "CONTINUE"}};
    }
    if (state.paused || state.dead || state.cleared) {
        std::vector<Button> result;
        if (state.paused && !state.dead && !state.cleared) {
            result.push_back({{0, 0, 242, 522, 260, 62}, SurvivalHudAction::Resume, "RESUME"});
        } else {
            result.push_back({{0, 0, 242, 522, 260, 62}, SurvivalHudAction::Retry, "PLAY AGAIN"});
        }
        result.push_back({{0, 0, 522, 522, 260, 62}, SurvivalHudAction::Exit, "MAIN MENU"});
        if (state.paused && !state.dead && !state.cleared) {
            result.push_back({{0, 0, 382, 598, 260, 58}, SurvivalHudAction::Retry, "RESTART"});
        }
        return result;
    }
    return {
        {{0, 0, 0, 0, 75, 58}, SurvivalHudAction::Pause, ""},
        {{0, 0, 262, 685, 118, 80}, SurvivalHudAction::Weapon1, "1"},
        {{0, 0, 648, 685, 118, 80}, SurvivalHudAction::Weapon2, "2"},
        {{0, 0, 806, 671, 194, 58}, SurvivalHudAction::UseItem, "USE [G]"},
        {{0, 0, 840, 734, 150, 30}, SurvivalHudAction::NextItem, "NEXT [F]"}
    };
}

SurvivalHudAction SurvivalHud::Pointer(const SurvivalHudState &state, float x, float y, bool down) {
    m_mouseX = x;
    m_mouseY = y;
    const bool clicked = down && !m_previousDown;
    m_previousDown = down;
    if (!clicked) { return SurvivalHudAction::None; }
    for (const Button &button : Buttons(state)) {
        if (button.rect.Contains(x, y)) { return button.action; }
    }
    return SurvivalHudAction::None;
}

bool SurvivalHud::CapturesPointer(const SurvivalHudState &state, float x, float y) const {
    if (state.paused || state.dead || state.cleared || !state.dialog.empty()) { return true; }
    for (const Button &button : Buttons(state)) {
        if (button.rect.Contains(x, y)) { return true; }
    }
    return y >= 682;
}

void SurvivalHud::Centre(const std::string &text, float y, unsigned font, float scale) {
    const float width = m_movies.TextWidth(text, font, scale);
    m_movies.Text(text, 512 - width * 0.5f, y, font, scale);
}

bool SurvivalHud::Draw(const SurvivalHudState &state) {
    ObserveProgress(state);
    if (state.withBrother && state.brotherHealth > 0 && state.brotherLabelAlpha > 0) {
        m_movies.Text(state.brotherName, state.brotherLabelX - m_movies.TextWidth(state.brotherName, 0, 0.7f) * 0.5f,
            state.brotherLabelY - 8, 0, 0.7f, 0, state.brotherLabelAlpha);
    }
    // Preserve the authored iPad bottom rail and original top-left pause control.
    // Exact ARMv7 CLevelIndicator::INDICATOR_ANIMS bytes at 0x3C4AB0.
    constexpr unsigned indicatorAnimations[7][3] = {{31,31,255}, {67,67,68}, {53,54,255},
        {69,69,70}, {64,64,65}, {64,64,66}, {69,69,70}};
    for (const CLevelIndicator &indicator : state.indicators) {
        if (indicator.type >= 7 || indicator.IsDone()) { continue; }
        const float x = std::clamp(indicator.x, 20.0f, 1004.0f);
        const float y = std::clamp(indicator.y, 20.0f, 688.0f);
        const float dx = indicator.x - x, dy = indicator.y - y;
        float angle = 0;
        if (std::hypot(dx, dy) >= 1) { angle = std::atan2(dx, -dy) * 180 / 3.14159265f; }
        unsigned animation = indicatorAnimations[indicator.type][0];
        unsigned elapsed = indicator.elapsedMs;
        // Type 2 has a distinct appearing animation before its idle loop.
        if (indicator.type == 2) {
            const unsigned introDuration = m_movies.SpriteDuration(1, animation);
            if (introDuration > 0 && elapsed >= introDuration) { animation = indicatorAnimations[2][1]; elapsed -= introDuration; }
        }
        m_movies.DrawSprite(1, animation, elapsed, x, y, 1, indicator.Alpha(), angle);
        const unsigned icon = indicatorAnimations[indicator.type][2];
        if (icon != 255) { m_movies.DrawSprite(1, icon, elapsed, x, y, 1, indicator.Alpha()); }
    }
    m_movies.Draw(116, 0);
    m_movies.Draw(3, 600);
    for (const MovieRegion &region : m_movies.Regions(116, 0)) {
        if (region.index > 1) { continue; }
        float fraction = state.health / std::max(1.0f, state.maximumHealth);
        if (region.index == 1) { fraction = static_cast<float>(state.experience) / std::max<std::uint64_t>(1, state.experienceDelta); }
        if (region.index == 0) { m_movies.Rectangle(region.x, region.y, region.width * std::clamp(fraction, 0.0f, 1.0f), region.height, 0.85f, 0.08f, 0.04f); }
        else { m_movies.Rectangle(region.x, region.y, region.width * std::clamp(fraction, 0.0f, 1.0f), region.height, 0.04f, 0.5f, 1); }
    }
    m_movies.Rectangle(366, 0, 300, 67, 0, 0, 0, 0.65f);
    const unsigned visibleWave = std::min(state.wave, 499u);
    std::string waveTitle = "WAVE " + std::to_string(visibleWave % 50 + 1);
    std::string waveSubtitle = "REVOLUTION " + std::to_string(visibleWave / 50 + 1) + " / 10";
    if (state.horde) {
        waveTitle = "HORDE " + std::to_string(state.wave + 1);
        char stopwatch[32];
        std::snprintf(stopwatch, sizeof(stopwatch), "%02u:%02u.%u", state.stopwatchMs / 60000,
            state.stopwatchMs / 1000 % 60, state.stopwatchMs / 100 % 10);
        waveSubtitle = std::string("BOKOR  ") + stopwatch;
    }
    Centre(waveTitle, 7, 5, 0.7f);
    Centre(waveSubtitle, 43, 0, 0.65f);
    std::string reward = "XPLODIUM " + std::to_string(state.xplodium);
    if (state.horde) { reward = "POINTS " + std::to_string(state.score); }
    m_movies.Text(reward, 788, 14, 0, 0.8f, 224);
    m_movies.Text("ENEMIES " + std::to_string(state.enemies), 788, 39, 0, 0.68f);
    m_movies.Text("KILLS " + std::to_string(state.kills), 788, 62, 0, 0.68f);
    if (state.horde) { m_movies.Text("STREAK x" + std::to_string(state.killStreak + 1), 788, 86, 0, 0.68f); }
    else if (state.xplodiumMultiplier != 100) { m_movies.Text("XPLODIUM " + std::to_string(state.xplodiumMultiplier) + "%", 788, 86, 0, 0.68f); }
    m_movies.Text("LV " + std::to_string(state.level), 464, 711, 0, 0.65f);
    m_movies.Text(state.weapon, 384, 680, 0, 0.64f, 255);
    m_movies.Text("HP " + std::to_string(static_cast<unsigned>(std::max(0.0f, state.health))) + " / " +
        std::to_string(static_cast<unsigned>(state.maximumHealth)), 12, 698, 0, 0.7f, 235);
    if (state.withBrother) {
        std::string brother = "BRO HP " + std::to_string(static_cast<unsigned>(std::max(0.0f, state.brotherHealth)));
        if (state.brotherHealth <= 0) { brother = "BRO RETURNS NEXT WAVE"; }
        m_movies.Text(brother, 12, 727, 0, 0.63f, 230);
    }
    m_movies.Text(state.item + " x" + std::to_string(state.itemCount), 754, 643, 0, 0.6f, 255);
    if (!state.buffs.empty()) { m_movies.Text(state.buffs, 16, 621, 0, 0.65f, 950); }
    if (state.transitioning && !state.paused && !state.dead) {
        if (m_notices.empty()) { Centre("GET READY", 315, 5, 1.2f); }
    }
    DrawNotice();
    if (state.paused || state.dead || state.cleared) {
        m_movies.Draw(21, 850);
        m_movies.Draw(111, 600, 512, 350);
        std::string title = "PAUSED";
        if (state.dead) { title = "MISSION FAILED"; }
        if (state.cleared) { title = "MISSION COMPLETE"; }
        Centre(title, 205, 5, 0.95f);
        Centre(waveTitle + "   " + waveSubtitle, 297, 0, 0.75f);
        Centre("KILLS " + std::to_string(state.kills) + "     " + reward, 345, 0, 0.85f);
        Centre("WASD: MOVE   MOUSE: AIM / FIRE   1 / 2: WEAPONS", 415, 0, 0.67f);
        Centre("G: USE ITEM   F: NEXT ITEM   ESC / SPACE: PAUSE", 446, 0, 0.67f);
    }
    if (!state.dialog.empty()) {
        m_movies.Rectangle(32, 398, 960, 238, 0, 0.02f, 0.04f, 0.96f);
        std::istringstream words(state.dialog);
        std::string word, line;
        float y = 420;
        while (words >> word) {
            if (m_movies.TextWidth(line + " " + word, 0, 0.8f) > 900) {
                m_movies.Text(line, 50, y, 0, 0.8f);
                line.clear();
                y += 27;
            }
            if (!line.empty()) { line += ' '; }
            line += word;
        }
        m_movies.Text(line, 50, y, 0, 0.8f);
    }
    for (const Button &button : Buttons(state)) {
        const MovieRegion &rect = button.rect;
        if (button.label[0] == 0) { continue; }
        const bool hovered = rect.Contains(m_mouseX, m_mouseY);
        m_movies.ButtonBackground(rect.x, rect.y, rect.width, rect.height, false, hovered);
        bool weaponButton = false;
        if (button.action == SurvivalHudAction::Weapon1 || button.action == SurvivalHudAction::Weapon2) {
            unsigned slot = 0;
            if (button.action == SurvivalHudAction::Weapon2) { slot = 1; }
            const MovieRegion icon{0, 0, rect.x + 14, rect.y + 4, rect.width - 28, rect.height - 16};
            Icon(6, state.guns[slot], icon);
            m_movies.Text(button.label, rect.x + 7, rect.y + 5, 0, 0.6f);
            weaponButton = true;
        }
        float scale = 0.8f;
        const float textWidth = m_movies.TextWidth(button.label, 0, scale);
        if (!weaponButton) { m_movies.Text(button.label, rect.x + (rect.width - textWidth) / 2, rect.y + (rect.height - 23 * scale) / 2, 0, scale); }
        if ((button.action == SurvivalHudAction::Weapon1 && state.weaponSlot == 0) ||
            (button.action == SurvivalHudAction::Weapon2 && state.weaponSlot == 1)) {
            m_movies.Rectangle(rect.x + 12, rect.y + rect.height - 9, rect.width - 24, 3, 1, 0.65f, 0);
        }
    }
    return m_movies.Failures() == 0;
}

void SurvivalHud::ResetNotices() {
    m_notices.clear();
    m_observedProgress = false;
    m_previousBossIntro = 0;
}

void SurvivalHud::ObserveProgress(const SurvivalHudState &state) {
    bool bossStarted = false;
    if (state.bossIntroSerial != m_previousBossIntro) {
        m_previousBossIntro = state.bossIntroSerial;
        if (state.bossIntroSerial > 0) {
            bossStarted = true;
            // OnBossWaveStart :90041 clears pending overlays before this one.
            m_notices.clear();
            m_notices.push_back({34, 0, m_movies.NamedString("IDS_HUD_BOSS_WAVE_START"), ""});
        }
    }
    // CInputPad::Init / SetUserRegionCallback :88231 / :90944. Reopening a
    // saved account sets the baseline; it must not replay old level-up notices.
    if (m_observedProgress && !bossStarted) {
        if (state.wave < m_previousWave) { m_notices.clear(); }
        if (state.level > m_previousLevel) {
            m_notices.push_back({7, 0, "LEVEL UP!  " + std::to_string(state.level), ""});
        }
        if (state.wave > m_previousWave) {
            if (state.perfectBonus > 0 && !state.horde) {
                m_notices.push_back({87, 0, "PERFECT WAVE!", "+" + std::to_string(state.perfectBonus) + " XPLODIUM"});
            } else { m_notices.push_back({34, 0, "WAVE CLEARED!", ""}); }
        }
    }
    m_previousLevel = state.level;
    m_previousWave = state.wave;
    m_observedProgress = true;
}

void SurvivalHud::Advance(int deltaMs) {
    if (deltaMs <= 0 || m_notices.empty()) { return; }
    Notice &notice = m_notices.front();
    notice.elapsed += static_cast<unsigned>(deltaMs);
    CMovie *movie = m_movies.GetMovie(notice.movie);
    if (movie != nullptr && notice.elapsed >= movie->duration) { m_notices.erase(m_notices.begin()); }
}

void SurvivalHud::DrawNotice() {
    if (m_notices.empty()) { return; }
    const Notice &notice = m_notices.front();
    m_movies.Draw(notice.movie, notice.elapsed);
    for (const MovieRegion &region : m_movies.Regions(notice.movie, notice.elapsed)) {
        std::string text = notice.title;
        if (region.index == 1) { text = notice.footer; }
        if (region.index > 1 || text.empty()) { continue; }
        float scale = std::min(1.3f, region.height / 27.0f);
        const float width = m_movies.TextWidth(text, 5, scale);
        if (width > region.width) { scale *= region.width / width; }
        m_movies.Text(text, region.x + (region.width - m_movies.TextWidth(text, 5, scale)) * 0.5f,
            region.y + (region.height - 27 * scale) * 0.5f, 5, scale, 0, region.alpha);
    }
}

int RunSurvivalHudCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    CWindow window;
    if (!window.Open("Gun Bros - HUD Check", 1024, 768)) { return 1; }
    SurvivalHud hud;
    PackTables tables(toc);
    if (!hud.Init(toc, tables)) { return 1; }
    SurvivalHudState state;
    state.health = 750; state.maximumHealth = 1000; state.brotherHealth = 800;
    state.withBrother = true; state.level = 12; state.experience = 250; state.experienceDelta = 1000;
    state.wave = 52; state.enemies = 18; state.kills = 44; state.xplodium = 937;
    state.weapon = "WHIPPERSNAPPERS"; state.item = "FRAG GRENADE"; state.itemCount = 5;
    unsigned failures = 0;
    if (hud.Pointer(state, 20, 20, true) != SurvivalHudAction::Pause) { ++failures; }
    if (hud.Pointer(state, 20, 20, true) != SurvivalHudAction::None) { ++failures; }
    hud.Pointer(state, 20, 20, false);
    state.paused = true;
    if (hud.Pointer(state, 300, 550, true) != SurvivalHudAction::Resume) { ++failures; }
    hud.Pointer(state, 300, 550, false);
    if (!hud.CapturesPointer(state, 500, 300)) { ++failures; }
    state.paused = false;
    if (hud.CapturesPointer(state, 500, 300)) { ++failures; }
    const char *names[] = {"active", "paused", "dead", "complete", "perfect"};
    for (unsigned index = 0; index < 5; ++index) {
        state.paused = index == 1; state.dead = index == 2; state.cleared = index == 3;
        state.transitioning = index == 4; state.perfectBonus = 100;
        if (index == 4) {
            ++state.wave;
            if (!hud.Draw(state)) { ++failures; }
            hud.Advance(1200);
        }
        glClearColor(0.06f, 0.10f, 0.13f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (!hud.Draw(state) || !window.SaveFrame(std::string("out/hud-check-") + names[index] + ".png")) { ++failures; }
        window.Present();
    }
    hud.ResetNotices();
    state.transitioning = false;
    state.perfectBonus = 0;
    if (!hud.Draw(state)) { ++failures; }
    ++state.level;
    ++state.wave;
    if (!hud.Draw(state) || hud.NoticeCount() != 2) { ++failures; }
    hud.Advance(800);
    hud.Advance(0);
    if (hud.NoticeTime() != 800) { ++failures; }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!hud.Draw(state) || !window.SaveFrame("out/hud-check-level-up.png")) { ++failures; }
    hud.Advance(1700);
    if (hud.NoticeCount() != 1 || hud.NoticeTime() != 0) { ++failures; }
    hud.Advance(900);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!hud.Draw(state) || !window.SaveFrame("out/hud-check-wave-cleared.png")) { ++failures; }
    hud.Advance(1100);
    if (hud.NoticeCount() != 0) { ++failures; }
    hud.ResetNotices();
    if (!hud.Draw(state) || hud.NoticeCount() != 0) { ++failures; }
    ++state.bossIntroSerial;
    ++state.wave;
    if (!hud.Draw(state) || hud.NoticeCount() != 1) { ++failures; }
    hud.Advance(900);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!hud.Draw(state) || !window.SaveFrame("out/hud-check-boss.png")) { ++failures; }
    hud.Advance(1100);
    if (hud.NoticeCount() != 0) { ++failures; }
    for (unsigned type = 0; type < 7; ++type) {
        CLevelIndicator indicator;
        indicator.type = type;
        indicator.x = 95 + type * 135.0f;
        indicator.y = -200;
        indicator.elapsedMs = 700;
        state.indicators.push_back(indicator);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!hud.Draw(state) || !window.SaveFrame("out/hud-check-indicators.png")) { ++failures; }
    std::printf("[hud-check] states=9 indicator-types=7 original-notice-queue=1 boss-replaces-queue=1 pause-restart=1 failures=%u\n", failures);
    return failures != 0;
}
