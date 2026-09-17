/** @file CInputPadControls.cpp
 * @brief CInputPad::Base::Bind (:88320) binds meters to regions 0/1 and guns to 2/3.
 */
#define NOMINMAX
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

bool CInputPad::RestoreForPowerup(CPowerup &powerup) {
    m_animationPowerup = &powerup;
    m_restoreBase = m_controlsHidden || m_selectorWasOpen;
    m_restorePeripheralPending = m_controlsHidden;
    if (m_restorePeripheralPending) {
        const auto *movie = m_resources.m_movies.GetMovie(m_resources.m_movies.Ordinal("GLU_MOVIE_HUD_PAUSE"));
        if (movie == nullptr) { m_animationPowerup = nullptr; return false; }
        m_restorePeripheral.Bind(*movie);
        // Native 13 requests Peripheral state 1: SetState :88097 -> chapter 5.
        if (!m_restorePeripheral.SetChapter(5)) { m_animationPowerup = nullptr; return false; }
    }
    if (!m_restoreBase && !m_restorePeripheralPending) {
        m_animationPowerup = nullptr;
        powerup.OnInputPadAnimationComplete();
    }
    return true;
}

void CInputPad::CancelPowerupAnimation(CPowerup &powerup) {
    if (m_animationPowerup != &powerup) { return; }
    m_animationPowerup = nullptr;
    m_restorePeripheral.Cancel();
    m_restoreBase = false;
    m_restorePeripheralPending = false;
}

void CInputPad::UpdatePowerupAnimation(unsigned deltaMs) {
    if (m_animationPowerup == nullptr || deltaMs == 0) { return; }
    if (m_restoreBase) {
        // Base::Update :88366 states 1/8. This is the original alpha rate
        // (2 * deltaMs * 0.001), not a substitute completion timer.
        m_baseAlpha = std::min(1.0f, m_baseAlpha + 2.0f * deltaMs * 0.001f);
        if (m_baseAlpha < 1) { return; }
        m_restoreBase = false;
        if (m_restorePeripheralPending) { return; }
    }
    if (m_restorePeripheralPending) {
        m_restorePeripheral.Update(deltaMs);
        if (!m_restorePeripheral.TakeCompletion()) { return; }
        m_restorePeripheralPending = false;
    }
    CPowerup *powerup = m_animationPowerup;
    m_animationPowerup = nullptr;
    m_controlsHidden = false;
    m_selectorWasOpen = false;
    powerup->OnInputPadAnimationComplete();
}

bool CInputPad::FindActionRegion(const ZInputPadState &state, ZInputPadAction action, ZMovieRegion &region) const {
    if (state.shopOpen) { return m_selector.FindActionRegion(action, region); }
    if (state.paused && !m_pauseHelp && action == ZInputPadAction::Resume) {
        for (const auto &hit : m_pauseHits) {
            if (hit.second >= m_pauseItems.size()) { continue; }
            const auto *entry = FindMenuData("MDS_PAUSE_ROOT", m_pauseItems[hit.second]);
            if (entry->action == 31) { region = hit.first; return true; }
        }
        return false;
    }
    for (const auto &button : Buttons(state)) {
        if (button.action == action) { region = button.rect; return true; }
    }
    return false;
}

bool CInputPad::CapturesPointer(const ZInputPadState &state, float x, float y) const {
    if (state.shopOpen || state.paused || state.dead || state.cleared) { return true; }
    for (const Button &button : Buttons(state)) {
        if (button.rect.Contains(x, y)) { return true; }
    }
    return false;
}

bool CInputPad::DrawMeter(const ZMovieRegion &area, unsigned slot) {
    // Original native configuration: border1, outline, full top/bottom,
    // empty top/bottom, separator flag/color. CInputPadMeter::Draw :130857.
    constexpr unsigned colors[2][7] = {
        {0xffb1beca, 0xff01d810, 0xff01a60c, 0xffdb0b0b, 0xffa80b0b, 1, 0xff28750e},
        {0xffb1beca, 0xff0195d7, 0xff0174a6, 0xff8e8e8e, 0xff787878, 0, 0}
    };
    const unsigned *config = colors[slot];
    const float x = area.x, y = area.y;
    const int width = int(area.width), height = int(area.height);
    const unsigned outline = config[0];
    const float red = float((outline >> 16) & 255) / 255, green = float((outline >> 8) & 255) / 255, blue = float(outline & 255) / 255;
    // Utility::DrawRect draws a one-pixel outline; native meter inset is border+1.
    m_resources.m_movies.Rectangle(x, y, width, 1, red, green, blue, area.alpha);
    m_resources.m_movies.Rectangle(x, y + height - 1, width, 1, red, green, blue, area.alpha);
    m_resources.m_movies.Rectangle(x, y, 1, height, red, green, blue, area.alpha);
    m_resources.m_movies.Rectangle(x + width - 1, y, 1, height, red, green, blue, area.alpha);
    const int interiorWidth = width - 4, interiorHeight = height - 4;
    const int filled = int(interiorWidth * m_meters[slot].GetDrawValue());
    const unsigned highlight = m_meters[slot].GetHighlight();
    unsigned top = 0, bottom = 0, divider = 0;
    for (unsigned channel = 0; channel < 3; ++channel) {
        const unsigned shift = channel * 8;
        top |= std::min(255u, ((config[1] >> shift) & 255) + highlight) << shift;
        bottom |= std::min(255u, ((config[2] >> shift) & 255) + highlight) << shift;
        divider |= std::min(255u, ((config[6] >> shift) & 255) + highlight) << shift;
    }
    int gradientWidth = filled;
    if (config[5] && filled != interiorWidth && filled > 0) {
        --gradientWidth;
        m_resources.m_movies.Rectangle(x + 2 + gradientWidth, y + 2, 1, interiorHeight,
            float((divider >> 16) & 255) / 255, float((divider >> 8) & 255) / 255, float(divider & 255) / 255, area.alpha);
    }
    return m_resources.m_movies.Gradient(x + 2, y + 2, gradientWidth, interiorHeight, top, bottom, area.alpha) &&
        m_resources.m_movies.Gradient(x + 2 + filled, y + 2, interiorWidth - filled, interiorHeight, config[3], config[4], area.alpha);
}

bool CInputPad::DrawPowerup(const ZInputPadState &state, unsigned slot, float x, float y, unsigned movie, unsigned time, float alpha) {
    const GameObjectRef *object = &state.leftPowerup;
    unsigned count = state.leftCount;
    if (slot == 1) { object = &state.rightPowerup; count = state.rightCount; }
    const ZPowerupEntry *powerup = nullptr;
    for (const auto &entry : m_resources.m_powerups) {
        if (entry.resource.packHash == object->packHash && entry.resource.localIndex == object->localIndex) { powerup = &entry; break; }
    }
    class PowerupCallback : public ZMovieRegionCallback {
    public:
        PowerupCallback(CInputPad &owner, const ZPowerupEntry *item, unsigned quantity, const ZInputPadState &snapshot) : hud(owner), powerup(item), count(quantity), state(snapshot) {}
        bool DrawMovieRegion(const ZMovieRegion &region) override {
            // Original binding: region1 = count, region2 = alternate input icon.
            if (powerup == nullptr) { return true; }
            if (region.index == 1) {
                const auto cooldown = state.powerupCooldowns.find(powerup->resource.localIndex);
                if (state.deathmatch && cooldown != state.powerupCooldowns.end() && cooldown->second > 0) {
                    return hud.m_selector.DrawPowerupCooldown(*powerup, cooldown->second, region);
                }
                unsigned animation = 87;
                if (count > 9) { animation = 88; }
                const int x = int(region.x) + int(region.width) / 2, y = int(region.y) + int(region.height) / 2;
                if (!hud.m_resources.m_movies.DrawSprite(0, animation, 0, float(x), float(y), 1, region.alpha)) { return false; }
                const auto text = std::to_string(count);
                return hud.m_resources.m_movies.Text(text, x - int(hud.m_resources.m_movies.TextWidth(text, 0)) / 2,
                    y - int(hud.m_resources.m_movies.TextHeight(0)) / 2, 0, 1, 0, region.alpha);
            }
            if (region.index == 2) {
                const auto &sprite = powerup->data.sprite;
                auto &renderer = *hud.m_resources.m_powerupRenderers.at(sprite.packHash);
                ZMovieRegion bounds;
                const unsigned animation = powerup->data.field29;
                if (!renderer.SpriteBounds(sprite.archetype, animation, bounds)) { return false; }
                return renderer.DrawSprite(sprite.archetype, animation, 0,
                    region.x - bounds.x + int(region.width - bounds.width) / 2,
                    region.y - bounds.y + int(region.height - bounds.height) / 2, 1, region.alpha);
            }
            return true;
        }
        CInputPad &hud;
        const ZPowerupEntry *powerup;
        unsigned count;
        const ZInputPadState &state;
    } callback(*this, powerup, count, state);
    return m_resources.m_movies.Draw(movie, time, x, y, 1024, 768, 0, alpha, &callback);
}

bool CInputPad::DrawControls(const ZInputPadState &state) {
    // CBrother native 12 -> CInputPad::Hide. Restore owns the visible progress
    // until its sequence completes, even while the actor is still reviving.
    if (m_animationPowerup == nullptr) {
        m_controlsHidden = state.inputHidden;
        // A powerup owns the closing selector until native 13 restores Base.
        if (state.shopOpen) { m_selectorWasOpen = true; }
        else if (!m_selector.GetPowerup().IsPresentationActive()) { m_selectorWasOpen = false; }
        if (m_controlsHidden) { m_baseAlpha = 0; return true; }
        m_baseAlpha = 1;
    }
    const float health = state.health / std::max(1.0f, state.maximumHealth);
    const float experience = float(state.experience) / std::max<std::uint64_t>(1, state.experienceDelta);
    if (!m_metersBound) {
        m_metersBound = true;
        m_meters[0].SnapValue(health);
        m_meters[1].SnapValue(experience);
    } else {
        m_meters[0].SetValue(health);
        m_meters[1].SetValue(experience);
    }
    const unsigned base = m_resources.m_movies.Ordinal("GLU_MOVIE_HUD_PAD_IPAD");
    class BaseCallback : public ZMovieRegionCallback {
    public:
        BaseCallback(CInputPad &owner, const ZInputPadState &snapshot) : hud(owner), state(snapshot) {}
        bool DrawMovieRegion(const ZMovieRegion &region) override {
            if (region.index < 2) { return hud.DrawMeter(region, region.index); }
            if (region.index == 2 || region.index == 3) {
                unsigned animation = 27;
                // CInputPad::Base::SetState :87545: selector state 7 keeps
                // the red button down (28); returning state 8 restores 27.
                if (state.shopOpen || hud.m_selectorWasOpen) { animation = 28; }
                if (region.index == 3) {
                    animation = 35;
                    // Base::UpdateInput :88480 uses 36 for a held touch in
                    // region 3. Actions still fire once on the down edge.
                    if ((state.swapKeyDown || (hud.m_previousDown && region.Contains(hud.m_mouseX, hud.m_mouseY))) &&
                        !state.shopOpen && !state.paused && !state.dead && !state.cleared) {
                        animation = 36;
                    }
                }
                return hud.m_resources.m_movies.DrawSprite(1, animation, hud.m_controlTime, region.x, region.y + region.height, 1, region.alpha);
            }
            return true;
        }
        CInputPad &hud;
        const ZInputPadState &state;
    } baseCallback(*this, state);
    if (!m_resources.m_movies.Draw(base, 0, 512, 384, 1024, 768, 0, m_baseAlpha, &baseCallback)) { return false; }
    ZMovieRegion stickBounds;
    if (!m_resources.m_movies.SpriteBounds(1, 6, stickBounds)) { return false; }
    for (unsigned slot = 0; slot < 2; ++slot) {
        ZMovieRegion area;
        if (!m_resources.m_movies.Region(base, 4 + slot, 0, area)) { return false; }
        const float x = area.x + int(area.width) / 2, y = area.y + int(area.height) / 2;
        float directionX = state.moveX, directionY = state.moveY;
        const char *name = "GLU_MOVIE_POWERUP_BUTTON";
        if (slot == 1) { directionX = state.aimX; directionY = state.aimY; name = "GLU_MOVIE_GRENADE_BUTTON"; }
        // Windows keys/mouse supply the original normalized control vector.
        // Floating sticks remain hidden until their corresponding input is active.
        if (!state.dockedSticks && directionX == 0 && directionY == 0) { continue; }
        if (!m_resources.m_movies.DrawSprite(1, 6 + slot, m_controlTime, x, y, 1, m_baseAlpha)) { return false; }
        const unsigned movie = m_resources.m_movies.Ordinal(name);
        unsigned start = 0, end = 0;
        // CInputPad::Load :88166 starts chapter 0 and loops chapter 1.
        // Chapter 2 is the hide/flash sequence, not a perpetual idle effect.
        if (!m_resources.m_movies.GetMovie(movie)->GetChapterRange(1, start, end) ||
            !DrawPowerup(state, slot, x, y, movie, start + m_controlTime % (end - start + 1), m_baseAlpha)) { return false; }
        // Bind radius = sprite width * .42; ControlStick::Draw displacement *.35.
        const float travel = stickBounds.width * 0.42f * 0.35f;
        if (!m_resources.m_movies.DrawSprite(1, 8, m_controlTime, float(int(x + directionX * travel)), float(int(y + directionY * travel)), 1, m_baseAlpha)) { return false; }
    }
    // SetAnimation :86309 restores components sequentially, not concurrently.
    if (m_animationPowerup != nullptr && m_restoreBase && m_restorePeripheralPending) { return true; }
    class PeripheralCallback : public ZMovieRegionCallback {
    public:
        PeripheralCallback(CInputPad &owner, ZMovieRenderer &renderer, const ZInputPadState &snapshot) : hud(owner), movies(renderer), state(snapshot) {}
        bool DrawMovieRegion(const ZMovieRegion &region) override {
            if (region.index == 3 && state.deathmatch) {
                float y = region.y;
                for (const auto &message : hud.m_matchMessages) {
                    if (!movies.Text(message.text, region.x + region.width + movies.TextWidth(" "), y, 0, 1, 0, region.alpha)) { return false; }
                    y += movies.TextHeight();
                }
                return true;
            }
            if (region.index == 0 && state.deathmatch) {
                // PeripheralHUD::DrawMPMatchScore :86733 uses two right-aligned rows.
                const std::string score = std::to_string(state.matchScore[0]) + " " + movies.NamedString("IDS_HUD_KILLS");
                const std::string opponent = std::to_string(state.matchScore[1]) + " " + movies.NamedString("IDS_HUD_DEATHS");
                return movies.Text(score, region.x + region.width - movies.TextWidth(score), region.y, 0, 1, 0, region.alpha) &&
                    movies.Text(opponent, region.x + region.width - movies.TextWidth(opponent), region.y + movies.TextHeight(), 0, 1, 0, region.alpha);
            }
            if (region.index == 0) {
                if (!state.horde) {
                    if (!movies.DrawSprite(1, 39, 0, region.x, region.y, 1, region.alpha)) { return false; }
                    // Native UTF-32 format at VA0x3C3DF8 is "%03d%%".
                    char number[32];
                    std::snprintf(number, sizeof(number), "%03d%%", state.xplodiumMultiplier);
                    const std::string text = number;
                    return movies.Text(text, region.x + region.width - movies.TextWidth(text, 0),
                        region.y + region.height - movies.TextHeight(0), 0, 1, 0, region.alpha);
                }
                // OnScoreChange :87887 appends localized suffixes to each number.
                const std::string score = std::to_string(state.score) + movies.NamedString("IDS_HUD_SCORE");
                const std::string kills = std::to_string(state.kills) + movies.NamedString("IDS_HUD_KILLS");
                return movies.Text(score, region.x + region.width - movies.TextWidth(score), region.y, 0, 1, 0, region.alpha) &&
                    movies.Text(kills, region.x + region.width - movies.TextWidth(kills), region.y + movies.TextHeight(0), 0, 1, 0, region.alpha);
            }
            if (region.index == 1 && state.horde) {
                const std::string label = movies.NamedString("IDS_HUD_KILL_STREAK");
                const std::string value = std::to_string(state.killStreak);
                float x = region.x + region.width / 2 - int(movies.TextWidth(value, 12)) / 2;
                if (value.size() > 3) { x = region.x + region.width - movies.TextWidth(value, 12); }
                return movies.Text(label, region.x + region.width / 2 - int(movies.TextWidth(label)) / 2,
                    region.y + movies.TextHeight(12), 0, 1, 0, region.alpha) &&
                    movies.Text(value, x, region.y, 12, 1, 0, region.alpha);
            }
            return true;
        }
        ZMovieRenderer &movies;
        const ZInputPadState &state;
        CInputPad &hud;
    } peripheralCallback(*this, m_resources.m_movies, state);
    const unsigned peripheral = m_resources.m_movies.Ordinal("GLU_MOVIE_HUD_PAUSE");
    unsigned start = 0, end = 0;
    unsigned chapter = 3;
    if (HasChallenges()) { chapter = 5; }
    if (!m_resources.m_movies.GetMovie(peripheral)->GetChapterRange(chapter, start, end)) { return false; }
    if (HasChallenges()) { start = end; }
    if (m_animationPowerup != nullptr && m_restorePeripheralPending) { start = m_restorePeripheral.GetTime(); }
    if (!m_resources.m_movies.Draw(peripheral, start, 512, 384, 1024, 768, 0, 1, &peripheralCallback)) { return false; }
    if (HasChallenges()) {
        ZMovieRegion area;
        if (!m_resources.m_movies.Region(peripheral, 4, start, area)) { return false; }
        // PeripheralHUD::Bind :88876 uses core sprite0 animation170;
        // Update :89028 positions its origin at region4's right and y + y/8.
        if (!m_resources.m_movies.DrawSprite(0, 170, m_controlTime, area.x + area.width,
            area.y + (static_cast<int>(area.y) >> 3))) { return false; }
    }
    return true;
}
