/** @file CPowerUpSelector.cpp
 * @brief CPowerUpSelector single-player adapter; original source :184203-187670.
 * ui_movie.bt / powerup_template.bt / store_entry.bt describe the BIG inputs.
 */
#define NOMINMAX
#include "gun_bros_re/ui/hud/CPowerUpSelector.h"
#include "gun_bros_re/gameplay/multiplayer/bot/ZLocalPVPBot.h"
#include "gun_bros_re/ui/content/CMenuDataProvider.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
#include "engine/platform/ZWindow.h"
#include "engine/resources/CResTOCManager.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

std::string CPowerUpSelector::SelectorCurrency(const char *name, std::uint64_t amount) {
    std::string text = m_resources.m_movies.NamedString(name);
    const auto marker = text.find("%i");
    if (marker == std::string::npos) {
        std::printf("[powerup-selector] unsupported currency format %s\n", name);
        return text;
    }
    text.replace(marker, 2, std::to_string(static_cast<unsigned>(amount)));
    return text;
}

bool CPowerUpSelector::DrawSelectorButton(const char *table, unsigned index, float x, float y, float alpha,
    ZInputPadAction action, int storeIndex, ZMovieRegion *touch) {
    const auto *entry = CMenuDataProvider::Find(table, index);
    if (entry == nullptr) { return false; }
    const unsigned movie = m_resources.m_movies.Ordinal(entry->movies[0]);
    unsigned start = 0, end = 0;
    if (!m_resources.m_movies.GetMovie(movie)->GetChapterRange(1, start, end)) { return false; }
    // CMenuMovieButton::Draw consumes the original button size, without fitting.
    ZMovieRegion graphic;
    if (!m_resources.m_movies.Region(movie, 1, end, graphic)) { return false; }
    const float originX = x - graphic.x - graphic.width / 2 + 512;
    const float originY = y - graphic.y - graphic.height / 2 + 384;
    class Callback : public ZMovieRegionCallback {
    public:
        Callback(CPowerUpSelector &owner, const CMenuDataProvider::Entry &data) : hud(owner), entry(data) {}
        bool DrawMovieRegion(const ZMovieRegion &area) override {
            if (area.index != 1) { return true; }
            const unsigned sprite = entry.sprites[0];
            if (sprite != UINT32_MAX && !hud.m_resources.m_movies.DrawSprite(sprite >> 16, sprite & 255, 0,
                area.x, area.y, 1, area.alpha)) { return false; }
            const auto text = hud.m_resources.m_movies.NamedString(entry.strings[0]);
            return hud.m_resources.m_movies.Text(text, area.x + (area.width - hud.m_resources.m_movies.TextWidth(text, 5)) / 2,
                area.y + (area.height - hud.m_resources.m_movies.TextHeight(5)) / 2, 5, 1, 0, area.alpha);
        }
        CPowerUpSelector &hud;
        const CMenuDataProvider::Entry &entry;
    } callback(*this, *entry);
    if (!m_resources.m_movies.Draw(movie, end, originX, originY, 1024, 768, 0, alpha, &callback)) { return false; }
    for (auto area : m_resources.m_movies.Regions(movie, end, originX, originY, true)) {
        if (area.index == 0 && touch != nullptr) { *touch = area; }
        if (area.index == 0 && action != ZInputPadAction::None) {
            m_selectorHits.push_back({area, action, storeIndex});
        }
    }
    return true;
}

bool CPowerUpSelector::DrawSelectorItem(const ZInputPadState &state, unsigned index, const ZMovieRegion &area) {
    const unsigned storeIndex = m_selectorEntries[index];
    const auto &store = m_resources.m_store[storeIndex];
    const auto &reference = store.data.objects.front().object;
    const ZPowerupEntry *powerup = nullptr;
    for (const auto &item : m_resources.m_powerups) {
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
    auto &renderer = *m_resources.m_powerupRenderers.at(sprite.packHash);
    ZMovieRegion bounds;
    if (!renderer.SpriteBounds(sprite.archetype, sprite.animation, bounds)) { return false; }
    // PowerUpControlCallback :185179 centers the unscaled sprite, then raises it by height/4.
    const int x = int(area.x - bounds.x) + int(area.width - bounds.width) / 2;
    const int y = int(area.y - bounds.y) + int(area.height - bounds.height) / 2 - int(bounds.height) / 4;
    if (!renderer.DrawSprite(sprite.archetype, sprite.animation, 0, float(x), float(y), 1, alpha)) { return false; }
    const bool available = (powerup->data.field112 != 0) == state.afterDeathShop;
    if (!available && !renderer.DrawSprite(sprite.archetype, powerup->data.field28, 0, float(x), float(y), 1, alpha)) { return false; }
    if (count > 0) {
        unsigned animation = 87;
        if (count > 9) { animation = 88; }
        const int badgeX = x - int(bounds.height) / 8 + int(bounds.x + bounds.width);
        const int badgeY = y + int(bounds.height) / 8 + int(bounds.y);
        if (!m_resources.m_movies.DrawSprite(0, animation, 0, float(badgeX), float(badgeY), 1, alpha)) { return false; }
        const auto text = std::to_string(count);
        m_resources.m_movies.Text(text, badgeX - int(m_resources.m_movies.TextWidth(text, 0)) / 2,
            badgeY - int(m_resources.m_movies.TextHeight(0)) / 2, 0, 1, 0, alpha);
    }
    // Original 26-wide-character name buffer and font8, not the STORE thumbnail.
    // Retail names in this selector are ASCII and fit the original buffer.
    const auto &name = powerup->name;
    int topOffset = -1;
    if (area.height < 101) { topOffset = -4; }
    m_resources.m_movies.Text(name, area.x + (area.width - m_resources.m_movies.TextWidth(name, 8)) / 2,
        area.y + topOffset + int(m_resources.m_movies.TextHeight(8)) / 2, 8, 1, 0, alpha);
    const char *currency = "IDS_SHOP_COMMON";
    unsigned price = store.data.commonPrice;
    if (price == 0) { price = store.data.rarePrice; currency = "IDS_SHOP_RARE"; }
    const auto cost = SelectorCurrency(currency, price);
    const float costY = y + bounds.height + bounds.y + 1;
    float costAlpha = alpha;
    if (state.itemChoice && m_selectedItem == static_cast<int>(storeIndex)) { costAlpha *= 0.25f; }
    m_resources.m_movies.Text(cost, x - m_resources.m_movies.TextWidth(cost, 0) / 2, costY, 0, 1, 0, costAlpha);
    // The native BUY button is placed from sprite bounds + original font height.
    const auto *buy = CMenuDataProvider::Find("MDS_BUTTON_POWERUP_SELECTOR_BUY", 0);
    const unsigned buttonMovie = m_resources.m_movies.Ordinal(buy->movies[0]);
    unsigned start = 0, end = 0;
    ZMovieRegion buttonBounds;
    if (!m_resources.m_movies.GetMovie(buttonMovie)->GetChapterRange(1, start, end) || !m_resources.m_movies.Region(buttonMovie, 1, end, buttonBounds)) { return false; }
    auto action = ZInputPadAction::None;
    if (!state.itemChoice) { action = ZInputPadAction::BuyItem; }
    if (!DrawSelectorButton("MDS_BUTTON_POWERUP_SELECTOR_BUY", 0, float(x),
        y + bounds.height + int(bounds.height) / 6 + bounds.y + int(m_resources.m_movies.TextHeight(0)) / 2 + buttonBounds.height / 2,
        costAlpha, action, static_cast<int>(storeIndex))) { return false; }
    if (!state.itemChoice && available && count > 0) {
        // PowerUpSelect :186013 restricts the original control region to icon height.
        ZMovieRegion hit = area;
        hit.y = float(y - int(bounds.height) / 2);
        hit.height = bounds.height;
        m_selectorHits.push_back({hit, ZInputPadAction::SelectItem, static_cast<int>(storeIndex)});
    }
    return true;
}

bool CPowerUpSelector::DrawSelector(const ZInputPadState &state) {
    if (!m_presentingPowerup) { m_presentationSnapshot = state; }
    if (!m_selectorBound) {
        m_selectorEntries.clear();
        unsigned gameType = 0;
        if (state.localLive) { gameType = 1; }
        if (state.deathmatch) { gameType = 2; }
        for (unsigned storeIndex : m_selectorAllEntries) {
            // The STORE exclusion bits also drive the front-end mode labels.
            if (m_resources.m_store[storeIndex].data.IsExcludedFromGameType(gameType)) { continue; }
            const auto &ref = m_resources.m_store[storeIndex].data.objects.front().object;
            if (state.deathmatch && state.remoteShop &&
                !ZLocalPVPBot::IsGrenadePowerup(ref) && !ZLocalPVPBot::IsHealthPowerup(ref)) { continue; }
            for (const auto &powerup : m_resources.m_powerups) {
                if (powerup.resource.packHash == ref.packHash && powerup.resource.localIndex == ref.localIndex &&
                    (powerup.data.field112 != 0) == state.afterDeathShop) { m_selectorEntries.push_back(storeIndex); }
            }
        }
        m_selectorBound = true;
        m_selectorTime = 0;
        m_selectorChoiceTime = 0;
        // Show :186506, SetBoundsOptions :140797: two options before/after focus.
        m_selectorPosition = std::min(2.0f, std::max(0.0f, float(m_selectorEntries.size()) - 3));
        m_selectorTarget = m_selectorPosition;
        m_selectorChoice = false;
        m_matchSelectedSlot = 0;
        // CPowerUpSelector::Show :186531 always enters SetState(0), POWER UPS.
        // Correction: state 0 starts the opening animation. Show's mode at
        // mem+3800 selects GUNS (1) or POWER UPS (0) when state 2 is reached.
        m_matchGuns = state.deathmatch && m_selectorStartOnGuns;
        m_selectorStartOnGuns = false;
        m_matchSlotChapter = 1;
        m_matchSlotTime = 0;
    }
    if (state.itemChoice != m_selectorChoice) { m_selectorChoiceTime = 0; m_selectorChoice = state.itemChoice; }
    m_selectorHits.clear();
    const unsigned menu = m_resources.m_movies.Ordinal("GLU_MOVIE_POWERUP_MENU_NEW");
    const unsigned layout = m_resources.m_movies.Ordinal("GLU_MOVIE_POWER_UP_LAYOUT");
    unsigned menuStart = 0, menuEnd = 0, idleStart = 0, idleEnd = 0, layoutStart = 0, layoutEnd = 0;
    if (!m_resources.m_movies.GetMovie(menu)->GetChapterRange(0, menuStart, menuEnd) ||
        !m_resources.m_movies.GetMovie(menu)->GetChapterRange(2, idleStart, idleEnd) ||
        !m_resources.m_movies.GetMovie(layout)->GetChapterRange(1, layoutStart, layoutEnd)) { return false; }
    unsigned time = menuStart + m_selectorTime;
    if (time > idleEnd) { time = idleStart + (m_selectorTime - idleStart) % (idleEnd - idleStart + 1); }
    if (m_presentingPowerup) { time = m_powerupMenu.GetTime(); }
    const bool ready = m_selectorTime >= std::max(idleStart, layoutStart);
    unsigned layoutTime = layoutStart;
    if (m_selectorTime < layoutStart) { layoutTime = m_selectorTime; }
    else { layoutTime += static_cast<unsigned>((m_selectorPosition - std::floor(m_selectorPosition)) * (layoutEnd - layoutStart + 1)); }
    if (m_presentingPowerup && m_presentationState != 0 && m_powerupItems.IsBound()) { layoutTime = m_powerupItems.GetTime(); }
    const int base = static_cast<int>(std::floor(m_selectorPosition)) - 3;
    class Content : public ZMovieRegionCallback {
    public:
        Content(CPowerUpSelector &owner, const ZInputPadState &value, int offset) : hud(owner), state(value), base(offset) {}
        bool DrawMovieRegion(const ZMovieRegion &area) override {
            if (hud.m_presentingPowerup && !hud.m_powerupItemsVisible) { return true; }
            if (area.type < 2) { return true; }
            const int index = base + static_cast<int>(area.type) - 2;
            if (index < 0 || index >= static_cast<int>(hud.m_selectorEntries.size())) { return true; }
            return hud.DrawSelectorItem(state, static_cast<unsigned>(index), area);
        }
        CPowerUpSelector &hud; const ZInputPadState &state; int base;
    } content(*this, state, base);
    class Panel : public ZMovieRegionCallback {
    public:
        Panel(CPowerUpSelector &owner, const ZInputPadState &value, unsigned movie, unsigned time,
            unsigned panelTime, ZMovieRegionCallback &items) :
            hud(owner), state(value), layout(movie), layoutTime(time), menuTime(panelTime), content(items) {}
        bool DrawMovieRegion(const ZMovieRegion &area) override {
            if (area.index == 2 && (state.localLive || state.deathmatch)) {
                if (state.deathmatch && state.shopRemainingMs == 0) { return true; }
                // DrawPlayerNameAndTimer :185362 binds the original region 2.
                const std::string seconds = std::to_string((state.shopRemainingMs + 500) / 1000);
                float centerX = area.x + area.width / 2;
                if (state.remoteShop) {
                    const float nameWidth = hud.m_resources.m_movies.TextWidth(state.brotherName, 5);
                    if (!hud.m_resources.m_movies.Text(state.brotherName, centerX - nameWidth / 2,
                        area.y + (area.height - hud.m_resources.m_movies.TextHeight(5)) / 2, 5, 1, 0, area.alpha)) { return false; }
                    // ARM 0x106980..0x106AB4: name edge + one font11 zero glyph.
                    centerX += nameWidth / 2 + hud.m_resources.m_movies.TextWidth("0", 11);
                }
                return hud.m_resources.m_movies.Text(seconds, centerX - hud.m_resources.m_movies.TextWidth(seconds, 11) / 2,
                    area.y + (area.height - hud.m_resources.m_movies.TextHeight(11)) / 2, 11, 1, 0, area.alpha);
            }
            if (area.index == 1) {
                if (state.deathmatch && hud.m_matchGuns) { return hud.DrawMatchGuns(state, area); }
                return hud.m_resources.m_movies.Draw(layout, layoutTime, area.x + int(area.width) / 2,
                    area.y + int(area.height) / 2, 1024, 768, 0, area.alpha, &content);
            }
            if (area.index == 4 && state.deathmatch) { return hud.DrawMatchTabs(area); }
            if (area.index == 3) {
                const auto coins = hud.SelectorCurrency("IDS_SHOP_COMMON", state.coins);
                const auto bucks = hud.SelectorCurrency("IDS_SHOP_RARE", state.warbucks);
                // Native UTF-32 string at VA0x3C4A50 is "88", a measured currency gap.
                const float gap = hud.m_resources.m_movies.TextWidth("88", 0);
                const float coinsWidth = hud.m_resources.m_movies.TextWidth(coins, 0);
                const float totalWidth = coinsWidth + gap + hud.m_resources.m_movies.TextWidth(bucks, 0);
                // Windows overflow adaptation: keep long native-save balances
                // inside the BIG currency slot and clear of the original button.
                // Normal balances retain DrawPlayerCurrency's font and origin.
                const unsigned menu = hud.m_resources.m_movies.Ordinal("GLU_MOVIE_POWERUP_MENU_NEW");
                ZMovieRegion cancel, button;
                unsigned cancelIndex = 0;
                if (state.itemChoice) { cancelIndex = 1; }
                const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_POWERUP_SELECTOR", cancelIndex);
                if (entry == nullptr) { return false; }
                const unsigned buttonMovie = hud.m_resources.m_movies.Ordinal(entry->movies[0]);
                unsigned start = 0, end = 0;
                if (!hud.m_resources.m_movies.Region(menu, 0, menuTime, cancel) ||
                    !hud.m_resources.m_movies.GetMovie(buttonMovie)->GetChapterRange(1, start, end) ||
                    !hud.m_resources.m_movies.Region(buttonMovie, 1, end, button)) { return false; }
                float right = area.x + area.width;
                if (cancel.y < area.y + area.height && cancel.y + cancel.height > area.y) {
                    right = std::min(right, cancel.x + cancel.width / 2 - button.width / 2);
                }
                float scale = 1;
                const float available = std::max(0.0f, right - area.x);
                if (totalWidth + gap > available) { scale = available / (totalWidth + gap); }
                const float y = area.y + area.height / 2 - hud.m_resources.m_movies.TextHeight(0, scale) / 2;
                hud.m_resources.m_movies.Text(coins, area.x, y, 0, scale, 0, area.alpha);
                hud.m_resources.m_movies.Text(bucks, area.x + (coinsWidth + gap) * scale, y, 0, scale, 0, area.alpha);
            }
            return true;
        }
        CPowerUpSelector &hud; const ZInputPadState &state; unsigned layout, layoutTime, menuTime; ZMovieRegionCallback &content;
    } panel(*this, state, layout, layoutTime, time, content);
    if (!m_resources.m_movies.Draw(menu, time, 512, 384, 1024, 768, 0, 1, &panel)) { return false; }
    if (m_presentingPowerup) { m_selectorHits.clear(); return m_resources.m_movies.Failures() == 0; }
    ZMovieRegion resume;
    if (!m_resources.m_movies.Region(menu, 0, time, resume)) { return false; }
    unsigned cancelIndex = 0;
    auto cancelAction = ZInputPadAction::CloseShop;
    if (state.itemChoice) { cancelIndex = 1; cancelAction = ZInputPadAction::CancelItem; }
    if (!state.remoteShop && !DrawSelectorButton("MDS_BUTTON_POWERUP_SELECTOR", cancelIndex,
        resume.x + resume.width / 2, resume.y + resume.height / 2, resume.alpha, cancelAction)) { return false; }
    if (state.itemChoice) {
        const auto *selected = SelectedItem();
        if (selected == nullptr) { return false; }
        const auto &reference = selected->data.objects.front().object;
        const ZPowerupEntry *powerup = nullptr;
        for (const auto &item : m_resources.m_powerups) {
            if (item.resource.packHash == reference.packHash && item.resource.localIndex == reference.localIndex) { powerup = &item; break; }
        }
        if (powerup == nullptr) { return false; }
        CPowerup query;
        query.SetDeathmatch(state.deathmatch);
        query.Bind(powerup->data, state.powerupStatus);
        const bool equip = query.Query(0);
        int remaining = 0;
        const auto cooldown = state.powerupCooldowns.find(reference.localIndex);
        if (state.deathmatch && cooldown != state.powerupCooldowns.end()) { remaining = cooldown->second; }
        const bool use = remaining == 0 && query.Query(1) && query.Query(2);
        const unsigned movie = m_resources.m_movies.Ordinal("GLU_MOVIE_POWERUP_MENU_NEW_COPY");
        // This movie has no chapter track; CMovie::Update runs its complete timeline.
        const unsigned end = m_resources.m_movies.GetMovie(movie)->duration;
        const unsigned choiceTime = std::min(m_selectorChoiceTime, end);
        class Choices : public ZMovieRegionCallback {
        public:
            Choices(CPowerUpSelector &owner, bool equipable, bool usable, const ZPowerupEntry &item, int cooldown)
                : hud(owner), equip(equipable), use(usable), powerup(item), remaining(cooldown) {}
            bool DrawMovieRegion(const ZMovieRegion &area) override {
                if (area.index == 0 || area.index == 1) {
                    const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_POWERUP_SELECTOR", 2);
                    unsigned sprite = entry->sprites[0];
                    if (!equip) { sprite = entry->sprites[1]; }
                    ZMovieRegion bounds;
                    if (!hud.m_resources.m_movies.SpriteBounds(sprite >> 16, sprite & 255, bounds)) { return false; }
                    const float x = area.x + area.width / 2, y = area.y + area.height / 2;
                    hud.m_resources.m_movies.DrawSprite(sprite >> 16, sprite & 255, 0, x - bounds.width / 2, y - bounds.height / 2, 1, area.alpha);
                    const auto text = hud.m_resources.m_movies.NamedString(entry->strings[0]);
                    hud.m_resources.m_movies.Text(text, x - hud.m_resources.m_movies.TextWidth(text, 5) / 2, y - hud.m_resources.m_movies.TextHeight(5) / 2, 5, 1, 0, area.alpha);
                }
                if (area.index == 5) {
                    if (remaining > 0) { return hud.DrawPowerupCooldown(powerup, remaining, area, 1); }
                    const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_POWERUP_SELECTOR", 3);
                    const float x = area.x + area.width / 2, y = area.y + area.height / 2;
                    float alpha = area.alpha;
                    if (!use) {
                        const unsigned sprite = entry->sprites[0];
                        hud.m_resources.m_movies.DrawSprite(sprite >> 16, sprite & 255, 0, x, y, 1, alpha);
                        alpha *= 0.5f;
                    }
                    const auto lines = CTextBox::Format(hud.m_resources.m_movies, hud.m_resources.m_movies.NamedString(entry->strings[0]), 100, {5,5,5,5,5});
                    const float height = lines.size() * hud.m_resources.m_movies.TextHeight(5);
                    float lineY = y - height / 2;
                    for (const auto &line : lines) {
                        // Bind :187108 sets CTextBox's center flag; paint
                        // :104153 centers each line within the 100-unit box.
                        const float lineX = x - line.width / 2;
                        for (const auto &run : line.runs) {
                            hud.m_resources.m_movies.Text(run.text, lineX + run.x, lineY, run.font, 1, 0, alpha);
                        }
                        lineY += hud.m_resources.m_movies.TextHeight(5);
                    }
                }
                return true;
            }
            CPowerUpSelector &hud; bool equip, use; const ZPowerupEntry &powerup; int remaining;
        } choices(*this, equip, use, *powerup, remaining);
        if (!m_resources.m_movies.Draw(movie, choiceTime, 512, 384, 1024, 768, 0, 1, &choices)) { return false; }
        if (choiceTime >= end) {
            for (const auto &area : m_resources.m_movies.Regions(movie, choiceTime, 512, 384, true)) {
                if (area.index == 3 && equip) { m_selectorHits.push_back({area, ZInputPadAction::EquipLeft}); }
                if (area.index == 4 && equip) { m_selectorHits.push_back({area, ZInputPadAction::EquipRight}); }
                if (area.index == 5 && use) { m_selectorHits.push_back({area, ZInputPadAction::UseNow}); }
            }
        }
    }
    if (!ready || state.remoteShop) { m_selectorHits.clear(); }
    return DrawSelectorPrompt() && m_resources.m_movies.Failures() == 0;
}

void CPowerUpSelector::ReportSelectorPurchase(ZPurchaseResult result, const ZInputPadState &state) {
    if (result == ZPurchaseResult::Purchased) { return; }
    m_selectorPrompt = CMenuPopupPrompt{};
    m_selectorPromptRequested = true;
    m_selectorPromptTable = "MDS_STORE_PROMPT_UNAVAILABLE";
    m_selectorPromptBody.clear();
    m_selectorPromptFunds = result == ZPurchaseResult::InsufficientCoins || result == ZPurchaseResult::InsufficientWarbucks;
    if (!m_selectorPromptFunds) { return; }
    m_selectorPromptTable = "MDS_STORE_PROMPT_MOMONEY_INGAME";
    const auto *entry = CMenuDataProvider::Find(m_selectorPromptTable, 0);
    m_selectorPromptBody = m_resources.m_movies.NamedString(entry->strings[0]);
    const auto *selected = SelectedItem();
    if (selected == nullptr) { return; }
    const char *currency = "IDS_SHOP_COMMON";
    unsigned price = selected->data.commonPrice;
    std::uint64_t balance = state.coins;
    if (result == ZPurchaseResult::InsufficientWarbucks) { currency = "IDS_SHOP_RARE"; price = selected->data.rarePrice; balance = state.warbucks; }
    unsigned missing = 0;
    if (price > balance) { missing = static_cast<unsigned>(price - balance); }
    // GetLastFailPurchaseInfo :156610 formats the total and missing amounts.
    for (unsigned amount : {price, missing}) {
        const auto marker = m_selectorPromptBody.find("%s");
        if (marker == std::string::npos) { break; }
        m_selectorPromptBody.replace(marker, 2, SelectorCurrency(currency, amount));
    }
}

bool CPowerUpSelector::DrawSelectorPrompt() {
    m_selectorPromptHits.clear();
    if (!m_selectorPromptRequested && !m_selectorPrompt.IsActive()) { return true; }
    m_selectorHits.clear();
    const auto *entry = CMenuDataProvider::Find(m_selectorPromptTable, 0);
    if (entry == nullptr) { return false; }
    const unsigned movie = m_resources.m_movies.Ordinal("GLU_MOVIE_POPUP");
    unsigned start = 0, end = 0, expanded = 0, expandedEnd = 0;
    ZMovieRegion compact, large, visual;
    if (!m_resources.m_movies.GetMovie(movie)->GetChapterRange(1, start, end) || !m_resources.m_movies.GetMovie(movie)->GetChapterRange(2, expanded, expandedEnd) ||
        !m_resources.m_movies.Region(movie, 1, start, compact) || !m_resources.m_movies.Region(movie, 1, expanded, large)) { return false; }
    const bool hasVisual = entry->sprites[0] != UINT32_MAX;
    if (hasVisual && !m_resources.m_movies.SpriteBounds(entry->sprites[0] >> 16, entry->sprites[0] & 255, visual)) { return false; }
    std::string body = m_selectorPromptBody;
    if (body.empty()) { body = m_resources.m_movies.NamedString(entry->strings[0]); }
    const auto lines = CTextBox::Format(m_resources.m_movies, body, compact.width, {0,1,0,0,0});
    const float titleHeight = m_resources.m_movies.TextHeight(0) + int(m_resources.m_movies.TextHeight(0)) / 2;
    float visualHeight = 0;
    if (hasVisual) { visualHeight = visual.height + (int(m_resources.m_movies.TextHeight(0)) & ~1); }
    float contentHeight = titleHeight + visualHeight;
    for (const auto &line : lines) { contentHeight += line.height; }
    if (m_selectorPromptRequested) {
        if (!m_selectorPrompt.Bind(*m_resources.m_movies.GetMovie(movie), compact.height, large.height, contentHeight)) { return false; }
        m_selectorPromptRequested = false;
    }
    const unsigned time = m_selectorPrompt.MovieTime();
    if (!m_resources.m_movies.Draw(movie, time)) { return false; }
    ZMovieRegion area;
    if (!m_resources.m_movies.Region(movie, 1, time, area)) { return true; }
    const float alpha = area.alpha * m_selectorPrompt.ContentAlpha();
    const auto title = m_resources.m_movies.NamedString(entry->strings[1]);
    m_resources.m_movies.Text(title, area.x + (area.width - m_resources.m_movies.TextWidth(title, 0)) / 2, area.y, 0, 1, 0, alpha);
    float y = area.y + titleHeight + visualHeight;
    for (const auto &line : lines) {
        const float x = area.x + (area.width - line.width) / 2;
        for (const auto &run : line.runs) { m_resources.m_movies.Text(run.text, x + run.x, y + (line.height - run.height) / 2, run.font, 1, 0, alpha); }
        y += line.height;
    }
    if (hasVisual && m_selectorPrompt.IsReady()) {
        m_resources.m_movies.DrawSprite(entry->sprites[0] >> 16, entry->sprites[0] & 255, 0,
            area.x + area.width / 2, area.y + titleHeight + visualHeight / 2, 1, alpha);
    }
    if (m_selectorPromptFunds) {
        for (unsigned index = 1; index <= 2; ++index) {
            ZMovieRegion slot;
            if (!m_resources.m_movies.Region(movie, index + 2, time, slot)) { continue; }
            ZMovieRegion touch;
            if (!DrawSelectorButton("MDS_BUTTON_STORE_INGAME_PROMPT", index, slot.x + slot.width / 2,
                slot.y + slot.height / 2, alpha, ZInputPadAction::None, -1, &touch)) { return false; }
            if (m_selectorPrompt.IsReady()) {
                m_selectorPromptHits.push_back({touch, CMenuDataProvider::Find("MDS_BUTTON_STORE_INGAME_PROMPT", index)->action});
            }
        }
    } else {
        ZMovieRegion slot, touch;
        if (!m_resources.m_movies.Region(movie, 2, time, slot) || !m_resources.m_movies.Region(movie, 0, time, touch)) { return true; }
        const auto *dismiss = CMenuDataProvider::Find("MDS_BUTTON_POPUP_PROMPT", 0);
        const auto text = m_resources.m_movies.NamedString(dismiss->strings[1]);
        m_resources.m_movies.Text(text, slot.x + (slot.width - m_resources.m_movies.TextWidth(text, 5)) / 2, slot.y + slot.height / 2, 5, 1, 0, alpha);
        if (m_selectorPrompt.IsReady()) { m_selectorPromptHits.push_back({touch, dismiss->action}); }
    }
    return true;
}

void CPowerUpSelector::BrowseRemoteShop(unsigned selection) {
    if (m_selectorEntries.empty()) { return; }
    // The local peer drives the same resource list's focus, not a second UI.
    const unsigned index = selection % static_cast<unsigned>(m_selectorEntries.size());
    const float maximum = std::max(0.0f, float(m_selectorEntries.size()) - 3);
    m_selectorTarget = std::clamp(static_cast<float>(index), std::min(2.0f, maximum), maximum);
    m_selectedItem = static_cast<int>(m_selectorEntries[index]);
}

const ZStoreEntry *CPowerUpSelector::SelectedItem() const {
    if (m_selectedItem < 0 || m_selectedItem >= static_cast<int>(m_resources.m_store.size())) { return nullptr; }
    return &m_resources.m_store[m_selectedItem];
}

bool CPowerUpSelector::BackFromSelectorPrompt() {
    if (!m_selectorPromptRequested && !m_selectorPrompt.IsActive()) { return false; }
    m_selectorPromptRequested = false;
    m_selectorPrompt.Hide();
    return true;
}

bool CPowerUpSelector::DrawPowerupCooldown(const ZPowerupEntry &entry, int remaining, const ZMovieRegion &region, float scale) {
    // CPowerUpSelector::DrawCoolDownTimer :184092, CInputPad sprite 1:94.
    std::vector<unsigned> times;
    if (!m_resources.m_movies.SpriteFrameTimes(1, 94, times) || times.size() < 12 || entry.data.field124 == 0) { return false; }
    const unsigned tail = 4 * (times[11] - times[10]);
    unsigned frame = 0;
    if (remaining >= static_cast<int>(tail)) {
        frame = static_cast<unsigned>(std::max(0.0f, (1 - float(remaining) / (entry.data.field124 * 1000)) * 9));
    } else if (tail > 0) { frame = 9 + static_cast<unsigned>((1 - float(remaining) / tail) * 4); }
    frame = std::min(frame, static_cast<unsigned>(times.size() - 1));
    return m_resources.m_movies.DrawSprite(1, 94, times[frame], region.x + region.width / 2, region.y + region.height / 2, scale, region.alpha);
}

ZInputPadAction CPowerUpSelector::Pointer(const ZInputPadState &state, float x, float y, bool down, bool previousDown) {
    bool clicked = down && !previousDown;
    if (state.shopOpen && !state.itemChoice && !m_selectorPromptRequested && !m_selectorPrompt.IsActive()) {
        // Commit a list tap on release so dragging an icon cannot equip it.
        if (clicked) {
            m_selectorPressX = x;
            m_selectorPressY = y;
            m_selectorDragged = false;
            m_selectorPressArmed = false;
            for (const auto &hit : m_selectorHits) {
                if (hit.area.Contains(x, y) && (hit.action == ZInputPadAction::SelectItem || hit.action == ZInputPadAction::BuyItem || hit.action == ZInputPadAction::SelectMatchGun)) {
                    m_selectorPressArmed = true;
                    break;
                }
            }
        }
        if (previousDown && std::hypot(x - m_selectorPressX, y - m_selectorPressY) > 8) {
            m_selectorDragged = true;
        }
        if (m_selectorPressArmed) { clicked = !down && previousDown && !m_selectorDragged; }
    }
    if (!clicked) { return ZInputPadAction::None; }
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
            return ZInputPadAction::None;
        }
        for (const auto &hit : m_selectorHits) {
            if (!hit.area.Contains(x, y)) { continue; }
            if (hit.action == ZInputPadAction::ShowGuns || hit.action == ZInputPadAction::ShowPowerups) {
                m_matchGuns = hit.action == ZInputPadAction::ShowGuns;
                return ZInputPadAction::None;
            }
            if (hit.action == ZInputPadAction::MatchSlot1 || hit.action == ZInputPadAction::MatchSlot2) {
                m_matchSelectedSlot = 0;
                if (hit.action == ZInputPadAction::MatchSlot2) { m_matchSelectedSlot = 1; }
                m_matchSlotChapter = 6;
                if (m_matchSelectedSlot == 1) { m_matchSlotChapter = 3; }
                m_matchSlotTime = 0;
                return ZInputPadAction::None;
            }
            if (hit.storeIndex >= 0) { m_selectedItem = hit.storeIndex; }
            return hit.action;
        }
        return ZInputPadAction::None;
    }
    return ZInputPadAction::None;
}

void CPowerUpSelector::InitEntries() {
    // CStoreAggregator::InitFilteredList :159210 and SortFilteredList :155433.
    std::vector<std::pair<int, unsigned>> selectorOrder;
    for (unsigned index = 0; index < m_resources.m_store.size(); ++index) {
        const auto &item = m_resources.m_store[index].data;
        if (item.type < 10 || item.type > 13 || item.displayOrder < 0 || item.value242 == 1 ||
            item.objects.empty() || item.objects.front().type != 17) { continue; }
        selectorOrder.push_back({item.displayOrder, index});
    }
    std::sort(selectorOrder.begin(), selectorOrder.end());
    for (const auto &item : selectorOrder) { m_selectorEntries.push_back(item.second); }
    m_selectorAllEntries = m_selectorEntries;
}

void CPowerUpSelector::Scroll(const ZInputPadState &state, float amount) {
    if (state.remoteShop || amount == 0) { return; }
    if (state.shopOpen) {
        if (state.deathmatch && m_matchGuns) {
            m_matchGunPosition = std::clamp(m_matchGunPosition - amount, 0.0f, std::max(0.0f, float(m_matchGunEntries.size()) - 4));
            return;
        }
        if (!state.itemChoice) {
            const float maximum = std::max(0.0f, float(m_selectorEntries.size()) - 3);
            m_selectorTarget = std::clamp(m_selectorTarget - amount, std::min(2.0f, maximum), maximum);
        }
        return;
    }
}

void CPowerUpSelector::ScrollInput(const ZInputPadState &state, float wheel, float dragX) {
    if (state.remoteShop) { return; }
    if (state.shopOpen) {
        Scroll(state, wheel);
        if (state.itemChoice || m_selectorPromptRequested || m_selectorPrompt.IsActive() || dragX == 0) { return; }
        const char *layoutName = "GLU_MOVIE_POWER_UP_LAYOUT";
        if (state.deathmatch && m_matchGuns) { layoutName = "GLU_MOVIE_GUN_LAYOUT"; }
        const unsigned layout = m_resources.m_movies.Ordinal(layoutName);
        unsigned start = 0, end = 0;
        ZMovieRegion first, second;
        if (!m_resources.m_movies.GetMovie(layout)->GetChapterRange(1, start, end) ||
            !m_resources.m_movies.Region(layout, 2, start, first) || !m_resources.m_movies.Region(layout, 3, start, second)) { return; }
        const float spacing = std::abs(second.x + second.width / 2 - first.x - first.width / 2);
        if (spacing <= 0) { return; }
        // Drag input is in the same 1024-wide logical canvas as the BIG layout.
        // Follow the pointer immediately, leaving fractional positions at rest.
        m_selectorTarget = m_selectorPosition;
        Scroll(state, dragX / spacing);
        m_selectorPosition = m_selectorTarget;
        return;
    }
}

void CPowerUpSelector::Update(unsigned deltaMs) {
    if (!m_selectorBound) { return; }
    m_selectorTime += deltaMs;
    m_selectorChoiceTime += deltaMs;
    m_matchSlotTime += deltaMs;
    m_selectorPrompt.Update(deltaMs);
    unsigned start = 0, end = 0;
    if (m_resources.m_movies.GetMovie(m_resources.m_movies.Ordinal("GLU_MOVIE_POWER_UP_LAYOUT"))->GetChapterRange(1, start, end)) {
        const float step = float(deltaMs) / (end - start + 1);
        if (m_selectorPosition < m_selectorTarget) { m_selectorPosition = std::min(m_selectorPosition + step, m_selectorTarget); }
        else { m_selectorPosition = std::max(m_selectorPosition - step, m_selectorTarget); }
    }
}

bool CPowerUpSelector::FindActionRegion(ZInputPadAction action, ZMovieRegion &region) const {

        for (const auto &hit : m_selectorHits) {
            if (hit.action == action) { region = hit.area; return true; }
        }
        return false;

}
