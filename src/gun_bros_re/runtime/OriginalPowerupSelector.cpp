/** @file OriginalPowerupSelector.cpp
 * @brief CPowerUpSelector single-player adapter; original source :184203-187670.
 * ui_movie.bt / powerup_template.bt / store_entry.bt describe the BIG inputs.
 */
#define NOMINMAX
#include "runtime/SurvivalHud.h"
#include "runtime/OriginalMenuData.h"
#include "runtime/OriginalTextLayout.h"
#include "engine/platform/CWindow.h"
#include "gun_bros/CResTOCManager.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

std::string SurvivalHud::SelectorCurrency(const char *name, std::uint64_t amount) {
    std::string text = m_movies.NamedString(name);
    const auto marker = text.find("%i");
    if (marker == std::string::npos) {
        std::printf("[powerup-selector] unsupported currency format %s\n", name);
        return text;
    }
    text.replace(marker, 2, std::to_string(static_cast<unsigned>(amount)));
    return text;
}

bool SurvivalHud::DrawSelectorButton(const char *table, unsigned index, float x, float y, float alpha,
    SurvivalHudAction action, int storeIndex, MovieRegion *touch) {
    const auto *entry = OriginalMenuData(table, index);
    if (entry == nullptr) { return false; }
    const unsigned movie = m_movies.Ordinal(entry->movies[0]);
    unsigned start = 0, end = 0;
    if (!m_movies.GetMovie(movie)->GetChapterRange(1, start, end)) { return false; }
    // CMenuMovieButton::Draw consumes the original button size, without fitting.
    MovieRegion graphic;
    if (!m_movies.Region(movie, 1, end, graphic)) { return false; }
    const float originX = x - graphic.x - graphic.width / 2 + 512;
    const float originY = y - graphic.y - graphic.height / 2 + 384;
    class Callback : public IMovieRegionCallback {
    public:
        Callback(SurvivalHud &owner, const OriginalMenuEntry &data) : hud(owner), entry(data) {}
        bool DrawMovieRegion(const MovieRegion &area) override {
            if (area.index != 1) { return true; }
            const unsigned sprite = entry.sprites[0];
            if (sprite != UINT32_MAX && !hud.m_movies.DrawSprite(sprite >> 16, sprite & 255, 0,
                area.x, area.y, 1, area.alpha)) { return false; }
            const auto text = hud.m_movies.NamedString(entry.strings[0]);
            return hud.m_movies.Text(text, area.x + (area.width - hud.m_movies.TextWidth(text, 5)) / 2,
                area.y + (area.height - hud.m_movies.TextHeight(5)) / 2, 5, 1, 0, area.alpha);
        }
        SurvivalHud &hud;
        const OriginalMenuEntry &entry;
    } callback(*this, *entry);
    if (!m_movies.Draw(movie, end, originX, originY, 1024, 768, 0, alpha, &callback)) { return false; }
    for (auto area : m_movies.Regions(movie, end, originX, originY, true)) {
        if (area.index == 0 && touch != nullptr) { *touch = area; }
        if (area.index == 0 && action != SurvivalHudAction::None) {
            m_selectorHits.push_back({area, action, storeIndex});
        }
    }
    return true;
}

bool SurvivalHud::DrawSelectorItem(const SurvivalHudState &state, unsigned index, const MovieRegion &area) {
    const unsigned storeIndex = m_selectorEntries[index];
    const auto &store = m_store[storeIndex];
    const auto &reference = store.data.objects.front().object;
    const PowerupEntry *powerup = nullptr;
    for (const auto &item : m_powerups) {
        if (item.resource.packHash == reference.packHash && item.resource.localIndex == reference.localIndex) { powerup = &item; break; }
    }
    if (powerup == nullptr) { return false; }
    unsigned count = 0;
    for (const auto &owned : state.inventory) {
        if (owned.resource.packHash == reference.packHash && owned.resource.localIndex == reference.localIndex) { count = owned.count; break; }
    }
    float alpha = area.alpha;
    if (state.itemChoice && m_selectedItem != static_cast<int>(storeIndex)) { alpha *= 0.25f; }
    const auto &sprite = powerup->data.sprite;
    auto &renderer = *m_powerupRenderers.at(sprite.packHash);
    MovieRegion bounds;
    if (!renderer.SpriteBounds(sprite.archetype, sprite.animation, bounds)) { return false; }
    // PowerUpControlCallback :185179 centers the unscaled sprite, then raises it by height/4.
    const int x = int(area.x - bounds.x) + int(area.width - bounds.width) / 2;
    const int y = int(area.y - bounds.y) + int(area.height - bounds.height) / 2 - int(bounds.height) / 4;
    if (!renderer.DrawSprite(sprite.archetype, sprite.animation, 0, float(x), float(y), 1, alpha)) { return false; }
    const bool available = powerup->data.field112 == 0;
    if (!available && !renderer.DrawSprite(sprite.archetype, powerup->data.field28, 0, float(x), float(y), 1, alpha)) { return false; }
    if (count > 0) {
        unsigned animation = 87;
        if (count > 9) { animation = 88; }
        const int badgeX = x - int(bounds.height) / 8 + int(bounds.x + bounds.width);
        const int badgeY = y + int(bounds.height) / 8 + int(bounds.y);
        if (!m_movies.DrawSprite(0, animation, 0, float(badgeX), float(badgeY), 1, alpha)) { return false; }
        const auto text = std::to_string(count);
        m_movies.Text(text, badgeX - int(m_movies.TextWidth(text, 0)) / 2,
            badgeY - int(m_movies.TextHeight(0)) / 2, 0, 1, 0, alpha);
    }
    // Original 26-wide-character name buffer and font8, not the STORE thumbnail.
    // Retail names in this selector are ASCII and fit the original buffer.
    const auto &name = powerup->name;
    int topOffset = -1;
    if (area.height < 101) { topOffset = -4; }
    m_movies.Text(name, area.x + (area.width - m_movies.TextWidth(name, 8)) / 2,
        area.y + topOffset + int(m_movies.TextHeight(8)) / 2, 8, 1, 0, alpha);
    const char *currency = "IDS_SHOP_COMMON";
    unsigned price = store.data.commonPrice;
    if (price == 0) { price = store.data.rarePrice; currency = "IDS_SHOP_RARE"; }
    const auto cost = SelectorCurrency(currency, price);
    const float costY = y + bounds.height + bounds.y + 1;
    float costAlpha = alpha;
    if (state.itemChoice && m_selectedItem == static_cast<int>(storeIndex)) { costAlpha *= 0.25f; }
    m_movies.Text(cost, x - m_movies.TextWidth(cost, 0) / 2, costY, 0, 1, 0, costAlpha);
    // The native BUY button is placed from sprite bounds + original font height.
    const auto *buy = OriginalMenuData("MDS_BUTTON_POWERUP_SELECTOR_BUY", 0);
    const unsigned buttonMovie = m_movies.Ordinal(buy->movies[0]);
    unsigned start = 0, end = 0;
    MovieRegion buttonBounds;
    if (!m_movies.GetMovie(buttonMovie)->GetChapterRange(1, start, end) || !m_movies.Region(buttonMovie, 1, end, buttonBounds)) { return false; }
    auto action = SurvivalHudAction::None;
    if (!state.itemChoice) { action = SurvivalHudAction::BuyItem; }
    if (!DrawSelectorButton("MDS_BUTTON_POWERUP_SELECTOR_BUY", 0, float(x),
        y + bounds.height + int(bounds.height) / 6 + bounds.y + int(m_movies.TextHeight(0)) / 2 + buttonBounds.height / 2,
        costAlpha, action, static_cast<int>(storeIndex))) { return false; }
    if (!state.itemChoice && available && count > 0) {
        // PowerUpSelect :186013 restricts the original control region to icon height.
        MovieRegion hit = area;
        hit.y = float(y - int(bounds.height) / 2);
        hit.height = bounds.height;
        m_selectorHits.push_back({hit, SurvivalHudAction::SelectItem, static_cast<int>(storeIndex)});
    }
    return true;
}

bool SurvivalHud::DrawOriginalSelector(const SurvivalHudState &state) {
    if (!m_selectorBound) {
        m_selectorBound = true;
        m_selectorTime = 0;
        m_selectorChoiceTime = 0;
        // Show :186506, SetBoundsOptions :140797: two options before/after focus.
        m_selectorPosition = std::min(2.0f, std::max(0.0f, float(m_selectorEntries.size()) - 3));
        m_selectorTarget = m_selectorPosition;
        m_selectorChoice = false;
    }
    if (state.itemChoice != m_selectorChoice) { m_selectorChoiceTime = 0; m_selectorChoice = state.itemChoice; }
    m_selectorHits.clear();
    const unsigned menu = m_movies.Ordinal("GLU_MOVIE_POWERUP_MENU_NEW");
    const unsigned layout = m_movies.Ordinal("GLU_MOVIE_POWER_UP_LAYOUT");
    unsigned menuStart = 0, menuEnd = 0, idleStart = 0, idleEnd = 0, layoutStart = 0, layoutEnd = 0;
    if (!m_movies.GetMovie(menu)->GetChapterRange(0, menuStart, menuEnd) ||
        !m_movies.GetMovie(menu)->GetChapterRange(2, idleStart, idleEnd) ||
        !m_movies.GetMovie(layout)->GetChapterRange(1, layoutStart, layoutEnd)) { return false; }
    unsigned time = menuStart + m_selectorTime;
    if (time > idleEnd) { time = idleStart + (m_selectorTime - idleStart) % (idleEnd - idleStart + 1); }
    const bool ready = m_selectorTime >= std::max(idleStart, layoutStart);
    unsigned layoutTime = layoutStart;
    if (m_selectorTime < layoutStart) { layoutTime = m_selectorTime; }
    else { layoutTime += static_cast<unsigned>((m_selectorPosition - std::floor(m_selectorPosition)) * (layoutEnd - layoutStart + 1)); }
    const int base = static_cast<int>(std::floor(m_selectorPosition)) - 3;
    class Content : public IMovieRegionCallback {
    public:
        Content(SurvivalHud &owner, const SurvivalHudState &value, int offset) : hud(owner), state(value), base(offset) {}
        bool DrawMovieRegion(const MovieRegion &area) override {
            if (area.type < 2) { return true; }
            const int index = base + static_cast<int>(area.type) - 2;
            if (index < 0 || index >= static_cast<int>(hud.m_selectorEntries.size())) { return true; }
            return hud.DrawSelectorItem(state, static_cast<unsigned>(index), area);
        }
        SurvivalHud &hud; const SurvivalHudState &state; int base;
    } content(*this, state, base);
    class Panel : public IMovieRegionCallback {
    public:
        Panel(SurvivalHud &owner, const SurvivalHudState &value, unsigned movie, unsigned time, IMovieRegionCallback &items) :
            hud(owner), state(value), layout(movie), layoutTime(time), content(items) {}
        bool DrawMovieRegion(const MovieRegion &area) override {
            if (area.index == 1) {
                return hud.m_movies.Draw(layout, layoutTime, area.x + int(area.width) / 2,
                    area.y + int(area.height) / 2, 1024, 768, 0, area.alpha, &content);
            }
            if (area.index == 3) {
                const auto coins = hud.SelectorCurrency("IDS_SHOP_COMMON", state.coins);
                const auto bucks = hud.SelectorCurrency("IDS_SHOP_RARE", state.warbucks);
                const float y = area.y + int(area.height) / 2 - int(hud.m_movies.TextHeight(0)) / 2;
                hud.m_movies.Text(coins, area.x, y, 0, 1, 0, area.alpha);
                // Native UTF-32 string at VA0x3C4A50 is "88", a measured currency gap.
                hud.m_movies.Text(bucks, area.x + hud.m_movies.TextWidth(coins, 0) + hud.m_movies.TextWidth("88", 0), y, 0, 1, 0, area.alpha);
            }
            return true;
        }
        SurvivalHud &hud; const SurvivalHudState &state; unsigned layout, layoutTime; IMovieRegionCallback &content;
    } panel(*this, state, layout, layoutTime, content);
    if (!m_movies.Draw(menu, time, 512, 384, 1024, 768, 0, 1, &panel)) { return false; }
    MovieRegion resume;
    if (!m_movies.Region(menu, 0, time, resume)) { return false; }
    unsigned cancelIndex = 0;
    auto cancelAction = SurvivalHudAction::CloseShop;
    if (state.itemChoice) { cancelIndex = 1; cancelAction = SurvivalHudAction::CancelItem; }
    if (!DrawSelectorButton("MDS_BUTTON_POWERUP_SELECTOR", cancelIndex,
        resume.x + resume.width / 2, resume.y + resume.height / 2, resume.alpha, cancelAction)) { return false; }
    if (state.itemChoice) {
        const auto *selected = SelectedItem();
        if (selected == nullptr) { return false; }
        const auto &reference = selected->data.objects.front().object;
        const PowerupEntry *powerup = nullptr;
        for (const auto &item : m_powerups) {
            if (item.resource.packHash == reference.packHash && item.resource.localIndex == reference.localIndex) { powerup = &item; break; }
        }
        if (powerup == nullptr) { return false; }
        CPowerup query;
        query.Bind(powerup->data, state.powerupStatus);
        const bool equip = query.Query(0);
        const bool use = query.Query(1) && query.Query(2);
        const unsigned movie = m_movies.Ordinal("GLU_MOVIE_POWERUP_MENU_NEW_COPY");
        // This movie has no chapter track; CMovie::Update runs its complete timeline.
        const unsigned end = m_movies.GetMovie(movie)->duration;
        const unsigned choiceTime = std::min(m_selectorChoiceTime, end);
        class Choices : public IMovieRegionCallback {
        public:
            Choices(SurvivalHud &owner, bool equipable, bool usable) : hud(owner), equip(equipable), use(usable) {}
            bool DrawMovieRegion(const MovieRegion &area) override {
                if (area.index == 0 || area.index == 1) {
                    const auto *entry = OriginalMenuData("MDS_BUTTON_POWERUP_SELECTOR", 2);
                    unsigned sprite = entry->sprites[0];
                    if (!equip) { sprite = entry->sprites[1]; }
                    MovieRegion bounds;
                    if (!hud.m_movies.SpriteBounds(sprite >> 16, sprite & 255, bounds)) { return false; }
                    const float x = area.x + area.width / 2, y = area.y + area.height / 2;
                    hud.m_movies.DrawSprite(sprite >> 16, sprite & 255, 0, x - bounds.width / 2, y - bounds.height / 2, 1, area.alpha);
                    const auto text = hud.m_movies.NamedString(entry->strings[0]);
                    hud.m_movies.Text(text, x - hud.m_movies.TextWidth(text, 5) / 2, y - hud.m_movies.TextHeight(5) / 2, 5, 1, 0, area.alpha);
                }
                if (area.index == 5) {
                    const auto *entry = OriginalMenuData("MDS_BUTTON_POWERUP_SELECTOR", 3);
                    const float x = area.x + area.width / 2, y = area.y + area.height / 2;
                    float alpha = area.alpha;
                    if (!use) {
                        const unsigned sprite = entry->sprites[0];
                        hud.m_movies.DrawSprite(sprite >> 16, sprite & 255, 0, x, y, 1, alpha);
                        alpha *= 0.5f;
                    }
                    const auto lines = FormatStoreText(hud.m_movies, hud.m_movies.NamedString(entry->strings[0]), 100, {5,5,5,5,5});
                    const float height = lines.size() * hud.m_movies.TextHeight(5);
                    float lineY = y - height / 2;
                    for (const auto &line : lines) {
                        float lineX = x - 50;
                        for (const auto &run : line.runs) {
                            hud.m_movies.Text(run.text, lineX, lineY, run.font, 1, 0, alpha);
                            lineX += run.width;
                        }
                        lineY += hud.m_movies.TextHeight(5);
                    }
                }
                return true;
            }
            SurvivalHud &hud; bool equip, use;
        } choices(*this, equip, use);
        if (!m_movies.Draw(movie, choiceTime, 512, 384, 1024, 768, 0, 1, &choices)) { return false; }
        if (choiceTime >= end) {
            for (const auto &area : m_movies.Regions(movie, choiceTime, 512, 384, true)) {
                if (area.index == 3 && equip) { m_selectorHits.push_back({area, SurvivalHudAction::EquipLeft}); }
                if (area.index == 4 && equip) { m_selectorHits.push_back({area, SurvivalHudAction::EquipRight}); }
                if (area.index == 5 && use) { m_selectorHits.push_back({area, SurvivalHudAction::UseNow}); }
            }
        }
    }
    if (!ready) { m_selectorHits.clear(); }
    return DrawSelectorPrompt() && m_movies.Failures() == 0;
}

void SurvivalHud::ReportSelectorPurchase(PurchaseResult result, const SurvivalHudState &state) {
    if (result == PurchaseResult::Purchased) { return; }
    m_selectorPrompt = CMenuPopupPrompt{};
    m_selectorPromptRequested = true;
    m_selectorPromptTable = "MDS_STORE_PROMPT_UNAVAILABLE";
    m_selectorPromptBody.clear();
    m_selectorPromptFunds = result == PurchaseResult::InsufficientCoins || result == PurchaseResult::InsufficientWarbucks;
    if (!m_selectorPromptFunds) { return; }
    m_selectorPromptTable = "MDS_STORE_PROMPT_MOMONEY_INGAME";
    const auto *entry = OriginalMenuData(m_selectorPromptTable, 0);
    m_selectorPromptBody = m_movies.NamedString(entry->strings[0]);
    const auto *selected = SelectedItem();
    if (selected == nullptr) { return; }
    const char *currency = "IDS_SHOP_COMMON";
    unsigned price = selected->data.commonPrice;
    std::uint64_t balance = state.coins;
    if (result == PurchaseResult::InsufficientWarbucks) { currency = "IDS_SHOP_RARE"; price = selected->data.rarePrice; balance = state.warbucks; }
    unsigned missing = 0;
    if (price > balance) { missing = static_cast<unsigned>(price - balance); }
    // GetLastFailPurchaseInfo :156610 formats the total and missing amounts.
    for (unsigned amount : {price, missing}) {
        const auto marker = m_selectorPromptBody.find("%s");
        if (marker == std::string::npos) { break; }
        m_selectorPromptBody.replace(marker, 2, SelectorCurrency(currency, amount));
    }
}

bool SurvivalHud::DrawSelectorPrompt() {
    m_selectorPromptHits.clear();
    if (!m_selectorPromptRequested && !m_selectorPrompt.IsActive()) { return true; }
    m_selectorHits.clear();
    const auto *entry = OriginalMenuData(m_selectorPromptTable, 0);
    if (entry == nullptr) { return false; }
    const unsigned movie = m_movies.Ordinal("GLU_MOVIE_POPUP");
    unsigned start = 0, end = 0, expanded = 0, expandedEnd = 0;
    MovieRegion compact, large, visual;
    if (!m_movies.GetMovie(movie)->GetChapterRange(1, start, end) || !m_movies.GetMovie(movie)->GetChapterRange(2, expanded, expandedEnd) ||
        !m_movies.Region(movie, 1, start, compact) || !m_movies.Region(movie, 1, expanded, large)) { return false; }
    const bool hasVisual = entry->sprites[0] != UINT32_MAX;
    if (hasVisual && !m_movies.SpriteBounds(entry->sprites[0] >> 16, entry->sprites[0] & 255, visual)) { return false; }
    std::string body = m_selectorPromptBody;
    if (body.empty()) { body = m_movies.NamedString(entry->strings[0]); }
    const auto lines = FormatStoreText(m_movies, body, compact.width, {0,1,0,0,0});
    const float titleHeight = m_movies.TextHeight(0) + int(m_movies.TextHeight(0)) / 2;
    float visualHeight = 0;
    if (hasVisual) { visualHeight = visual.height + (int(m_movies.TextHeight(0)) & ~1); }
    float contentHeight = titleHeight + visualHeight;
    for (const auto &line : lines) { contentHeight += line.height; }
    if (m_selectorPromptRequested) {
        if (!m_selectorPrompt.Bind(*m_movies.GetMovie(movie), compact.height, large.height, contentHeight)) { return false; }
        m_selectorPromptRequested = false;
    }
    const unsigned time = m_selectorPrompt.MovieTime();
    if (!m_movies.Draw(movie, time)) { return false; }
    MovieRegion area;
    if (!m_movies.Region(movie, 1, time, area)) { return true; }
    const float alpha = area.alpha * m_selectorPrompt.ContentAlpha();
    const auto title = m_movies.NamedString(entry->strings[1]);
    m_movies.Text(title, area.x + (area.width - m_movies.TextWidth(title, 0)) / 2, area.y, 0, 1, 0, alpha);
    float y = area.y + titleHeight + visualHeight;
    for (const auto &line : lines) {
        const float x = area.x + (area.width - line.width) / 2;
        for (const auto &run : line.runs) { m_movies.Text(run.text, x + run.x, y + (line.height - run.height) / 2, run.font, 1, 0, alpha); }
        y += line.height;
    }
    if (hasVisual && m_selectorPrompt.IsReady()) {
        m_movies.DrawSprite(entry->sprites[0] >> 16, entry->sprites[0] & 255, 0,
            area.x + area.width / 2, area.y + titleHeight + visualHeight / 2, 1, alpha);
    }
    if (m_selectorPromptFunds) {
        for (unsigned index = 1; index <= 2; ++index) {
            MovieRegion slot;
            if (!m_movies.Region(movie, index + 2, time, slot)) { continue; }
            MovieRegion touch;
            if (!DrawSelectorButton("MDS_BUTTON_STORE_INGAME_PROMPT", index, slot.x + slot.width / 2,
                slot.y + slot.height / 2, alpha, SurvivalHudAction::None, -1, &touch)) { return false; }
            if (m_selectorPrompt.IsReady()) {
                m_selectorPromptHits.push_back({touch, OriginalMenuData("MDS_BUTTON_STORE_INGAME_PROMPT", index)->action});
            }
        }
    } else {
        MovieRegion slot, touch;
        if (!m_movies.Region(movie, 2, time, slot) || !m_movies.Region(movie, 0, time, touch)) { return true; }
        const auto *dismiss = OriginalMenuData("MDS_BUTTON_POPUP_PROMPT", 0);
        const auto text = m_movies.NamedString(dismiss->strings[1]);
        m_movies.Text(text, slot.x + (slot.width - m_movies.TextWidth(text, 5)) / 2, slot.y + slot.height / 2, 5, 1, 0, alpha);
        if (m_selectorPrompt.IsReady()) { m_selectorPromptHits.push_back({touch, dismiss->action}); }
    }
    return true;
}

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
    const auto directory = std::filesystem::path("out/ui-original-2026-09-09") / ("selector-profile-" + std::to_string(window.GetTicksMs()));
    if (!LoadNativeProfile(toc, tables, profile, directory, "saves")) { return 1; }
    CPlayerProgress progress;
    progress.Bind(profile.nativeArchive->progression);
    progress.SetExperience(profile.experience);
    SurvivalHudState state;
    state.originalUi = true; state.shopOpen = true; state.paused = true;
    state.coins = profile.coins; state.warbucks = profile.warbucks;
    state.inventory = profile.powerups;
    unsigned failures = 0, purchases = 0, iconHits = 0, choices = 0;
    if (!hud.DrawOriginalSelector(state) || !hud.m_selectorHits.empty()) { ++failures; }
    for (const char *name : {"GLU_MOVIE_POWERUP_MENU_NEW", "GLU_MOVIE_POWER_UP_LAYOUT", "GLU_MOVIE_POWERUP_MENU_NEW_COPY"}) {
        const auto *movie = hud.m_movies.GetMovie(hud.m_movies.Ordinal(name));
        std::printf("[selector-check] %s duration=%u chapters=", name, movie->duration);
        for (unsigned chapter : movie->chapters) { std::printf("%u,", chapter); }
        std::printf("\n");
    }
    hud.AdvanceMenu(2000);
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
            const auto action = hud.Pointer(state, x, y, true);
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
            if (hud.Pointer(state, x, y, true) != SurvivalHudAction::SelectItem) { ++failures; break; }
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
            if (!window.SaveFrame("out/ui-original-2026-09-09/selector-original-" + std::to_string(index) + ".png")) { ++failures; }
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
        if (!window.SaveFrame("out/ui-original-2026-09-09/selector-prompt-" + std::to_string(unsigned(result)) + ".png")) { ++failures; }
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
