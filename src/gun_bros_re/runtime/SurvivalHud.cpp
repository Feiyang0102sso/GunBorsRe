/** @file SurvivalHud.cpp
 * @brief CInputPad::Base::Bind (:88320) binds meters to regions 0/1 and guns to 2/3.
 */
#define NOMINMAX
#include "runtime/SurvivalHud.h"
#include "runtime/HostSettings.h"
#include "runtime/PowerupCatalog.h"
#include "engine/platform/CWindow.h"
#include "gun_bros/CResTOCManager.h"
#include "engine/CPNG.h"
#include <algorithm>
#include <cstdio>
#include <sstream>

bool SurvivalHud::Init(CResTOCManager &toc, PackTables &tables) {
    m_tables = &tables;
    CResPackTOC &core = *toc.GetPack(toc.GetCorePackIndex());
    if (!m_movies.Init(core, core) || !LoadStoreCatalog(toc, tables, m_store)) { return false; }
    if (!LoadPowerupCatalog(toc, tables, m_powerups)) { return false; }
    for (const PowerupEntry &entry : m_powerups) {
        const unsigned hash = entry.data.sprite.packHash;
        if (m_powerupRenderers.count(hash) != 0) { continue; }
        auto renderer = std::make_unique<MovieRenderer>();
        CResPackTOC *pack = toc.GetPack(toc.GetPackIndexFromHash(hash));
        if (pack == nullptr || !renderer->Init(*pack, core)) { return false; }
        m_powerupRenderers[hash] = std::move(renderer);
    }
    // The retail carousel starts with turret/grenades, followed by buffs.
    constexpr unsigned order[] = {19, 13, 15, 14, 5, 6, 16, 17, 18, 12, 0, 10, 11, 1, 8, 9};
    for (unsigned item : order) {
        for (unsigned index = 0; index < m_store.size(); ++index) {
            const CStoreItem &entry = m_store[index].data;
            if (entry.objects.empty() || entry.type >= 14 || (entry.commonPrice == 0 && entry.rarePrice == 0)) { continue; }
            const GameObjectTypeRef &ref = entry.objects.front();
            if (ref.type != 17 || ref.object.localIndex != item || !IsPlayablePowerup(ref.object)) { continue; }
            bool sameItem = true;
            for (const GameObjectTypeRef &object : entry.objects) {
                if (object.type != 17 || object.object.packHash != ref.object.packHash || object.object.localIndex != item) { sameItem = false; }
            }
            if (!sameItem) { continue; }
            m_shopEntries.push_back(index);
            break;
        }
    }
    // First-use font and atlas uploads belong to loading, not the first shot
    // or a wave-clear frame. No gameplay or notification state is advanced.
    constexpr unsigned movies[] = {1, 3, 7, 34, 87, 116, 130, 131, 134, 135};
    for (unsigned movie : movies) {
        if (!m_movies.Draw(movie, 600)) { return false; }
    }
    constexpr unsigned sprites[] = {6, 7, 8, 27, 35, 39, 87};
    for (unsigned sprite : sprites) { m_movies.SpriteDuration(1, sprite); }
    for (unsigned font : {0u, 1u, 5u, 6u, 7u}) { m_movies.TextWidth("0123456789", font); }
    // Composite sticks and badges have their own lazy-expanded frame caches.
    // Draw a neutral snapshot while the loading screen still owns presentation.
    DrawControls(SurvivalHudState{});
    CountBadge(10, 0, 0, 46);
    return true;
}

void SurvivalHud::Icon(unsigned type, const GameObjectRef &object, const MovieRegion &region) {
    if (object.IsNull()) { return; }
    if (type == 17 && region.width < 60) {
        for (const PowerupEntry &entry : m_powerups) {
            if (entry.resource.packHash != object.packHash || entry.resource.localIndex != object.localIndex) { continue; }
            const CGameSpriteGluRef &sprite = entry.data.sprite;
            m_powerupRenderers[sprite.packHash]->DrawSpriteFitted(sprite.archetype, sprite.animation, 0,
                region.x, region.y, region.width, region.height);
            return;
        }
    }
    for (const StoreEntry &entry : m_store) {
        // Bundle thumbnails depict several products; a single equipped icon
        // must resolve the matching single-product offer instead.
        bool singleProduct = true;
        for (const GameObjectTypeRef &ref : entry.data.objects) {
            if (ref.type != type || ref.object.packHash != object.packHash || ref.object.localIndex != object.localIndex) { singleProduct = false; break; }
        }
        if (!singleProduct) { continue; }
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
    if (state.shopOpen) {
        if (state.itemChoice) {
            return {{{0, 0, 256, 503, 152, 40}, SurvivalHudAction::EquipLeft, "EQUIP"},
                {{0, 0, 462, 485, 100, 85}, SurvivalHudAction::UseNow, "USE NOW"},
                {{0, 0, 615, 503, 152, 40}, SurvivalHudAction::EquipRight, "EQUIP"},
                {{0, 0, 434, 365, 156, 36}, SurvivalHudAction::CancelItem, "CANCEL"}};
        }
        return {{{0, 0, 434, 365, 156, 36}, SurvivalHudAction::CloseShop, "RESUME"}};
    }
    if (!state.dialog.empty() && state.tutorialStep < 0) {
        return {{{0, 0, 716, 572, 256, 56}, SurvivalHudAction::Continue, "CONTINUE"}};
    }
    if (state.paused && !state.dead && !state.cleared) {
        const Button entries[] = {
            {{}, SurvivalHudAction::Resume, "RESUME"}, {{}, SurvivalHudAction::Exit, "SURRENDER"},
            {{}, SurvivalHudAction::Sound, "SFX"}, {{}, SurvivalHudAction::Music, "MUSIC"},
            {{}, SurvivalHudAction::Retry, "RESTART"}};
        std::vector<Button> result;
        for (unsigned row = 0; row < 4 && row + m_pauseOffset < 5; ++row) {
            Button button = entries[row + m_pauseOffset];
            float x = 135;
            if (row == 1) { x = 152; }
            if (row == 3) { x = 78; }
            button.rect = {0, 0, x, 326 + row * 85.0f, 220, 67};
            result.push_back(button);
        }
        return result;
    }
    if (state.dead || state.cleared) {
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
        {{0, 0, 270, 685, 82, 83}, SurvivalHudAction::OpenShop, ""},
        {{0, 0, 674, 685, 82, 83}, SurvivalHudAction::SwapWeapon, ""},
        {{0, 0, 22, 464, 205, 86}, SurvivalHudAction::UseLeft, ""},
        {{0, 0, 797, 464, 205, 86}, SurvivalHudAction::UseItem, ""}
    };
}

SurvivalHudAction SurvivalHud::Pointer(const SurvivalHudState &state, float x, float y, bool down) {
    m_mouseX = x;
    m_mouseY = y;
    const bool clicked = down && !m_previousDown;
    m_previousDown = down;
    if (!clicked) { return SurvivalHudAction::None; }
    if (state.shopOpen && !state.itemChoice) {
        for (unsigned column = 0; column < 6 && column + m_shopOffset < m_shopEntries.size(); ++column) {
            const float left = 104 + column * 140.0f;
            if (x < left || x >= left + 104) { continue; }
            if (y >= 190 && y < 345) {
                m_selectedItem = static_cast<int>(m_shopEntries[column + m_shopOffset]);
                if (y >= 311) { return SurvivalHudAction::BuyItem; }
                return SurvivalHudAction::SelectItem;
            }
        }
    }
    for (const Button &button : Buttons(state)) {
        if (button.rect.Contains(x, y)) { return button.action; }
    }
    return SurvivalHudAction::None;
}

bool SurvivalHud::CapturesPointer(const SurvivalHudState &state, float x, float y) const {
    if (state.shopOpen || state.paused || state.dead || state.cleared) { return true; }
    if (!state.dialog.empty() && state.tutorialStep < 0) { return true; }
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
        m_movies.Rectangle(region.x, region.y, region.width, region.height, 0.22f, 0.24f, 0.24f);
        if (region.index == 0) { m_movies.Rectangle(region.x + 1, region.y + 1, (region.width - 2) * std::clamp(fraction, 0.0f, 1.0f), region.height - 2, 0.06f, 0.72f, 0.03f); }
        else { m_movies.Rectangle(region.x + 1, region.y + 1, (region.width - 2) * std::clamp(fraction, 0.0f, 1.0f), region.height - 2, 0.04f, 0.5f, 1); }
    }
    DrawControls(state);
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
    std::string reward = "XPLODIUM " + std::to_string(state.xplodium);
    if (state.horde) { reward = "POINTS " + std::to_string(state.score); }
    if (state.horde) { Centre(waveSubtitle + "  " + reward, 18, 0, 0.65f); }
    if (GameHostSettings().debugMode) {
        m_movies.Rectangle(80, 0, 784, 71, 0, 0, 0, 0.65f);
        Centre(waveTitle + "  " + waveSubtitle, 8, 0, 0.68f);
        Centre("ENEMIES " + std::to_string(state.enemies) + "  KILLS " + std::to_string(state.kills) + "  " + reward, 35, 0, 0.68f);
        m_movies.Text(state.buffs, 16, 126, 0, 0.62f, 950);
        char debug[240];
        std::snprintf(debug, sizeof(debug), "FPS %.1f / %.2f MS / XY %.1f %.1f / DAMAGE %.1f / BONUS %llu / XP %llu",
            1000.0f / std::max(0.1f, state.frameMs), state.frameMs, state.playerX, state.playerY, state.damageDealt,
            state.perfectBonus, state.experience);
        m_movies.Rectangle(8, 93, 1008, 25, 0, 0, 0, 0.75f);
        m_movies.Text(debug, 12, 98, 0, 0.62f);
    }
    if (state.transitioning && !state.paused && !state.dead) {
        if (m_notices.empty()) { Centre("GET READY", 315, 5, 1.2f); }
    }
    DrawNotice();
    if (state.shopOpen) { DrawShop(state); }
    else if (state.paused && !state.dead && !state.cleared) {
        m_movies.Rectangle(0, 0, 1024, 768, 0, 0, 0, 0.48f);
        m_movies.Draw(10, 600);
        m_movies.Draw(11, 1600, 697, 0);
        m_movies.Draw(9, 600, -18, 449);
        m_movies.Text(std::to_string(state.coins), 104, 11, 0, 1.0f);
        m_movies.Text(std::to_string(state.warbucks), 392, 11, 0, 1.0f);
        char level[8];
        std::snprintf(level, sizeof(level), "%03u", std::min(200u, state.level));
        for (unsigned digit = 0; digit < 3; ++digit) {
            m_movies.Text(std::string(1, level[digit]), 939 + digit * 28.0f, 11, 7, 1);
        }
        std::string title = m_movies.NamedString("IDS_RESUME_TITLE");
        std::string description = m_movies.NamedString("IDS_RESUME_BODY");
        for (const Button &button : Buttons(state)) {
            if (!button.rect.Contains(m_mouseX, m_mouseY)) { continue; }
            if (button.action == SurvivalHudAction::Exit) { title = m_movies.NamedString("IDS_EXIT_TITLE"); description = m_movies.NamedString("IDS_EXIT_BODY"); }
            if (button.action == SurvivalHudAction::Sound) { title = "SOUND EFFECTS"; description = m_movies.NamedString("IDS_SOUND_BODY"); }
            if (button.action == SurvivalHudAction::Music) { title = "MUSIC"; description = m_movies.NamedString("IDS_MUSIC_BODY"); }
            if (button.action == SurvivalHudAction::Retry) { title = "RESTART"; description = "START THIS WAVE AGAIN."; }
        }
        // ^f1 introduces the body heading; ^f0 returns to normal body text.
        // Decode the original markup instead of printing control tokens.
        while (!description.empty() && (description.front() == '\n' || description.front() == '\r')) { description.erase(0, 1); }
        if (description.rfind("^f1", 0) == 0) {
            const auto headingEnd = description.find('\n');
            if (headingEnd != std::string::npos) {
                title = description.substr(3, headingEnd - 3);
                description.erase(0, headingEnd + 1);
            }
        }
        std::size_t marker = description.find("^f");
        while (marker != std::string::npos && marker + 2 < description.size()) {
            description.erase(marker, 3);
            marker = description.find("^f");
        }
        while (!description.empty() && (description.front() == '\n' || description.front() == '\r')) { description.erase(0, 1); }
        m_movies.Text(title, 442, 160, 0, 0.92f);
        // MDS_PAUSE_ROOT stores paragraph breaks in the original body strings.
        std::istringstream paragraphs(description);
        std::string paragraph;
        float textY = 205;
        while (std::getline(paragraphs, paragraph)) {
            std::istringstream words(paragraph);
            std::string word, line;
            while (words >> word) {
                std::string next = word;
                if (!line.empty()) { next = line + " " + word; }
                if (!line.empty() && m_movies.TextWidth(next, 0, 0.8f) > 550) {
                    m_movies.Text(line, 442, textY, 0, 0.8f);
                    textY += 25;
                    line = word;
                } else { line = next; }
            }
            m_movies.Text(line, 442, textY, 0, 0.8f);
            textY += 25;
        }
    }
    else if (state.dead || state.cleared) {
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
        // CDialogPopup draws the original radio portrait beside its text region.
        m_movies.Rectangle(50, 128, 550, 94, 0.02f, 0.14f, 0.21f, 0.72f);
        m_movies.Rectangle(50, 128, 550, 3, 0.48f, 0.83f, 1, 0.9f);
        m_movies.DrawSpriteFitted(1, 87, 0, 50, 128, 280, 94);
        std::istringstream words(state.dialog);
        std::string word, line;
        float y = 152;
        while (words >> word) {
            if (m_movies.TextWidth(line + " " + word, 0, 0.8f) > 435) {
                m_movies.Text(line, 150, y, 0, 0.8f);
                line.clear();
                y += 27;
            }
            if (!line.empty()) { line += ' '; }
            line += word;
        }
        m_movies.Text(line, 150, y, 0, 0.8f);
        if (state.tutorialStep == 0) { m_movies.Text("WASD + MOUSE", 150, 204, 1, 0.65f); }
        if (state.tutorialStep == 2) { m_movies.Text("Q", 708, 648, 0, 1.1f); }
        if (state.tutorialStep == 5) { m_movies.Text("G", 936, 449, 0, 1.1f); }
    }
    for (const Button &button : Buttons(state)) {
        const MovieRegion &rect = button.rect;
        if (button.label[0] == 0) { continue; }
        const bool hovered = rect.Contains(m_mouseX, m_mouseY);
        if (state.paused && !state.shopOpen && !state.dead && !state.cleared) {
            m_movies.Rectangle(rect.x, rect.y, rect.width, rect.height, 0, 0, 0, 0.95f);
            unsigned buttonTime = 200;
            if (hovered) { buttonTime = 800; }
            m_movies.DrawFitted(73, buttonTime, rect.x, rect.y, rect.width, rect.height);
        } else if (button.action != SurvivalHudAction::UseNow) {
            m_movies.ButtonBackground(rect.x, rect.y, rect.width, rect.height, false, hovered);
        }
        bool weaponButton = false;
        if (button.action == SurvivalHudAction::Weapon1 || button.action == SurvivalHudAction::Weapon2) {
            unsigned slot = 0;
            if (button.action == SurvivalHudAction::Weapon2) { slot = 1; }
            const MovieRegion icon{0, 0, rect.x + 14, rect.y + 4, rect.width - 28, rect.height - 16};
            Icon(6, state.guns[slot], icon);
            m_movies.Text(button.label, rect.x + 7, rect.y + 5, 0, 0.6f);
            weaponButton = true;
        }
        std::string label = button.label;
        if (button.action == SurvivalHudAction::Sound) {
            label += " OFF";
            if (state.soundEnabled) { label = "SFX ON"; }
        }
        if (button.action == SurvivalHudAction::Music) {
            label += " OFF";
            if (state.musicEnabled) { label = "MUSIC ON"; }
        }
        float scale = 0.8f;
        const float textWidth = m_movies.TextWidth(label, 0, scale);
        if (!weaponButton) { m_movies.Text(label, rect.x + (rect.width - textWidth) / 2, rect.y + (rect.height - 23 * scale) / 2, 0, scale); }
        if ((button.action == SurvivalHudAction::Weapon1 && state.weaponSlot == 0) ||
            (button.action == SurvivalHudAction::Weapon2 && state.weaponSlot == 1)) {
            m_movies.Rectangle(rect.x + 12, rect.y + rect.height - 9, rect.width - 24, 3, 1, 0.65f, 0);
        }
    }
    return m_movies.Failures() == 0;
}

const StoreEntry *SurvivalHud::SelectedItem() const {
    if (m_selectedItem < 0 || m_selectedItem >= static_cast<int>(m_store.size())) { return nullptr; }
    return &m_store[m_selectedItem];
}

void SurvivalHud::Scroll(const SurvivalHudState &state, float amount) {
    if (amount == 0) { return; }
    if (state.shopOpen && !state.itemChoice) {
        if (amount > 0 && m_shopOffset > 0) { --m_shopOffset; }
        if (amount < 0 && m_shopOffset + 6 < m_shopEntries.size()) { ++m_shopOffset; }
    } else if (state.paused) {
        if (amount > 0) { m_pauseOffset = 0; }
        else { m_pauseOffset = 1; }
    }
}

void SurvivalHud::CountBadge(unsigned count, float x, float y, float diameter) {
    // Utility::DrawIconBadge :104839 selects the wider badge above nine.
    unsigned animation = 87;
    if (count > 9) { animation = 88; }
    m_movies.DrawSpriteFitted(0, animation, 0, x, y, diameter, diameter);
    const std::string text = std::to_string(count);
    const float scale = std::min(1.1f, diameter / 47);
    m_movies.Text(text, x + (diameter - m_movies.TextWidth(text, 0, scale)) / 2, y + diameter * 0.28f, 0, scale);
}

void SurvivalHud::DrawControls(const SurvivalHudState &state) {
    // CInputPad::DrawSticks :90823 uses these original composite animations.
    m_movies.DrawSpriteFitted(1, 6, 600, 22, 542, 205, 205);
    m_movies.DrawSpriteFitted(1, 7, 600, 797, 542, 205, 205);
    m_movies.DrawSpriteFitted(1, 8, 0, 94 + state.moveX * 37, 614 + state.moveY * 37, 61, 61);
    m_movies.DrawSpriteFitted(1, 8, 0, 869 + state.aimX * 37, 614 + state.aimY * 37, 61, 61);
    m_movies.DrawFitted(135, 600, 22, 464, 205, 86);
    m_movies.DrawFitted(130, 600, 797, 464, 205, 86);
    Icon(17, state.leftPowerup, {0, 0, 72, 480, 47, 47});
    Icon(17, state.rightPowerup, {0, 0, 907, 480, 47, 47});
    CountBadge(state.leftCount, 128, 480, 46);
    CountBadge(state.rightCount, 851, 480, 46);
    m_movies.DrawSpriteFitted(1, 27, 0, 270, 685, 82, 90);
    m_movies.DrawSpriteFitted(1, 35, 0, 674, 685, 82, 90);
    m_movies.DrawSpriteFitted(1, 39, 0, 888, 12, 78, 62);
    m_movies.Text(std::to_string(state.xplodiumMultiplier) + "%", 953, 43, 0, 0.78f, 70);
}

void SurvivalHud::DrawShop(const SurvivalHudState &state) {
    m_movies.Draw(131, 600);
    for (unsigned column = 0; column < 6 && column + m_shopOffset < m_shopEntries.size(); ++column) {
        const StoreEntry &entry = m_store[m_shopEntries[column + m_shopOffset]];
        const GameObjectRef &ref = entry.data.objects.front().object;
        const float x = 104 + column * 140.0f;
        m_movies.Text(entry.name, x - 4, 162, 5, 0.42f, 122);
        Icon(17, ref, {0, 0, x, 190, 104, 104});
        unsigned count = 0;
        for (const PowerupInventoryEntry &owned : state.inventory) {
            if (owned.resource.packHash == ref.packHash && owned.resource.localIndex == ref.localIndex) { count = owned.count; break; }
        }
        if (count > 0) { CountBadge(count, x + 70, 176, 46); }
        std::string price = std::to_string(entry.data.commonPrice) + " C";
        if (entry.data.commonPrice == 0) { price = std::to_string(entry.data.rarePrice) + " W"; }
        m_movies.Text(price, x + 20, 292, 0, 0.77f);
        m_movies.ButtonBackground(x, 314, 104, 31, true, false);
        m_movies.Text("BUY", x + 22, 315, 5, 0.70f);
    }
    m_movies.Text(std::to_string(state.coins) + " C", 165, 374, 0, 0.8f, 180);
    m_movies.Text(std::to_string(state.warbucks) + " W", 620, 374, 0, 0.8f, 250);
    if (!state.shopMessage.empty()) { Centre(state.shopMessage, 422, 0, 0.72f); }
    if (state.itemChoice) {
        m_movies.Rectangle(0, 153, 1024, 206, 0.02f, 0.1f, 0.13f, 0.55f);
        // The lower equip/use strip is the original NEW_COPY movie.
        m_movies.Draw(134, 600);
        const StoreEntry *selected = SelectedItem();
        if (selected != nullptr) {
            for (unsigned column = 0; column < 6 && column + m_shopOffset < m_shopEntries.size(); ++column) {
                if (&m_store[m_shopEntries[column + m_shopOffset]] != selected) { continue; }
                Icon(17, selected->data.objects.front().object, {0, 0, 104 + column * 140.0f, 190, 104, 104});
            }
        }
    }
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
                m_notices.push_back({87, 0, "PERFECT WAVE!", "+10%"});
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
    if (hud.Pointer(state, 200, 350, true) != SurvivalHudAction::Resume) { ++failures; }
    hud.Pointer(state, 200, 350, false);
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
    state.indicators.clear();
    state.shopOpen = true;
    state.coins = 5000;
    state.warbucks = 500;
    hud.Pointer(state, 250, 328, false);
    if (hud.Pointer(state, 250, 328, true) != SurvivalHudAction::BuyItem || hud.SelectedItem() == nullptr) { ++failures; }
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    CProfileManager buyer;
    buyer.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    buyer.coins = state.coins;
    buyer.warbucks = state.warbucks;
    const StoreEntry *grenades = hud.SelectedItem();
    if (grenades == nullptr) { return 1; }
    const GameObjectRef grenade = grenades->data.objects.front().object;
    const unsigned before = buyer.GetPowerupCount(grenade);
    if (buyer.AcquireItem(grenades->data, 200) != PurchaseResult::Purchased ||
        buyer.GetPowerupCount(grenade) != before + grenades->data.objects.size()) { ++failures; }
    state.inventory = buyer.powerups;
    state.leftPowerup = grenade;
    state.rightPowerup = grenade;
    state.leftCount = buyer.GetPowerupCount(grenade);
    state.rightCount = state.leftCount;
    state.coins = buyer.coins;
    state.warbucks = buyer.warbucks;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!hud.Draw(state) || !window.SaveFrame("out/hud-check-shop.png")) { ++failures; }
    state.itemChoice = true;
    hud.Pointer(state, 700, 524, false);
    if (hud.Pointer(state, 700, 524, true) != SurvivalHudAction::EquipRight) { ++failures; }
    hud.Pointer(state, 320, 524, false);
    if (hud.Pointer(state, 320, 524, true) != SurvivalHudAction::EquipLeft) { ++failures; }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!hud.Draw(state) || !window.SaveFrame("out/hud-check-equip.png")) { ++failures; }
    state.shopOpen = false;
    state.itemChoice = false;
    hud.Pointer(state, 710, 720, false);
    if (hud.Pointer(state, 710, 720, true) != SurvivalHudAction::SwapWeapon) { ++failures; }
    state.paused = true;
    hud.Scroll(state, -1);
    hud.Pointer(state, 120, 610, false);
    if (hud.Pointer(state, 120, 610, true) != SurvivalHudAction::Retry) { ++failures; }
    std::printf("[hud-check] retail-controls=1 shop-buy=%zu equip-both=1 pause-scroll=1\n", grenades->data.objects.size());
    state.paused = false;
    state.dialog = "Touch and drag to move and shoot.";
    state.tutorialStep = 0;
    if (hud.CapturesPointer(state, 512, 360)) { ++failures; }
    if (!hud.CapturesPointer(state, 710, 720)) { ++failures; }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!hud.Draw(state) || !window.SaveFrame("out/hud-check-tutorial.png")) { ++failures; }
    std::printf("[hud-check] states=9 indicator-types=7 original-notice-queue=1 boss-replaces-queue=1 pause-restart=1 failures=%u\n", failures);
    return failures != 0;
}
