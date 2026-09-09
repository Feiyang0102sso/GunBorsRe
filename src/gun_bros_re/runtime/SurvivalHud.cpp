/** @file SurvivalHud.cpp
 * @brief CInputPad::Base::Bind (:88320) binds meters to regions 0/1 and guns to 2/3.
 */
#define NOMINMAX
#include "runtime/SurvivalHud.h"
#include "runtime/OriginalMenuData.h"
#include "runtime/OriginalTextLayout.h"
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
    // CStoreAggregator::InitFilteredList :159210 and SortFilteredList :155433.
    // The native selector consumes that list, not the historical ordinal order above.
    std::vector<std::pair<int, unsigned>> selectorOrder;
    for (unsigned index = 0; index < m_store.size(); ++index) {
        const auto &item = m_store[index].data;
        if (item.type < 10 || item.type > 13 || item.displayOrder < 0 || item.value242 == 1 ||
            item.objects.empty() || item.objects.front().type != 17) { continue; }
        selectorOrder.push_back({item.displayOrder, index});
    }
    std::sort(selectorOrder.begin(), selectorOrder.end());
    for (const auto &item : selectorOrder) { m_selectorEntries.push_back(item.second); }
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
    if (state.originalUi) { return OriginalControlButtons(state); }
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
    if (state.originalUi && state.shopOpen) {
        if (m_selectorPromptRequested || m_selectorPrompt.IsActive()) {
            if (m_selectorPrompt.IsReady()) {
                for (const auto &hit : m_selectorPromptHits) {
                    if (!hit.first.Contains(x, y)) { continue; }
                    if (hit.second == 45) { m_selectorPrompt.Hide(); }
                    if (hit.second == 71) {
                        // The Windows selector has no live NGS checkout. Preserve
                        // the original offline response rather than invent a payment.
                        m_selectorPrompt = CMenuPopupPrompt{};
                        m_selectorPromptTable = "MDS_STORE_PROMPT_OFFLINE";
                        m_selectorPromptBody.clear();
                        m_selectorPromptRequested = true;
                        m_selectorPromptFunds = false;
                    }
                    break;
                }
            }
            return SurvivalHudAction::None;
        }
        for (const auto &hit : m_selectorHits) {
            if (!hit.area.Contains(x, y)) { continue; }
            if (hit.storeIndex >= 0) { m_selectedItem = hit.storeIndex; }
            return hit.action;
        }
        return SurvivalHudAction::None;
    }
    if (state.originalUi && state.paused && !state.shopOpen && !state.dead && !state.cleared) {
        return OriginalPausePointer(state, x, y);
    }
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
    if (state.originalUi) { return false; }
    return y >= 682;
}

void SurvivalHud::Centre(const std::string &text, float y, unsigned font, float scale) {
    const float width = m_movies.TextWidth(text, font, scale);
    m_movies.Text(text, 512 - width * 0.5f, y, font, scale);
}

bool SurvivalHud::Draw(const SurvivalHudState &state) {
    if (!state.shopOpen) { m_selectorBound = false; m_selectorHits.clear(); }
    if (!state.paused && m_pauseBound) { m_pauseHelp = false; ResetPauseList(); }
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
    if (state.originalUi) {
        if (!DrawOriginalControls(state)) { return false; }
    } else {
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
    }
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
    if (state.horde && !state.originalUi) { Centre(waveSubtitle + "  " + reward, 18, 0, 0.65f); }
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
    if (!state.originalUi && state.transitioning && !state.paused && !state.dead) {
        if (m_notices.empty()) { Centre("GET READY", 315, 5, 1.2f); }
    }
    DrawNotice(state.originalUi);
    if (state.shopOpen && state.originalUi) { return DrawOriginalSelector(state); }
    if (state.shopOpen) { DrawShop(state); }
    else if (state.originalUi && state.paused && !state.dead && !state.cleared) {
        return DrawOriginalPause(state);
    }
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
    if (state.originalUi && state.shopOpen) {
        if (!state.itemChoice) {
            const float maximum = std::max(0.0f, float(m_selectorEntries.size()) - 3);
            m_selectorTarget = std::clamp(m_selectorTarget - amount, std::min(2.0f, maximum), maximum);
        }
        return;
    }
    if (state.originalUi && state.paused && !state.shopOpen) {
        MovieRegion content;
        const unsigned movie = m_movies.Ordinal("GLU_MOVIE_LIST_MENU_PAUSE");
        unsigned start = 0, end = 0;
        if (!m_movies.GetMovie(movie)->GetChapterRange(1, start, end)) { return; }
        if (m_movies.Region(movie, 8, start, content) && content.Contains(m_mouseX, m_mouseY)) {
            m_pauseBodyPosition = std::max(0.0f, m_pauseBodyPosition - amount);
        } else {
            m_pauseTarget = std::clamp(m_pauseTarget - amount, 0.0f, std::max(0.0f, float(m_pauseItems.size()) - 3));
        }
        return;
    }
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
    m_interstitialCompleted = false;
}

std::string SurvivalHud::OriginalNoticeNumber(const char *name, unsigned number) {
    // OnLevelUp/OnLevelStart/OnWaveClear pass one signed integer to SWPrintF_S.
    // ARM: 0x62A40, 0x63588, 0x62714 and 0x627B0 (LEVEL reward percent).
    std::string text = m_movies.NamedString(name);
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

void SurvivalHud::QueueOriginalNotice(const char *movie, const std::string &title, const std::string &footer, bool releaseLevel) {
    // SetUpOverlay :87596 uses a six-slot ring with one empty slot.
    if (m_notices.size() >= 5) { return; }
    m_notices.push_back({m_movies.Ordinal(movie), 0, title, footer, releaseLevel});
}

bool SurvivalHud::HasInterstitial() const {
    for (const auto &notice : m_notices) { if (notice.releaseLevel) { return true; } }
    return false;
}

bool SurvivalHud::TakeInterstitialCompletion() {
    const bool completed = m_interstitialCompleted;
    m_interstitialCompleted = false;
    return completed;
}

void SurvivalHud::BeginOriginalLevel(unsigned wave, bool horde, bool boss) {
    ResetNotices();
    const char *name = "IDS_HUD_WAVE_START";
    if (horde) { name = "IDS_HUD_HORDE_START"; }
    std::string text = OriginalNoticeNumber(name, wave);
    if (boss) { text = m_movies.NamedString("IDS_HUD_BOSS_WAVE_START"); }
    QueueOriginalNotice("GLU_MOVIE_WAVE_CLEARED", text, "", true);
    std::printf("[hud-overlay] begin wave=%u horde=%d boss=%d text=%s\n", wave, horde, boss, text.c_str());
}

void SurvivalHud::OnOriginalWaveClear(unsigned wave, bool perfect, unsigned rewardPercent, bool boss) {
    // OnWaveClear :89760 discards previous notices before its new sequence.
    m_notices.clear();
    m_interstitialCompleted = false;
    std::string text = OriginalNoticeNumber("IDS_HUD_WAVE_CLEAR", wave);
    // Original ARM 0x626A0..0x6271C loads the boss string but never copies it
    // to the zeroed output buffer. Preserve this observed original defect.
    if (boss) { text.clear(); }
    QueueOriginalNotice("GLU_MOVIE_WAVE_CLEARED", text, "", !perfect);
    if (perfect) {
        QueueOriginalNotice("GLU_MOVIE_PERFECT_WAVE", m_movies.NamedString("IDS_HUD_WAVE_PERFECT"),
            OriginalNoticeNumber("IDS_HUD_WAVE_PERFECT_SUMMARY", rewardPercent), true);
    }
    std::printf("[hud-overlay] clear wave=%u perfect=%d reward=%u boss=%d\n", wave, perfect, rewardPercent, boss);
}

void SurvivalHud::ObserveProgress(const SurvivalHudState &state) {
    if (state.originalUi) {
        // Native wave events come directly from the LEVEL consumer, not from
        // comparing two rendered snapshots. Reloading an account is no level-up.
        if (m_observedProgress && state.level > m_previousLevel) {
            QueueOriginalNotice("GLU_MOVIE_LEVEL_UP", OriginalNoticeNumber("IDS_HUD_LEVEL_REACHED", state.level));
            QueueOriginalNotice("GLU_MOVIE_LEVEL_UP", m_movies.NamedString("IDS_HUD_HEALTH_UP"));
        }
        m_previousLevel = state.level;
        m_observedProgress = true;
        return;
    }
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
    if (deltaMs > 0) {
        m_controlTime += deltaMs;
        for (auto &meter : m_meters) { meter.Update(deltaMs); }
    }
    if (deltaMs <= 0 || m_notices.empty()) { return; }
    Notice &notice = m_notices.front();
    notice.elapsed += static_cast<unsigned>(deltaMs);
    CMovie *movie = m_movies.GetMovie(notice.movie);
    if (movie != nullptr && notice.elapsed >= movie->duration) {
        if (notice.releaseLevel) { m_interstitialCompleted = true; }
        m_notices.erase(m_notices.begin());
    }
}

void SurvivalHud::DrawNotice(bool originalUi) {
    if (m_notices.empty()) { return; }
    const Notice &notice = m_notices.front();
    if (originalUi) {
        // OverlayDraw :86532 uses font 11 at its original size and centers in
        // the Movie region. Drawing as a callback preserves authored layering.
        class OverlayCallback : public IMovieRegionCallback {
        public:
            OverlayCallback(MovieRenderer &renderer, const Notice &current) : movies(renderer), notice(current) {}
            bool DrawMovieRegion(const MovieRegion &area) override {
                if (area.index > 1) { return true; }
                const std::string *text = &notice.title;
                if (area.index == 1) { text = &notice.footer; }
                if (text->empty()) { return true; }
                const float x = area.x + int(area.width) / 2 - int(movies.TextWidth(*text, 11)) / 2;
                const float y = area.y + int(area.height) / 2 - int(movies.TextHeight(11)) / 2;
                return movies.Text(*text, x, y, 11, 1, 0, area.alpha);
            }
            MovieRenderer &movies;
            const Notice &notice;
        } callback(m_movies, notice);
        m_movies.Draw(notice.movie, notice.elapsed, 512, 384, 1024, 768, 0, 1, &callback);
        return;
    }
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
/** MENU_PAUSE 0x4032f0 / MENU_HELP_PAUSE 0x402fd0, CMenuList :140175.
 * Original provider2 skips the multiplayer AI entry for single-player; the
 * XGA iPad surface retains the docked-stick option. Geometry stays in BIG. */
void SurvivalHud::ResetPauseList() {
    m_pauseBound = false;
    m_pauseTime = 0;
    m_pauseBodyTime = 0;
    m_pauseFocus = 0;
    m_pausePosition = 0;
    m_pauseTarget = 0;
    m_pauseBodyPosition = 0;
    m_pauseHits.clear();
}

void SurvivalHud::AdvanceMenu(unsigned deltaMs) {
    m_pauseDelta = deltaMs;
    if (!m_selectorBound) { return; }
    m_selectorTime += deltaMs;
    m_selectorChoiceTime += deltaMs;
    m_selectorPrompt.Update(deltaMs);
    unsigned start = 0, end = 0;
    if (m_movies.GetMovie(m_movies.Ordinal("GLU_MOVIE_POWER_UP_LAYOUT"))->GetChapterRange(1, start, end)) {
        const float step = float(deltaMs) / (end - start + 1);
        if (m_selectorPosition < m_selectorTarget) { m_selectorPosition = std::min(m_selectorPosition + step, m_selectorTarget); }
        else { m_selectorPosition = std::max(m_selectorPosition - step, m_selectorTarget); }
    }
}

bool SurvivalHud::BackFromHelp() {
    if (!m_pauseHelp) { return false; }
    m_pauseHelp = false;
    ResetPauseList();
    return true;
}

std::string SurvivalHud::PauseText(const SurvivalHudState &state, unsigned index, unsigned slot) {
    const char *table = "MDS_PAUSE_ROOT";
    if (m_pauseHelp) { table = "MDS_HELP"; }
    const auto *entry = OriginalMenuData(table, index);
    if (entry == nullptr || slot > 1) { return {}; }
    if (entry->strings[slot][0] != 0) { return m_movies.NamedString(entry->strings[slot]); }
    // CMenuAction::ResolveActionString :95837..95911.
    if (entry->action == 9) {
        if (state.soundEnabled) { return m_movies.NamedString("IDS_SOUND_ON_GAME"); }
        return m_movies.NamedString("IDS_SOUND_OFF_GAME");
    }
    if (entry->action == 10) {
        if (state.musicEnabled) { return m_movies.NamedString("IDS_MUSIC_ON_GAME"); }
        return m_movies.NamedString("IDS_MUSIC_OFF_GAME");
    }
    if (entry->action == 16) {
        if (state.dockedSticks) { return m_movies.NamedString("IDS_DOCKED_STICKS_ON_GAME"); }
        return m_movies.NamedString("IDS_DOCKED_STICKS_OFF_GAME");
    }
    return {};
}

SurvivalHudAction SurvivalHud::OriginalPausePointer(const SurvivalHudState &state, float x, float y) {
    for (const auto &hit : m_pauseHits) {
        if (!hit.first.Contains(x, y)) { continue; }
        if (hit.second == UINT32_MAX) { BackFromHelp(); return SurvivalHudAction::None; }
        const unsigned item = hit.second;
        if (item != m_pauseFocus) {
            m_pauseButtonTimes[m_pauseFocus] = 0;
            m_pauseFocus = item;
            m_pauseBodyTime = 0;
            m_pauseBodyPosition = 0;
            unsigned start = 0, end = 0;
            m_movies.GetMovie(m_movies.Ordinal("GLU_MOVIE_LIST_MENU_BUTTON"))->GetChapterRange(2, start, end);
            m_pauseButtonTimes[item] = start;
            m_pauseTarget = std::clamp(float(item) - 1, 0.0f, float(m_pauseItems.size()) - 3);
        }
        const char *table = "MDS_PAUSE_ROOT";
        if (m_pauseHelp) { table = "MDS_HELP"; }
        const auto *entry = OriginalMenuData(table, m_pauseItems[item]);
        switch (entry->action) {
        case 31: return SurvivalHudAction::Resume;
        case 40: return SurvivalHudAction::Exit;
        case 9: return SurvivalHudAction::Sound;
        case 10: return SurvivalHudAction::Music;
        case 16: return SurvivalHudAction::DockedSticks;
        case 1: m_pauseHelp = true; ResetPauseList(); break;
        }
        return SurvivalHudAction::None;
    }
    return SurvivalHudAction::None;
}

bool SurvivalHud::DrawOriginalPause(const SurvivalHudState &state) {
    const unsigned list = m_movies.Ordinal("GLU_MOVIE_LIST_MENU_PAUSE");
    const unsigned button = m_movies.Ordinal("GLU_MOVIE_LIST_MENU_BUTTON");
    const unsigned body = m_movies.Ordinal("GLU_MOVIE_LIST_MENU_TEXT");
    unsigned start = 0, end = 0, focusStart = 0, focusEnd = 0, restStart = 0, restEnd = 0;
    unsigned bodyStart = 0, bodyEnd = 0, textStart = 0, textEnd = 0;
    if (!m_movies.GetMovie(list)->GetChapterRange(1, start, end) ||
        !m_movies.GetMovie(button)->GetChapterRange(2, focusStart, focusEnd) ||
        !m_movies.GetMovie(button)->GetChapterRange(0, restStart, restEnd) ||
        !m_movies.GetMovie(body)->GetChapterRange(0, bodyStart, bodyEnd) ||
        !m_movies.GetMovie(body)->GetChapterRange(1, textStart, textEnd)) { return false; }
    if (!m_pauseBound) {
        m_pauseBound = true;
        m_pauseItems.clear();
        const char *table = "MDS_PAUSE_ROOT";
        if (m_pauseHelp) { table = "MDS_HELP"; }
        for (unsigned index = 0; OriginalMenuData(table, index) != nullptr; ++index) {
            if (!m_pauseHelp && OriginalMenuData(table, index)->action == 135) { continue; }
            m_pauseItems.push_back(index);
        }
        m_pauseButtonTimes.assign(m_pauseItems.size(), 0);
        m_pauseButtonTimes[0] = focusStart;
    } else {
        m_pauseTime = std::min(start, m_pauseTime + m_pauseDelta);
        m_pauseBodyTime = std::min(bodyEnd, m_pauseBodyTime + m_pauseDelta);
    }
    const float step = float(m_pauseDelta) / (end - start + 1);
    if (m_pausePosition < m_pauseTarget) { m_pausePosition = std::min(m_pauseTarget, m_pausePosition + step); }
    else { m_pausePosition = std::max(m_pauseTarget, m_pausePosition - step); }
    const int first = static_cast<int>(std::floor(m_pausePosition)) - 1;
    unsigned time = m_pauseTime;
    if (time == start) { time += unsigned((m_pausePosition - std::floor(m_pausePosition)) * (end - start)); }
    m_pauseHits.clear();
    // CMenuSystem supplies HEADER with NAVBAR_DISABLED; no trunk buttons.
    const unsigned header = m_movies.Ordinal("GLU_MOVIE_HEADER");
    unsigned headerStart = 0, headerEnd = 0;
    if (!m_movies.GetMovie(header)->GetChapterRange(2, headerStart, headerEnd)) { return false; }
    unsigned headerTime = m_pauseTime;
    if (m_pauseTime == start) { headerTime = headerStart; }
    class PauseHeader : public IMovieRegionCallback {
    public:
        PauseHeader(MovieRenderer &renderer, const SurvivalHudState &snapshot) : movies(renderer), state(snapshot) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index == 14 || region.index == 15) {
                std::uint32_t value = static_cast<std::uint32_t>(state.coins);
                if (region.index == 15) { value = static_cast<std::uint32_t>(state.warbucks); }
                movies.Text(std::to_string(value), region.x, region.y, 0, 1, 0, region.alpha);
            }
            if (region.index == 16) {
                const unsigned info = movies.Ordinal("GLU_MOVIE_INFO_CLUSTER");
                unsigned start = 0, end = 0;
                start = 0; // INFO_CLUSTER has no chapter track; native code loops duration.
                if (!movies.Draw(info, start, region.x, region.y)) { return false; }
                for (const auto &part : movies.Regions(info, start, region.x, region.y)) {
                    if (part.index == 0) {
                        const float ratio = std::clamp(float(state.experience) / std::max<std::uint64_t>(1, state.experienceDelta), 0.0f, 1.0f);
                        movies.Rectangle(part.x, part.y, int(part.width * ratio), part.height, 1.0f / 255, 149.0f / 255, 215.0f / 255, part.alpha);
                    } else if (part.index < 4) {
                        char digits[16];
                        std::snprintf(digits, sizeof(digits), "%.3u", state.level);
                        movies.Text(std::string(1, digits[part.index - 1]), part.x, part.y + part.height / 2 - std::floor(movies.TextHeight(7) / 2), 7, 1, 0, part.alpha);
                    }
                }
            }
            return true;
        }
        MovieRenderer &movies;
        const SurvivalHudState &state;
    } headerCallback(m_movies, state);
    if (m_pauseHelp && !m_movies.DrawNamed("GLU_MOVIE_BG_OPTIONS", m_pauseTime)) { return false; }
    if (!m_movies.Draw(header, headerTime, 512, 384, 1024, 768, 0, 1, &headerCallback) || !m_movies.Draw(list, time)) { return false; }
    // Shared RADIAL_WIDGET is present even when the root has no BACK action.
    for (const auto &region : m_movies.Regions(list, time)) {
        if (region.index != m_movies.Regions(list, time, 512, 384, true).size() - 3) { continue; }
        const unsigned radial = m_movies.Ordinal("GLU_MOVIE_RADIAL_WIDGET");
        unsigned radialStart = 0, radialEnd = 0;
        if (!m_movies.GetMovie(radial)->GetChapterRange(1, radialStart, radialEnd)) { return false; }
        if (!m_movies.Draw(radial, radialStart, region.x + int(region.width) / 2, region.y + int(region.height) / 2)) { return false; }
    }
    if (m_pauseHelp) {
        // CMenuList::Init :140686 creates BACK_BUTTON at region tagged 1.
        for (const auto &region : m_movies.Regions(list, time)) {
            if (region.index != m_movies.Regions(list, time, 512, 384, true).size() - 3) { continue; }
            const unsigned back = m_movies.Ordinal("GLU_MOVIE_BACK_BUTTON");
            unsigned backStart = 0, backEnd = 0;
            if (!m_movies.GetMovie(back)->GetChapterRange(0, backStart, backEnd)) { return false; }
            const float x = region.x + int(region.width) / 2, y = region.y + int(region.height) / 2;
            if (!m_movies.Draw(back, std::min(m_pauseTime, backEnd), x, y)) { return false; }
            if (m_pauseTime != start) { continue; }
            for (const auto &part : m_movies.Regions(back, backEnd, x, y, true)) {
                if (part.index == 0) { m_pauseHits.insert(m_pauseHits.begin(), {part, UINT32_MAX}); }
            }
        }
    }
    for (const auto &region : m_movies.Regions(list, time)) {
        if (region.type < 2) { continue; }
        const int index = first + int(region.type) - 2;
        if (index < 0 || index >= int(m_pauseItems.size())) { continue; }
        auto &buttonTime = m_pauseButtonTimes[index];
        if (unsigned(index) == m_pauseFocus) { buttonTime = focusStart + (buttonTime - focusStart + m_pauseDelta) % (focusEnd - focusStart + 1); }
        else { buttonTime = std::min(restEnd, buttonTime + m_pauseDelta); }
        const float x = region.x + int(region.width) / 2, y = region.y + int(region.height) / 2;
        const std::string label = PauseText(state, m_pauseItems[index], 0);
        class Caption : public IMovieRegionCallback {
        public:
            Caption(MovieRenderer &renderer, const std::string &caption) : movies(renderer), label(caption) {}
            bool DrawMovieRegion(const MovieRegion &part) override {
                if (part.index == 1) { movies.Text(label, part.x, part.y + int(part.height) / 2 - int(movies.TextHeight(0)) / 2, 0, 1, 0, part.alpha); }
                return true;
            }
            MovieRenderer &movies;
            const std::string &label;
        } callback(m_movies, label);
        if (!m_movies.Draw(button, buttonTime, x, y, 1024, 768, 0, region.alpha, &callback)) { return false; }
        if (m_pauseTime != start) { continue; }
        for (const auto &part : m_movies.Regions(button, buttonTime, x, y, true)) {
            if (part.index == 0) { m_pauseHits.push_back({part, unsigned(index)}); }
        }
    }
    MovieRegion content, scrollBar, pageBounds;
    if (!m_movies.Region(list, 8, time, content) || !m_movies.Region(list, 9, time, scrollBar) ||
        !m_movies.Region(body, 2, 0, pageBounds)) { return false; }
    const auto lines = FormatStoreText(m_movies, PauseText(state, m_pauseItems[m_pauseFocus], 1), content.width - scrollBar.width, {0, 6, 0, 0, 0});
    std::vector<std::vector<StoreTextLine>> pages(1);
    float pageHeight = 0;
    for (const auto &line : lines) {
        if (!pages.back().empty() && pageHeight + line.height > pageBounds.height) { pages.push_back({}); pageHeight = 0; }
        pages.back().push_back(line);
        pageHeight += line.height;
    }
    m_pauseBodyPosition = std::clamp(m_pauseBodyPosition, 0.0f, float(pages.size() - 1));
    const int firstPage = int(std::floor(m_pauseBodyPosition));
    unsigned textTime = m_pauseBodyTime;
    if (textTime == bodyEnd) { textTime = textStart + unsigned((m_pauseBodyPosition - firstPage) * (textEnd - textStart)); }
    if (!m_movies.Draw(body, textTime, content.x, content.y)) { return false; }
    GLint viewport[4], previousClip[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_SCISSOR_BOX, previousClip);
    const GLboolean clipped = glIsEnabled(GL_SCISSOR_TEST);
    glEnable(GL_SCISSOR_TEST);
    glScissor(int(content.x * viewport[2] / 1024), int((768 - content.y - content.height) * viewport[3] / 768),
        int(content.width * viewport[2] / 1024), int(content.height * viewport[3] / 768));
    for (const auto &region : m_movies.Regions(body, textTime, content.x, content.y)) {
        if (region.type < 2) { continue; }
        const int page = firstPage + int(region.type) - 2;
        if (page < 0 || page >= int(pages.size())) { continue; }
        float y = region.y;
        for (const auto &line : pages[page]) {
            for (const auto &run : line.runs) { m_movies.Text(run.text, region.x + run.x, y + (line.height - run.height) / 2, run.font, 1, 0, content.alpha * region.alpha); }
            y += line.height;
        }
    }
    glScissor(previousClip[0], previousClip[1], previousClip[2], previousClip[3]);
    if (!clipped) { glDisable(GL_SCISSOR_TEST); }
    if (pages.size() > 1) {
        const auto *entry = OriginalMenuData("MDS_SCROLLBARS", 1);
        const unsigned bar = m_movies.Ordinal(entry->movies[0]);
        MovieRegion bounds;
        if (!m_movies.Region(bar, 0, 0, bounds)) { return false; }
        if (!m_movies.Draw(bar, unsigned(m_movies.GetMovie(bar)->duration * m_pauseBodyPosition / (pages.size() - 1)),
            scrollBar.x, scrollBar.y + int(scrollBar.height) / 2 - int(bounds.height) / 2)) { return false; }
    }
    m_pauseDelta = 0;
    return m_movies.Failures() == 0;
}

int RunOriginalPauseCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    CWindow window;
    if (!window.Open("Gun Bros - Original Pause Check", 1600, 1200)) { return 1; }
    SurvivalHud hud;
    PackTables tables(toc);
    if (!hud.Init(toc, tables)) { return 1; }
    CProfileManager profile;
    const auto directory = std::filesystem::path("out/ui-original-2026-09-09") / ("pause-profile-" + std::to_string(window.GetTicksMs()));
    CRefinementManager::Template refinement;
    if (!LoadRefinementTemplate(toc, tables, refinement)) { return 1; }
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!LoadNativeProfile(toc, tables, profile, directory, std::filesystem::path(ASSET_ROOT) / "saves")) { return 1; }
    CPlayerProgress progress;
    progress.Bind(profile.nativeArchive->progression);
    progress.SetExperience(profile.experience);
    SurvivalHudState state;
    state.originalUi = true;
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
            const unsigned originalAction = OriginalMenuData("MDS_PAUSE_ROOT", hud.m_pauseItems[index])->action;
            if (originalAction == 31 && action != SurvivalHudAction::Resume) { ++failures; }
            if (originalAction == 40 && action != SurvivalHudAction::Exit) { ++failures; }
            if (originalAction == 9) {
                if (action != SurvivalHudAction::Sound) { ++failures; }
                profile.soundEnabled = !profile.soundEnabled;
                state.soundEnabled = profile.soundEnabled;
            }
            if (originalAction == 10) {
                if (action != SurvivalHudAction::Music) { ++failures; }
                profile.musicEnabled = !profile.musicEnabled;
                state.musicEnabled = profile.musicEnabled;
            }
            if (originalAction == 16) {
                if (action != SurvivalHudAction::DockedSticks) { ++failures; }
                profile.options.ToggleDockedSticks();
                state.dockedSticks = profile.options.DockedSticks();
            }
            break;
        }
        if (!found) { ++failures; }
        if (!hud.Draw(state)) { ++failures; }
        if (index == 0 && !window.SaveFrame("out/ui-original-2026-09-09/pause-original-ready.png")) { ++failures; }
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
        if (index == 0 && !window.SaveFrame("out/ui-original-2026-09-09/pause-original-help.png")) { ++failures; }
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
    if (!LoadNativeProfile(toc, tables, reloaded, directory) || reloaded.soundEnabled != profile.soundEnabled ||
        reloaded.musicEnabled != profile.musicEnabled || reloaded.options.DockedSticks() != profile.options.DockedSticks() ||
        reloaded.coins != state.coins || reloaded.warbucks != state.warbucks) { ++failures; }
    std::printf("[pause-check] native-hits=%u help-items=14 back=%d preference-reload=3 failures=%u\n", hits, returned, failures);
    return failures != 0;
}

std::vector<SurvivalHud::Button> SurvivalHud::OriginalControlButtons(const SurvivalHudState &state) const {
    std::vector<Button> buttons;
    const unsigned base = m_movies.Ordinal("GLU_MOVIE_HUD_PAD_IPAD");
    const unsigned peripheral = m_movies.Ordinal("GLU_MOVIE_HUD_PAUSE");
    unsigned idle = 0, end = 0;
    if (!m_movies.GetMovie(peripheral)->GetChapterRange(3, idle, end)) { return buttons; }
    MovieRegion region;
    if (m_movies.Region(peripheral, 3, idle, region)) { buttons.push_back({region, SurvivalHudAction::Pause, ""}); }
    if (m_movies.Region(base, 2, 0, region)) { buttons.push_back({region, SurvivalHudAction::OpenShop, ""}); }
    if (m_movies.Region(base, 3, 0, region)) { buttons.push_back({region, SurvivalHudAction::SwapWeapon, ""}); }
    for (unsigned slot = 0; slot < 2; ++slot) {
        if (!m_movies.Region(base, 4 + slot, 0, region)) { continue; }
        const float x = region.x + int(region.width) / 2, y = region.y + int(region.height) / 2;
        const char *name = "GLU_MOVIE_POWERUP_BUTTON";
        SurvivalHudAction action = SurvivalHudAction::UseLeft;
        if (slot == 1) { name = "GLU_MOVIE_GRENADE_BUTTON"; action = SurvivalHudAction::UseItem; }
        const unsigned movie = m_movies.Ordinal(name);
        if (!m_movies.GetMovie(movie)->GetChapterRange(2, idle, end)) { continue; }
        for (const auto &part : m_movies.Regions(movie, idle, x, y, true)) {
            if (part.index == 0) { buttons.push_back({part, action, ""}); }
        }
    }
    return buttons;
}

bool SurvivalHud::BackFromSelectorPrompt() {
    if (!m_selectorPromptRequested && !m_selectorPrompt.IsActive()) { return false; }
    m_selectorPromptRequested = false;
    m_selectorPrompt.Hide();
    return true;
}

bool SurvivalHud::DrawOriginalMeter(const MovieRegion &area, unsigned slot) {
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
    m_movies.Rectangle(x, y, width, 1, red, green, blue, area.alpha);
    m_movies.Rectangle(x, y + height - 1, width, 1, red, green, blue, area.alpha);
    m_movies.Rectangle(x, y, 1, height, red, green, blue, area.alpha);
    m_movies.Rectangle(x + width - 1, y, 1, height, red, green, blue, area.alpha);
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
        m_movies.Rectangle(x + 2 + gradientWidth, y + 2, 1, interiorHeight,
            float((divider >> 16) & 255) / 255, float((divider >> 8) & 255) / 255, float(divider & 255) / 255, area.alpha);
    }
    return m_movies.Gradient(x + 2, y + 2, gradientWidth, interiorHeight, top, bottom, area.alpha) &&
        m_movies.Gradient(x + 2 + filled, y + 2, interiorWidth - filled, interiorHeight, config[3], config[4], area.alpha);
}

bool SurvivalHud::DrawOriginalPowerup(const SurvivalHudState &state, unsigned slot, float x, float y, unsigned movie, unsigned time) {
    const GameObjectRef *object = &state.leftPowerup;
    unsigned count = state.leftCount;
    if (slot == 1) { object = &state.rightPowerup; count = state.rightCount; }
    const PowerupEntry *powerup = nullptr;
    for (const auto &entry : m_powerups) {
        if (entry.resource.packHash == object->packHash && entry.resource.localIndex == object->localIndex) { powerup = &entry; break; }
    }
    class PowerupCallback : public IMovieRegionCallback {
    public:
        PowerupCallback(SurvivalHud &owner, const PowerupEntry *item, unsigned quantity) : hud(owner), powerup(item), count(quantity) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            // Original binding: region1 = count, region2 = alternate input icon.
            if (powerup == nullptr) { return true; }
            if (region.index == 1) {
                unsigned animation = 87;
                if (count > 9) { animation = 88; }
                const int x = int(region.x) + int(region.width) / 2, y = int(region.y) + int(region.height) / 2;
                if (!hud.m_movies.DrawSprite(0, animation, 0, float(x), float(y), 1, region.alpha)) { return false; }
                const auto text = std::to_string(count);
                return hud.m_movies.Text(text, x - int(hud.m_movies.TextWidth(text, 0)) / 2,
                    y - int(hud.m_movies.TextHeight(0)) / 2, 0, 1, 0, region.alpha);
            }
            if (region.index == 2) {
                const auto &sprite = powerup->data.sprite;
                auto &renderer = *hud.m_powerupRenderers.at(sprite.packHash);
                MovieRegion bounds;
                const unsigned animation = powerup->data.field29;
                if (!renderer.SpriteBounds(sprite.archetype, animation, bounds)) { return false; }
                return renderer.DrawSprite(sprite.archetype, animation, 0,
                    region.x - bounds.x + int(region.width - bounds.width) / 2,
                    region.y - bounds.y + int(region.height - bounds.height) / 2, 1, region.alpha);
            }
            return true;
        }
        SurvivalHud &hud;
        const PowerupEntry *powerup;
        unsigned count;
    } callback(*this, powerup, count);
    return m_movies.Draw(movie, time, x, y, 1024, 768, 0, 1, &callback);
}

bool SurvivalHud::DrawOriginalControls(const SurvivalHudState &state) {
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
    const unsigned base = m_movies.Ordinal("GLU_MOVIE_HUD_PAD_IPAD");
    class BaseCallback : public IMovieRegionCallback {
    public:
        explicit BaseCallback(SurvivalHud &owner) : hud(owner) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index < 2) { return hud.DrawOriginalMeter(region, region.index); }
            if (region.index == 2 || region.index == 3) {
                unsigned animation = 27;
                if (region.index == 3) { animation = 35; }
                return hud.m_movies.DrawSprite(1, animation, hud.m_controlTime, region.x, region.y + region.height, 1, region.alpha);
            }
            return true;
        }
        SurvivalHud &hud;
    } baseCallback(*this);
    if (!m_movies.Draw(base, 0, 512, 384, 1024, 768, 0, 1, &baseCallback)) { return false; }
    MovieRegion stickBounds;
    if (!m_movies.SpriteBounds(1, 6, stickBounds)) { return false; }
    for (unsigned slot = 0; slot < 2; ++slot) {
        MovieRegion area;
        if (!m_movies.Region(base, 4 + slot, 0, area)) { return false; }
        const float x = area.x + int(area.width) / 2, y = area.y + int(area.height) / 2;
        float directionX = state.moveX, directionY = state.moveY;
        const char *name = "GLU_MOVIE_POWERUP_BUTTON";
        if (slot == 1) { directionX = state.aimX; directionY = state.aimY; name = "GLU_MOVIE_GRENADE_BUTTON"; }
        // Windows keys/mouse supply the original normalized control vector.
        // Floating sticks remain hidden until their corresponding input is active.
        if (!state.dockedSticks && directionX == 0 && directionY == 0) { continue; }
        if (!m_movies.DrawSprite(1, 6 + slot, m_controlTime, x, y)) { return false; }
        const unsigned movie = m_movies.Ordinal(name);
        unsigned start = 0, end = 0;
        if (!m_movies.GetMovie(movie)->GetChapterRange(2, start, end) ||
            !DrawOriginalPowerup(state, slot, x, y, movie, start + m_controlTime % (end - start + 1))) { return false; }
        // Bind radius = sprite width * .42; ControlStick::Draw displacement *.35.
        const float travel = stickBounds.width * 0.42f * 0.35f;
        if (!m_movies.DrawSprite(1, 8, m_controlTime, float(int(x + directionX * travel)), float(int(y + directionY * travel)))) { return false; }
    }
    class PeripheralCallback : public IMovieRegionCallback {
    public:
        PeripheralCallback(MovieRenderer &renderer, const SurvivalHudState &snapshot) : movies(renderer), state(snapshot) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
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
        MovieRenderer &movies;
        const SurvivalHudState &state;
    } peripheralCallback(m_movies, state);
    const unsigned peripheral = m_movies.Ordinal("GLU_MOVIE_HUD_PAUSE");
    unsigned start = 0, end = 0;
    if (!m_movies.GetMovie(peripheral)->GetChapterRange(3, start, end)) { return false; }
    return m_movies.Draw(peripheral, start, 512, 384, 1024, 768, 0, 1, &peripheralCallback);
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
    PackTables tables(toc);
    CWindow window;
    if (!window.Open("Gun Bros - Original HUD Check", 1600, 1200)) { return 1; }
    SurvivalHud hud;
    if (!hud.Init(toc, tables)) { return 1; }
    SurvivalHudState state;
    state.originalUi = true;
    state.health = state.maximumHealth = 1;
    unsigned hits = 0;
    const auto buttons = hud.OriginalControlButtons(state);
    if (buttons.size() != 5) { ++failures; }
    for (const auto &button : buttons) {
        const float x = button.rect.x + button.rect.width / 2, y = button.rect.y + button.rect.height / 2;
        hud.Pointer(state, x, y, false);
        if (hud.Pointer(state, x, y, true) != button.action) { ++failures; }
        else { ++hits; }
    }
    // Perturb the in-memory parsed Movie: hit geometry must follow its bytes.
    const unsigned base = hud.m_movies.Ordinal("GLU_MOVIE_HUD_PAD_IPAD");
    CMovie *movie = hud.m_movies.GetMovie(base);
    unsigned regionIndex = 0;
    bool mutated = false;
    for (auto &object : movie->objects) {
        if (object.type != 6) { continue; }
        if (regionIndex++ != 2) { continue; }
        const auto original = object;
        for (auto &frame : object.frames) { frame.x += 23; }
        const auto changed = hud.OriginalControlButtons(state);
        for (const auto &button : changed) {
            if (button.action != SurvivalHudAction::OpenShop) { continue; }
            for (const auto &before : buttons) {
                if (before.action == button.action && std::abs(button.rect.x - before.rect.x - 23) < 0.001f) { mutated = true; }
            }
        }
        object = original;
        break;
    }
    if (!mutated) { ++failures; }
    unsigned icons = 0;
    for (const auto &powerup : hud.m_powerups) {
        state.leftPowerup = state.rightPowerup = powerup.resource;
        state.leftCount = 9;
        state.rightCount = 10;
        state.moveX = 1;
        state.aimY = -1;
        if (!hud.DrawOriginalControls(state)) { ++failures; }
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
        if (!hud.DrawOriginalControls(state) || !window.SaveFrame("out/ui-original-2026-09-09/hud-original-" + std::to_string(mode) + ".png")) { ++failures; }
    }
    // Original notifications: source strings, two level-up Movies, independent
    // perfect title/body and completion on the last actual resource duration.
    hud.ResetNotices();
    state.level = 12;
    state.originalUi = true;
    hud.ObserveProgress(state);
    ++state.level;
    hud.ObserveProgress(state);
    if (hud.NoticeCount() != 2 || hud.HasInterstitial()) { ++failures; }
    unsigned noticeChecks = 0;
    for (unsigned index = 0; index < 2; ++index) {
        if (hud.m_notices.empty()) { ++failures; break; }
        const auto &notice = hud.m_notices.front();
        const unsigned duration = hud.m_movies.GetMovie(notice.movie)->duration;
        if (notice.title.empty() || notice.title.find("%d") != std::string::npos || notice.title.find("%i") != std::string::npos) { ++failures; }
        std::printf("[original-overlay-check] level step=%u movie=%u duration=%u text=%s\n", index, notice.movie, duration, notice.title.c_str());
        hud.Advance(duration / 2);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        hud.DrawNotice(true);
        if (!window.SaveFrame("out/ui-original-2026-09-09/hud-level-original-" + std::to_string(index) + ".png")) { ++failures; }
        hud.Advance(duration - duration / 2);
        ++noticeChecks;
    }
    for (unsigned mode = 0; mode < 4; ++mode) {
        if (mode == 0) { hud.BeginOriginalLevel(23, false, false); }
        if (mode == 1) { hud.BeginOriginalLevel(2, true, false); }
        if (mode == 2) { hud.BeginOriginalLevel(1, false, true); }
        if (mode == 3) { hud.OnOriginalWaveClear(23, true, 10, false); }
        if (!hud.HasInterstitial() || hud.TakeInterstitialCompletion()) { ++failures; }
        unsigned step = 0;
        while (!hud.m_notices.empty()) {
            const auto &notice = hud.m_notices.front();
            const unsigned duration = hud.m_movies.GetMovie(notice.movie)->duration;
            const bool final = notice.releaseLevel;
            if (notice.title.empty() || notice.title.find("%d") != std::string::npos || notice.footer.find("%d") != std::string::npos ||
                notice.title.find("%i") != std::string::npos || notice.footer.find("%i") != std::string::npos) { ++failures; }
            std::printf("[original-overlay-check] mode=%u movie=%u duration=%u title=%s footer=%s\n", mode, notice.movie, duration, notice.title.c_str(), notice.footer.c_str());
            hud.Advance(duration / 2);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            hud.DrawNotice(true);
            if (!window.SaveFrame("out/ui-original-2026-09-09/hud-notice-original-" + std::to_string(mode) + "-" + std::to_string(step++) + ".png")) { ++failures; }
            hud.Advance(duration - duration / 2 - 1);
            if (hud.TakeInterstitialCompletion()) { ++failures; }
            hud.Advance(1);
            if (hud.TakeInterstitialCompletion() != final || hud.TakeInterstitialCompletion()) { ++failures; }
            ++noticeChecks;
        }
    }
    hud.OnOriginalWaveClear(1, false, 10, true);
    if (hud.NoticeCount() != 1 || !hud.m_notices.front().title.empty()) { ++failures; }
    if (hud.m_movies.Failures() != 0) { ++failures; }
    std::printf("[original-overlay-check] timelines=%u failures=%u\n", noticeChecks, failures);
    const unsigned errors = glGetError();
    if (errors != 0) { ++failures; }
    std::printf("[original-hud-check] hits=%u alternate-icons=%u region-mutation=%d meter-interpolation=3 gl=%u failures=%u\n", hits, icons, mutated, errors, failures);
    return failures != 0;
}
