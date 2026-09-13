#include "gun_bros_re/debug/SurvivalDebug.h"
/** @file SurvivalHud.cpp
 * @brief CInputPad::Base::Bind (:88320) binds meters to regions 0/1 and guns to 2/3.
 */
#define NOMINMAX
#include "gun_bros_re/ui/SurvivalHud.h"
#include "gun_bros_re/ui/OriginalMenuData.h"
#include "gun_bros_re/ui/OriginalTextLayout.h"
#include "gun_bros_re/HostSettings.h"
#include "gun_bros_re/data/PowerupCatalog.h"
#include "engine/platform/CWindow.h"
#include "engine/resources/CResTOCManager.h"
#include "gun_bros_re/gameplay/CLevel.h"
#include "engine/graphics/CPNG.h"
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
    // First-use font and atlas uploads belong to loading, not the first shot
    // CStoreAggregator::InitFilteredList :159210 and SortFilteredList :155433.
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
    constexpr unsigned sprites[] = {6, 7, 8, 27, 28, 35, 36, 39, 87};
    for (unsigned sprite : sprites) { m_movies.SpriteDuration(1, sprite); }
    for (unsigned font : {0u, 1u, 5u, 6u, 7u, 9u}) { m_movies.TextWidth("0123456789", font); }
    for (const char *name : {"IDS_HUD_EXPERIENCE_UP", "IDS_HUD_POINTS_UP"}) {
        if (m_movies.NamedString(name).empty()) { return false; }
    }
    // Composite sticks and badges have their own lazy-expanded frame caches.
    // Draw a neutral snapshot while the loading screen still owns presentation.
    if (!DrawOriginalControls(SurvivalHudState{})) { return false; }
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
    // Every entry, including isolated map studies, uses original input geometry.
    if (state.shopOpen || state.paused || state.dead || state.cleared) { return {}; }
    return OriginalControlButtons(state);
}

SurvivalHudAction SurvivalHud::Pointer(const SurvivalHudState &state, float x, float y, bool down) {
    m_mouseX = x;
    m_mouseY = y;
    bool clicked = down && !m_previousDown;
    if (state.shopOpen && !state.itemChoice && !m_selectorPromptRequested && !m_selectorPrompt.IsActive()) {
        // Commit a list tap on release so dragging an icon cannot equip it.
        if (clicked) {
            m_selectorPressX = x;
            m_selectorPressY = y;
            m_selectorDragged = false;
            m_selectorPressArmed = false;
            for (const auto &hit : m_selectorHits) {
                if (hit.area.Contains(x, y) && (hit.action == SurvivalHudAction::SelectItem || hit.action == SurvivalHudAction::BuyItem)) {
                    m_selectorPressArmed = true;
                    break;
                }
            }
        }
        if (m_previousDown && std::hypot(x - m_selectorPressX, y - m_selectorPressY) > 8) {
            m_selectorDragged = true;
        }
        if (m_selectorPressArmed) { clicked = !down && m_previousDown && !m_selectorDragged; }
    }
    if (!down || !HasChallenges() || state.paused || state.shopOpen || state.dead || state.cleared) { m_challengeHeld = false; }
    if (clicked && !HasInterstitial()) {
        MovieRegion button;
        if (FindActionRegion(state, SurvivalHudAction::BroOps, button) && button.Contains(x, y)) {
            m_challengeHeld = true;
            m_challengeTime = 0;
        }
    }
    m_previousDown = down;
    if (!clicked) { return SurvivalHudAction::None; }
    if (state.shopOpen) {
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
    if (state.paused && !state.shopOpen && !state.dead && !state.cleared) {
        return OriginalPausePointer(state, x, y);
    }
    for (const Button &button : Buttons(state)) {
        if (button.rect.Contains(x, y)) { return button.action; }
    }
    return SurvivalHudAction::None;
}

void SurvivalHud::Centre(const std::string &text, float y, unsigned font, float scale) {
    const float width = m_movies.TextWidth(text, font, scale);
    m_movies.Text(text, 512 - width * 0.5f, y, font, scale);
}

bool SurvivalHud::DrawTutorialDebugNotice(std::uint64_t ticks) {
    ::DrawTutorialDebugNotice(m_movies, ticks);
    return m_movies.Failures() == 0;
}

bool SurvivalHud::Draw(const SurvivalHudState &state) {
    for (const auto &bar : state.enemyHealthBars) {
        // Original Utility::DrawRect border 0xFF7F8C98 and red fill.
        m_movies.Rectangle(bar.x, bar.y, bar.width, 1, 127 / 255.0f, 140 / 255.0f, 152 / 255.0f, 1);
        m_movies.Rectangle(bar.x, bar.y + bar.height - 1, bar.width, 1, 127 / 255.0f, 140 / 255.0f, 152 / 255.0f, 1);
        m_movies.Rectangle(bar.x, bar.y, 1, bar.height, 127 / 255.0f, 140 / 255.0f, 152 / 255.0f, 1);
        m_movies.Rectangle(bar.x + bar.width - 1, bar.y, 1, bar.height, 127 / 255.0f, 140 / 255.0f, 152 / 255.0f, 1);
        m_movies.Rectangle(bar.x + bar.border, bar.y + bar.border,
            (bar.width - 2 * bar.border) * bar.fraction, bar.height - 2 * bar.border,
            bar.red, std::max(0.0f, bar.red - 200 / 255.0f), std::max(0.0f, bar.red - 200 / 255.0f), 1);
    }
    if (!state.shopOpen) { m_selectorBound = false; m_selectorHits.clear(); }
    if (!state.paused && m_pauseBound) { m_pauseHelp = false; ResetPauseList(); }
    ObserveProgress(state);
    if (state.withBrother && state.brotherHealth > 0 && state.brotherLabelAlpha > 0) {
        m_movies.Text(state.brotherName, state.brotherLabelX - m_movies.TextWidth(state.brotherName, 0) * 0.5f,
            state.brotherLabelY - m_movies.TextHeight(0) / 2, 0, 1, 0, state.brotherLabelAlpha);
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
            const unsigned introDuration = m_movies.SpriteDuration(1, animation);
            if (introDuration > 0 && elapsed >= introDuration) { animation = indicatorAnimations[2][1]; elapsed -= introDuration; }
        }
        m_movies.DrawSprite(1, animation, elapsed, x, y, 1, indicator.Alpha(), angle);
        const unsigned icon = indicatorAnimations[indicator.type][2];
        if (icon != 255) { m_movies.DrawSprite(1, icon, elapsed, x, y, 1, indicator.Alpha()); }
    }
    if (!DrawOriginalControls(state)) { return false; }
    m_challengeRows = 0;
    if ((m_challengeHeld || m_challengeTime != 0) && !state.paused && !state.shopOpen && !state.dead && !state.cleared) {
        // ShowChallengeInfoOverlay uses HUD region2's bottom-center as origin.
        MovieRegion origin;
        const auto peripheral = m_movies.Ordinal("GLU_MOVIE_HUD_PAUSE");
        unsigned start = 0, end = 0;
        if (!m_movies.GetMovie(peripheral)->GetChapterRange(5, start, end) || !m_movies.Region(peripheral, 2, start, origin)) { return false; }
        if (!DrawChallengeOverlay(origin.x + origin.width / 2, origin.y + origin.height, m_challengeTime)) { return false; }
    }
    DrawNotice();
    DrawSurvivalDebugInfo(m_movies, state);
    if (state.shopOpen) { return DrawOriginalSelector(state); }
    if (state.paused && !state.dead && !state.cleared) { return DrawOriginalPause(state); }
    // End-of-run navigation is owned by the original postgame menu in the shell.
    // CDialogPopup draws the original radio portrait beside its text region.
    if (!m_dialog.Draw()) { return false; }
    return m_movies.Failures() == 0;
}

const StoreEntry *SurvivalHud::SelectedItem() const {
    if (m_selectedItem < 0 || m_selectedItem >= static_cast<int>(m_store.size())) { return nullptr; }
    return &m_store[m_selectedItem];
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

std::vector<SurvivalHud::Button> SurvivalHud::OriginalControlButtons(const SurvivalHudState &state) const {
    std::vector<Button> buttons;
    const unsigned base = m_movies.Ordinal("GLU_MOVIE_HUD_PAD_IPAD");
    const unsigned peripheral = m_movies.Ordinal("GLU_MOVIE_HUD_PAUSE");
    unsigned idle = 0, end = 0;
    unsigned chapter = 3;
    if (HasChallenges()) { chapter = 5; }
    if (!m_movies.GetMovie(peripheral)->GetChapterRange(chapter, idle, end)) { return buttons; }
    if (HasChallenges()) { idle = end; }
    MovieRegion region;
    if (HasChallenges() && m_movies.Region(peripheral, 4, idle, region)) { buttons.push_back({region, SurvivalHudAction::BroOps, ""}); }
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
