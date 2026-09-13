#include "gun_bros_re/ui/ModeOverlayCallbacks.h"
#include "gun_bros_re/ui/StoreRegionClip.h"
#include "gun_bros_re/ui/MenuInternal.h"
namespace MenuDetail {

// Page callback implementations.

bool MatchesEquipmentSlot(const StoreEntry &entry, unsigned slot,
    const std::vector<WeaponEntry> &weapons, const std::vector<ArmorEntry> &armors) {
    if (slot == 5) {
        if (entry.data.objects.empty() || entry.data.type >= 14) { return false; }
        // Zero-price consumable records are reward payloads (e.g. Bro-op
        // grenade prize), not a repeatable store purchase.
        if (entry.data.commonPrice == 0 && entry.data.rarePrice == 0) { return false; }
        for (const GameObjectTypeRef &object : entry.data.objects) {
            if (object.type != 17 || !IsPlayablePowerup(object.object)) { return false; }
        }
        return true;
    }
    if (entry.data.objects.size() != 1) { return false; }
    const GameObjectTypeRef &ref = entry.data.objects[0];
    if (slot < 2) {
        if (ref.type != 6) { return false; }
        for (const WeaponEntry &weapon : weapons) {
            if (weapon.packHash == ref.object.packHash && weapon.ordinal == ref.object.localIndex) {
                return !weapon.visualOnly && weapon.hasStoreEntry;
            }
        }
    } else {
        if (ref.type != 2) { return false; }
        for (const ArmorEntry &armor : armors) {
            if (armor.packHash == ref.object.packHash && armor.ordinal == ref.object.localIndex) {
                return armor.data.GetSlot() == kArmorSlots[slot];
            }
        }
    }
    return false;
}

GameObjectRef &Equipped(CProfileManager &profile, unsigned slot) {
    if (slot < 2) { return profile.configuration.guns[slot]; }
    return profile.configuration.armor[kArmorSlots[slot]];
}

/** Shared compact/expanded store status, distinct from the preview slot. */
bool IsStoreObjectEquipped(CProfileManager &profile, unsigned slot, const GameObjectRef &object) {
    // CStoreAggregator::GetItemStatus :155316 requests IsGunEquipped(..., -1).
    if (slot < 2) { return profile.configuration.IsGunEquipped(object) >= 0; }
    return slot < 5 && SameObject(Equipped(profile, slot), object);
}

/** A layout region must exist; a miss means the movie or chapter is wrong. */
bool RequireRegion(GameMenu &view, unsigned movie, unsigned index, unsigned time, MovieRegion &region, const char *what) {
    if (view.movies.Region(movie, index, time, region)) { return true; }
    std::printf("[store] missing region %u of movie %u at %u ms (%s)\n", index, movie, time, what);
    return false;
}

/** SHOP_BOX regions resolved for a card whose own origin sits at (x, y). */
bool CardRegion(GameMenu &view, unsigned card, unsigned index, const StoreCardFace &face, MovieRegion &region) {
    for (const MovieRegion &candidate : view.movies.Regions(card, face.time, face.x, face.y)) {
        if (candidate.index != index) { continue; }
        region = candidate;
        return true;
    }
    return false;
}

// Every menu button prints its label at the same size; the original never
// squeezes one to fit a narrower plate, it picks a wider plate instead.

/** Centre one original label inside a plate. */
void PlateLabel(GameMenu &view, const std::string &label, float x, float y, float width, float height) {
    view.movies.Text(label, x + (width - view.movies.TextWidth(label, 5)) * 0.5f,
        y + (height - view.movies.TextHeight(5)) * 0.5f, 5, 1);
}

/** Draw one MDS_BUTTON_STORE_ITEMS plate. Its width is the width of the button
 * movie that row names, right aligned on `right`: BUY and EQUIP take the small
 * plate, UPGRADE the large one, which is why UPGRADE reaches further left. */
bool StoreItemButton(GameMenu &view, unsigned entryIndex, float right, float y, float height, bool enabled, float alpha ) {
    const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", entryIndex);
    if (entry == nullptr) { return false; }
    MovieRegion label;
    if (!view.movies.Region(view.movies.Ordinal(entry->movies[0]), 1, 0, label)) {
        std::printf("[store] missing button region: %s\n", entry->movies[0]);
        return false;
    }
    const float width = label.width;
    height = label.height;
    const float x = right - width;
    const unsigned sprite = entry->sprites[0];
    view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, x, y, width, height, alpha);
    view.movies.Text(view.movies.NamedString(entry->strings[0]),
        x + (width - view.movies.TextWidth(view.movies.NamedString(entry->strings[0]), 5)) / 2,
        y + (height - view.movies.TextHeight(5)) / 2, 5, 1, 0, alpha);
    if (!enabled || !view.Hit(x, y, width, height)) { return false; }
    view.NotePress(view.movies.Ordinal(entry->movies[0]), x, y, width, height);
    return true;
}

/** Right aligned price: the original prints the currency sprite and the number,
 * never the currency's name. */
// CreateItemCostString :157573 resolves IDS_SHOP_COMMON/RARE (Omega/delta
// glyph plus %i in this BIG). The currency icons are glyphs in font 0; drawing
// an unrelated sprite at 1.3 times the row height duplicated their layout.
std::string StoreCostText(GameMenu &view, const CStoreItem &item) {
    if (item.commonPrice == 0 && item.rarePrice == 0) { return view.movies.NamedString("IDS_SHOP_FREE"); }
    std::string text = view.movies.NamedString("IDS_SHOP_COMMON");
    unsigned amount = item.commonPrice;
    if (amount == 0) {
        text = view.movies.NamedString("IDS_SHOP_RARE");
        amount = item.rarePrice;
    }
    const std::size_t placeholder = text.find("%i");
    if (placeholder == std::string::npos) {
        std::printf("[store] unsupported currency format: %s\n", text.c_str());
        return text;
    }
    text.replace(placeholder, 2, std::to_string(amount));
    return text;
}

void DrawCardPrice(GameMenu &view, const CStoreItem &item, const MovieRegion &row, float alpha) {
    const std::string text = StoreCostText(view, item);
    view.movies.Text(text, row.x + row.width - view.movies.TextWidth(text, 0), row.y, 0, 1, 0, alpha);
}

/** A bundle is owned once every object it hands over is. Only the records with
 * the single-purchase flag are treated this way. */
bool OwnsBundle(const CProfileManager &profile, const CStoreItem &item) {
    // The historical inventory approximation below does not apply to a
    // single-purchase package: CPackageOfferMgr keeps an independent key.
    if (item.singlePurchase != 0) { return profile.IsPackagePurchased(item.resource); }
    for (const GameObjectTypeRef &object : item.objects) {
        if (!profile.Owns(object.type, object.object)) { return false; }
    }
    return !item.objects.empty();
}

const WeaponEntry *FindWeaponEntry(const std::vector<WeaponEntry> &weapons, const GameObjectRef &ref) {
    for (const WeaponEntry &weapon : weapons) {
        if (weapon.packHash == ref.packHash && weapon.ordinal == ref.localIndex) { return &weapon; }
    }
    return nullptr;
}

/** Category caption under the icon, from the weapon or armour catalogue. */
// CreateItemCategoryString :157413 indexes IDS_SHOP_SORT3 + STORE.category.
std::string StoreItemKind(GameMenu &view, const StoreEntry &item) {
    if (item.data.type > 15) { return {}; }
    return view.movies.NamedString("IDS_SHOP_SORT3", item.data.type);
}

/** The three cell upgrade meter. CMenuStore::Load pulls sprite character 26
 * for exactly this movie, so the store shares the upgrade popup's artwork. */
// The former popup-meter implementation was replaced by the store child movie below.

/** Store display strings are templates: `^fN` marks a font switch and `#KEY` a
 * value slot. The `^fN` code indexes a font table this rebuild has not resolved
 * yet; against the iOS card, labels use the blue menu font and values the blue
 * digit font. Pieces wrap inside their own card region and each line is centred
 * there, which is what stacks POWER above its number. */
// Correction to the historical approximation above: the table is now resolved.
// CMenuStoreOptionGroup::InitOption :233898 -> SetFont :181499 ->
// SetupTextBox :181194 maps ^f0..4 to menu fonts 1,2,4,3,0. CTextBox::paint
// :104153 centers when byte 0 is set; byte 1, not byte 0, means right alignment.
// Use real newlines, font metrics and whitespace; never invent a row after #KEY.

std::string SubstituteStoreStats(const std::string &text,
    const std::vector<std::pair<std::string, std::string>> &values) {
    std::string result;
    for (std::size_t position = 0; position < text.size();) {
        if (text[position] != '#') { result += text[position++]; continue; }
        std::size_t end = position + 1;
        while (end < text.size() && std::isalpha(static_cast<unsigned char>(text[end]))) { ++end; }
        const std::string key = text.substr(position + 1, end - position - 1);
        bool found = false;
        for (const auto &value : values) {
            if (value.first != key) { continue; }
            result += value.second;
            found = true;
            break;
        }
        // Keep an unresolved token visible for diagnosis instead of deleting it.
        if (!found) { result += text.substr(position, end - position); }
        position = end;
    }
    return result;
}

void DrawStoreTemplate(GameMenu &view, const std::string &text, const MovieRegion &area,
    const std::vector<std::pair<std::string, std::string>> &values, bool centered , float layoutWidth ) {
    if (text.empty() || area.alpha <= 0 || area.width <= 0 || area.height <= 0) { return; }
    if (layoutWidth <= 0) { layoutWidth = area.width; }
    const auto lines = FormatStoreText(view.movies, SubstituteStoreStats(text, values), layoutWidth);
    StoreRegionClip clip(view, area);
    float y = area.y;
    for (const StoreTextLine &line : lines) {
        float x = area.x;
        if (centered) { x += (layoutWidth - line.width) / 2; }
        for (const StoreTextRun &run : line.runs) {
            // CTextBox::paint :104170 centers mixed fonts within the line height.
            view.movies.Text(run.text, x + run.x, y + (line.height - run.height) / 2, run.font, 1, 0, area.alpha);
        }
        y += line.height;
    }
}

/** CMenuDataProvider::CreateContentMovie :149246 and CMenuStoreOption::Bind
 * :181883 bind the store-specific eight-region movie, not the upgrade popup.
 * Child time is derived from CGun mastery XP and that movie's chapter lengths. */
bool StoreMasteryTarget(const CMovie &movie, const CGun::Template &weapon, unsigned experience, unsigned &target) {
    const unsigned level = weapon.GetMasteryLevel(experience);
    target = movie.duration;
    if (level < kMaxMasteryLevel) {
        unsigned lower = 0;
        if (level > 0) { lower = weapon.GetMasteryThreshold(level - 1); }
        const unsigned upper = weapon.GetMasteryThreshold(level);
        if (upper <= lower) { return false; }
        // GetElementValueInt32 :151132 scales each unfinished tier to 0..98,
        // using 99 before integer division; 100 advances the meter too far.
        const unsigned percent = static_cast<unsigned>((static_cast<std::uint64_t>(experience - lower) * 99) / (upper - lower));
        unsigned start = 0, end = 0;
        if (!movie.GetChapterRange(level + 1, start, end)) { return false; }
        target = start + percent * (end - start) / 100;
    }
    return true;
}

bool DrawMasteryMeter(GameMenu &view, const WeaponEntry &weapon, unsigned experience,
    const MovieRegion &area, unsigned elapsed) {
    if (area.alpha <= 0) { return true; }
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_MASTERY");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned target = 0;
    if (movie == nullptr || !StoreMasteryTarget(*movie, weapon.data, experience, target)) { return false; }
    // Focus resets this movie to chapter 0; Update :181490 advances it at 2x.
    const unsigned time = static_cast<unsigned>(std::min<std::uint64_t>(target, 2ull * elapsed));
    StoreRegionClip clip(view, area);
    if (!view.movies.Draw(ordinal, time, area.x, area.y, 1024, 768, 0, area.alpha)) { return false; }
    constexpr const char *captions[] = {
        "IDS_WEAPONMASTERY_STORE_TITLE", "IDS_WEAPONMASTERY_STORE_MASTERY_CRITICAL_CHANCE",
        "IDS_WEAPONMASTERY_STORE_MASTERY_CRITICAL_CHANCE_LOW",
        "IDS_WEAPONMASTERY_STORE_MASTERY_CRITICAL_CHANCE_MED",
        "IDS_WEAPONMASTERY_STORE_MASTERY_CRITICAL_CHANCE_HIGH"};
    for (const MovieRegion &region : view.movies.Regions(ordinal, time, area.x, area.y)) {
        if (region.index >= 2 && region.index <= 4) {
            // CreateContentSprite :150114, archetype 26, star animations 5/6/7.
            // The callback draws the initialized star frame; Update :181490
            // advances the child Movie, never these three CSpritePlayers.
            view.movies.DrawSprite(26, 5 + region.index - 2, 0, region.x + region.width / 2,
                region.y + region.height / 2, 1, area.alpha * region.alpha);
        } else {
            unsigned caption = region.index;
            if (region.index >= 5) { caption = region.index - 3; }
            if (caption >= 5) { return false; }
            const std::string text = view.movies.NamedString(captions[caption]);
            view.movies.Text(text, region.x + (region.width - view.movies.TextWidth(text, 1)) / 2,
                region.y, 1, 1, 0, area.alpha * region.alpha);
        }
    }
    return true;
}

/** Powerups use their own child movie; row locations and visibility live in BIG.
 * Bind :182003, GameTypeCallback :180652, GameTypeCompatibilityCallback :180688. */
bool DrawPowerupCompatibility(GameMenu &view, const CStoreItem &item, const MovieRegion &area, unsigned elapsed) {
    if (area.alpha <= 0) { return true; }
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_DEATHMATCH_ONLY_POWERUPS");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    if (movie == nullptr) { return false; }
    const unsigned time = std::min(elapsed, movie->duration);
    StoreRegionClip clip(view, area);
    if (!view.movies.Draw(ordinal, time, area.x, area.y, 1024, 768, 0, area.alpha)) { return false; }
    constexpr const char *labels[] = {"IDS_MULTIPLAYER_INACTIVE", "IDS_MULTIPLAYER_ACTIVE", "IDS_MULTIPLAYER_ACTIVE_VERSUS"};
    for (const MovieRegion &region : view.movies.Regions(ordinal, time, area.x, area.y)) {
        if (region.index < 1 || region.index > 6) { continue; }
        const unsigned mode = (region.index - 1) / 2;
        if ((region.index & 1) != 0) {
            const std::string text = view.movies.NamedString(labels[mode]);
            view.movies.Text(text, region.x + (region.width - view.movies.TextWidth(text, 1)) / 2,
                region.y, 1, 1, 0, area.alpha * region.alpha);
        } else {
            // CreateContentSprite :150197 uses STORE value8 exclusion bits.
            unsigned sprite = 174;
            if ((item.value8 & (1u << mode)) == 0) { sprite = 173; }
            view.movies.DrawSprite(0, sprite, elapsed, region.x + region.width / 2,
                region.y + region.height / 2, 1, area.alpha * region.alpha);
        }
    }
    return true;
}

/** The current mastery tier's values for the card templates. */
std::vector<std::pair<std::string, std::string>> StoreStatValues(const CStoreItem &item, std::size_t mastery) {
    // CStoreAggregator::SubstituteStatsInString :156816, STORE statGroups[0..7].
    constexpr const char *keys[] = {"POWER", "DMG", "RPM", "SPD", "DEF", "ATK", "COINS", "BUCKS"};
    std::vector<std::pair<std::string, std::string>> values;
    for (unsigned stat = 0; stat < 8; ++stat) {
        const auto &column = item.statGroups[stat];
        if (mastery >= column.size()) { continue; }
        const int value = column[mastery];
        std::int64_t magnitude = value;
        if (stat != 3 && magnitude < 0) { magnitude = -magnitude; }
        std::string text = std::to_string(magnitude);
        // The speed column is a percentage offset and keeps its sign.
        if (stat == 3 && value >= 0) { text = "+" + text; }
        values.push_back({keys[stat], text});
    }
    return values;
}

/** The four category tabs. Their widths come from the button movie each
 * MDS_BUTTON_STORE_CATEGORIES row names, not from measured screenshots. */
void DrawStoreCategories(GameMenu &view, const MovieRegion &bar, MenuState &state, bool interactive) {
    // The focus overlays belong to the same button movies: small 75, medium 76,
    // large 77 and extra large 78, in the same order as the movie ordinals.
    // The actual focus art is in each button's chapter 3 (:144639).
    float x = bar.x;
    for (unsigned category = 0; category < 4; ++category) {
        const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_STORE_CATEGORIES", category);
        if (entry == nullptr) { continue; }
        const unsigned plate = view.movies.Ordinal(entry->movies[0]);
        MovieRegion touch, label;
        if (!view.movies.Region(plate, 0, 0, touch) || !view.movies.Region(plate, 1, 0, label)) { continue; }
        const float width = label.width;
        const float touchX = x + touch.x - label.x;
        const float touchY = bar.y + touch.y - label.y;
        const bool selected = category == state.store.shopCategory;
        unsigned sprite = entry->sprites[1];
        // The bank tab keeps its green plate whether or not it is selected.
        if (selected || category == 3) { sprite = entry->sprites[0]; }
        view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, x, bar.y, width, bar.height);
        if (selected) {
            const CMovie *buttonMovie = view.movies.GetMovie(plate);
            unsigned focusStart = 0, focusEnd = 0;
            if (buttonMovie != nullptr && buttonMovie->GetChapterRange(3, focusStart, focusEnd)) {
                view.movies.DrawFitted(plate, focusEnd, x, bar.y, width, label.height, 1);
            }
        }
        PlateLabel(view, view.movies.NamedString(entry->strings[0]), x, bar.y, width, bar.height);
        if (interactive && view.Hit(touchX, touchY, touch.width, touch.height)) {
            view.NotePress(plate, x, bar.y, width, bar.height);
            {
                state.store.shopCategory = category;
                if (state.page == 17) { state.page = 2; }
                state.store.shopScroll = 0; state.store.shopMotion = MenuScrollMotion{};
                state.store.shopFilter = 0; state.store.shopExclusionFilter = 0;
                state.selectedItem = -1;
                state.store.shopDetailOpen = false;
            }
        }
        x += width + kCategoryGap;
    }
}

/** CMenuStore::InitSortButtons binds every row in the selected MDS table. */
unsigned StoreFilterRows(unsigned category, const char *&table) {
    table = "MDS_BUTTON_STORE_SORT_GUNS";
    if (category == 1) { table = "MDS_BUTTON_STORE_SORT_ARMOR"; }
    if (category == 2) { table = "MDS_BUTTON_STORE_SORT_POWERUP"; }
    if (category == 3) { table = "MDS_BUTTON_STORE_SORT_CURRENCY"; }
    unsigned count = 0;
    while (OriginalMenuData(table, count)) { ++count; }
    return count;
}

/** Non-looping SORT_BAR playback, CMenuStore::Bind :180092 and
 * HandleTouchInput :179259. Chapter 0 holds the initial closed pose; clicks
 * play chapter 1 forward or backward without restarting the current frame.
 * Bounds come from BIG ui_movie.bt / MovieChapter, never copied timestamps.
 * CMovie::Update :109097 advances milliseconds and clamps at chapter bounds. */
bool AdvanceStoreFilter(GameMenu &view, MenuState &state, const CMovie &movie) {
    unsigned closedStart = 0, closedEnd = 0, slideStart = 0, slideEnd = 0;
    if (!movie.GetChapterRange(0, closedStart, closedEnd) ||
        !movie.GetChapterRange(1, slideStart, slideEnd)) {
        std::printf("[store-filter] missing playback chapters\n");
        return false;
    }
    if (!state.store.shopFilterBound) {
        state.store.shopFilterTime = closedEnd;
        if (state.store.shopFilterOpen) { state.store.shopFilterTime = slideEnd; }
        state.store.shopFilterLastTick = view.clock;
        state.store.shopFilterBound = true;
        std::printf("[store-filter] BIG chapters closed=%u..%u slide=%u..%u\n",
            closedStart, closedEnd, slideStart, slideEnd);
        return true;
    }
    const std::uint64_t elapsed = view.clock - state.store.shopFilterLastTick;
    state.store.shopFilterLastTick = view.clock;
    if (state.store.shopFilterOpen) {
        state.store.shopFilterTime += static_cast<unsigned>(std::min<std::uint64_t>(elapsed, slideEnd - state.store.shopFilterTime));
    } else if (state.store.shopFilterTime > slideStart) {
        state.store.shopFilterTime -= static_cast<unsigned>(std::min<std::uint64_t>(elapsed, state.store.shopFilterTime - slideStart));
    }
    return true;
}

/** The original store draws two cards per column (ItemCallback :178878) on a
 * horizontal belt and expands the focused card in place. Item identity and
 * purchases still come directly from the BIG catalog. */
/** CornerCallback :180757 and CreateContentSprite :149994 display quantity
 * in a 0:87/88 badge, with the original numeric font; no handwritten OWN label. */
void DrawStoreQuantity(GameMenu &view, const CProfileManager &profile, const GameObjectTypeRef &ref,
    const MovieRegion &area, float alpha ) {
    if (ref.type != 17) { return; }
    const unsigned count = profile.GetPowerupCount(ref.object);
    unsigned sprite = 87;
    if (count >= 10) { sprite = 88; }
    const std::string text = std::to_string(count);
    view.movies.DrawSprite(0, sprite, 0, area.x + area.width / 2, area.y + area.height / 2, 1, alpha * area.alpha);
    view.movies.Text(text, area.x + (area.width - view.movies.TextWidth(text, 0)) / 2,
        area.y + (area.height - view.movies.TextHeight(0)) / 2, 0, 1, 0, alpha * area.alpha);
}

/** Focus/UnFocus :181356/:181402 reverse chapter 1; Update :181486 uses 4x.
 * Keep a closing card modal until its last frame, so a click cannot buy the card
 * underneath it. Bounds are read each time from CMovie, including resource edits. */
bool AdvanceStoreCard(GameMenu &view, MenuState &state, const CMovie &movie) {
    unsigned start = 0, end = 0;
    if (!movie.GetChapterRange(1, start, end)) { return false; }
    if (!state.store.shopDetailOpen) { return true; }
    std::uint64_t elapsed = 0;
    if (view.clock >= state.store.shopDetailLastTick) { elapsed = view.clock - state.store.shopDetailLastTick; }
    state.store.shopDetailLastTick = view.clock;
    const unsigned step = static_cast<unsigned>(std::min<std::uint64_t>(end - start, elapsed * kCardPlaybackRate));
    state.store.shopDetailTime = std::clamp(state.store.shopDetailTime, start, end);
    // CMenuStore::SetupFocusInterp :179101 uses 125 ms, an original code constant.
    const float movement = static_cast<float>(elapsed) / 125;
    if (state.store.shopDetailClosing) {
        state.store.shopDetailTime -= std::min(step, state.store.shopDetailTime - start);
        state.store.shopFocusAmount = std::max(0.0f, state.store.shopFocusAmount - movement);
        if (state.store.shopDetailTime == start && state.store.shopFocusAmount == 0) {
            state.store.shopDetailOpen = false;
            state.store.shopDetailClosing = false;
        }
    } else {
        state.store.shopDetailTime += std::min(step, end - state.store.shopDetailTime);
        state.store.shopFocusAmount = std::min(1.0f, state.store.shopFocusAmount + movement);
    }
    return true;
}

/** GetLastFailPurchaseInfo :156610; ARM 0xD25A8/0xD25F8 confirms the total
 * price and missing balance arguments omitted by the decompiler. */
bool StoreFailureText(GameMenu &view, const MenuState &state, std::string &body) {
    const char *currencyName = "IDS_SHOP_COMMON";
    if (state.failedCurrency == 1) { currencyName = "IDS_SHOP_RARE"; }
    const unsigned amounts[] = {state.failedPrice, state.failedMissing};
    for (unsigned amount : amounts) {
        std::string currency = view.movies.NamedString(currencyName);
        const auto number = currency.find("%i");
        const auto text = body.find("%s");
        if (number == std::string::npos || text == std::string::npos) { return false; }
        currency.replace(number, 2, std::to_string(amount));
        body.replace(text, 2, currency);
    }
    return body.find('%') == std::string::npos;
}

void ShowStoreFundsPrompt(MenuState &state, const std::vector<StoreEntry> &store,
    const CProfileManager &profile, unsigned currency, unsigned price, bool inGame ) {
    state.ShowStorePrompt("MDS_STORE_PROMPT_MOMONEY", false, false);
    state.storePromptButtons = "MDS_BUTTON_STORE_INGAME_PROMPT";
    if (!inGame) { state.storePromptButtons = "MDS_BUTTON_STORE_PROMPT"; }
    state.failedCurrency = currency;
    state.failedPrice = price;
    std::uint64_t balance = profile.coins;
    if (currency == 1) { balance = profile.warbucks; }
    state.failedMissing = 0;
    if (price > balance) { state.failedMissing = static_cast<unsigned>(price - balance); }
    state.currencyOffer = FindCurrencyOffer(store, currency, state.failedMissing);
    state.currencyOfferProduct.clear();
    if (state.currencyOffer >= 0) { state.currencyOfferProduct = store[state.currencyOffer].productId; }
}

/** CMenuStore::GunSwapCallback :178863; button size comes from Movie region 1. */
bool StoreGunSwapOrigin(GameMenu &view, const MovieRegion &parent, MovieRegion &origin) {
    const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_STORE_GUN_SWAP", 0);
    MovieRegion graphic;
    if (entry == nullptr || !view.movies.Region(view.movies.Ordinal(entry->movies[0]), 1, 0, graphic)) { return false; }
    origin = parent;
    origin.x = parent.x + parent.width - graphic.width;
    origin.y = parent.y + static_cast<int>(parent.height) / 2;
    return true;
}

/** Original button chapters own appearance, press completion and category hide. */
bool DrawStoreGunSwap(GameMenu &view, MenuState &state, const MovieRegion &parent, bool interactive) {
    const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_STORE_GUN_SWAP", 0);
    if (entry == nullptr) { return false; }
    const CMovie *movie = view.movies.GetMovie(view.movies.Ordinal(entry->movies[0]));
    unsigned showStart = 0, showEnd = 0, pressStart = 0, pressEnd = 0, idleStart = 0, idleEnd = 0;
    if (movie == nullptr || !movie->GetChapterRange(0, showStart, showEnd) ||
        !movie->GetChapterRange(1, pressStart, pressEnd) || !movie->GetChapterRange(2, idleStart, idleEnd)) { return false; }
    unsigned delta = 0;
    if (state.store.shopSwapLastTick != 0 && view.clock >= state.store.shopSwapLastTick) {
        delta = static_cast<unsigned>(view.clock - state.store.shopSwapLastTick);
    }
    state.store.shopSwapLastTick = view.clock;
    // CMenuStore::RefreshCategoryContent :179165 tests category, not filter bits.
    const bool visible = state.store.shopCategory == 0;
    if (visible && state.store.shopSwapPhase == 8) {
        state.store.shopSwapPhase = 0;
        state.store.shopSwapTime = showStart;
        delta = 0;
    } else if (!visible && state.store.shopSwapPhase != 1 && state.store.shopSwapPhase != 8) {
        state.store.shopSwapPhase = 1;
        state.store.shopSwapTime = showEnd;
        delta = 0;
    } else if (visible && state.store.shopSwapPhase == 1) {
        state.store.shopSwapPhase = 0;
        state.store.shopSwapTime = showStart;
        delta = 0;
    }
    if (state.store.shopSwapPhase == 0 || state.store.shopSwapPhase == 4) {
        unsigned end = showEnd;
        if (state.store.shopSwapPhase == 4) { end = pressEnd; }
        state.store.shopSwapTime = std::min(end, state.store.shopSwapTime + delta);
        if (state.store.shopSwapTime == end) {
            if (state.store.shopSwapPhase == 4) {
                // CMenuMovieButton::Update :144755 dispatches action 92 only
                // after chapter 1 finishes; PLAYER Flow then owns the swap.
                state.store.shopGunSlot = 1 - view.GetPlayerPreviewSlot();
                state.store.shopDetailOpen = false;
                state.store.shopPreview = false;
            }
            state.store.shopSwapPhase = 2;
            state.store.shopSwapTime = idleStart;
        }
    } else if (state.store.shopSwapPhase == 1) {
        if (delta >= state.store.shopSwapTime - showStart) { state.store.shopSwapPhase = 8; }
        else { state.store.shopSwapTime -= delta; }
    } else if (state.store.shopSwapPhase == 2) {
        state.store.shopSwapTime = idleStart + (state.store.shopSwapTime - idleStart + delta) % (idleEnd - idleStart + 1);
    }
    bool keyPressed = state.store.shopSwapKeyRequested;
    state.store.shopSwapKeyRequested = false;
    if (state.store.shopSwapPhase == 8) { return true; }
    MovieRegion origin;
    if (!StoreGunSwapOrigin(view, parent, origin)) { return false; }
    bool pressed = false;
    const bool enabled = interactive && state.store.shopSwapPhase == 2 && state.store.shopGunSlot == view.GetPlayerPreviewSlot();
    if (!DrawOriginalMovieButton(view, *entry, origin, std::to_string(view.GetPlayerPreviewSlot() + 1), 6,
        enabled, pressed, 0, 0, state.store.shopSwapTime)) { return false; }
    if (enabled && (pressed || keyPressed)) {
        state.store.shopSwapPhase = 4;
        state.store.shopSwapTime = pressStart;
    }
    return true;
}

bool CompleteOfflineIAP(std::uint64_t clock, MenuState &state, CProfileManager &profile,
    const std::vector<StoreEntry> &store, const std::filesystem::path &savePath) {
    if (!state.currencyPending) { return true; }
    int itemIndex = state.currencyItem;
    if (state.currencySimulated) {
        state.online.SetConnected(GameHostSettings().isConnected);
        state.online.UpdatePurchase(clock);
        const auto status = state.online.GetPurchaseState();
        if (status == LocalOnlineServices::PurchaseState::Cancelled) {
            state.currencyPending = false;
            state.online.FinishPurchase();
            state.ShowStorePrompt("MDS_STORE_PROMPT_UNAVAILABLE", false, true);
            std::printf("[local-iap] cancelled before delivery\n");
            return true;
        }
        if (status != LocalOnlineServices::PurchaseState::Completed) { return true; }
        // AcquireIAP :158280 resolves the callback product ID against the store,
        // independently of the selected card or the current catalog order.
        itemIndex = -1;
        for (unsigned index = 0; index < store.size(); ++index) {
            if (store[index].data.value32 == 1 && store[index].productId == state.online.GetProduct()) {
                itemIndex = static_cast<int>(index);
                break;
            }
        }
        state.online.FinishPurchase();
    } else if (clock < state.currencyReadyAt) { return true; }
    state.currencyPending = false;
    state.storePromptRequested = false;
    state.storePopup.Hide();
    if (itemIndex < 0 || itemIndex >= static_cast<int>(store.size())) {
        state.ShowStorePrompt("MDS_STORE_PROMPT_UNAVAILABLE", false, true);
        std::printf("[local-iap] product not found; no delivery\n");
        return true;
    }
    const auto previousCoins = profile.coins;
    const auto previousBucks = profile.warbucks;
    const PurchaseResult result = profile.AcquireCurrency(store[itemIndex].data);
    if (result == PurchaseResult::Purchased && !profile.SaveToDisk(savePath)) {
        profile.coins = previousCoins;
        profile.warbucks = previousBucks;
        return false;
    }
    if (result != PurchaseResult::Purchased) { state.ShowStorePrompt("MDS_STORE_PROMPT_UNAVAILABLE", false, true); }
    std::printf("[offline-iap] completed result=%u simulated-validation=%u\n", static_cast<unsigned>(result), state.currencySimulated);
    return true;
}

/** IAP is a standard modal prompt, layout mode 1 (visual left), no buttons.
 * CMenuSystem::ShowPopup :96455 selects fonts 0/0/1/5 and GLU_MOVIE_POPUP.
 * BindContent :207403 derives its target size from fonts and sprite bounds. */
bool DrawStorePrompt(GameMenu &view, MenuState &state) {
    if (!state.storePromptRequested && !state.storePopup.IsActive()) { return true; }
    const auto *entry = OriginalMenuData(state.storePromptTable, state.storePromptIndex);
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_POPUP");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned smallStart = 0, smallEnd = 0, largeStart = 0, largeEnd = 0;
    MovieRegion compactRegion, expandedRegion, visual;
    if (entry == nullptr || movie == nullptr || !movie->GetChapterRange(1, smallStart, smallEnd) ||
        !movie->GetChapterRange(2, largeStart, largeEnd) ||
        !view.movies.Region(ordinal, 1, smallStart, compactRegion) || !view.movies.Region(ordinal, 1, largeStart, expandedRegion)) { return false; }
    const bool hasVisual = entry->sprites[0] != UINT32_MAX;
    if (hasVisual && !view.movies.SpriteBounds(entry->sprites[0] >> 16, entry->sprites[0] & 255, visual)) { return false; }
    std::string title = view.movies.NamedString(entry->strings[1]);
    std::string body = view.movies.NamedString(entry->strings[0]);
    if (!state.challengeRewardTitle.empty()) { title = state.challengeRewardTitle; body = state.challengeRewardBody; }
    if (state.matchingPrompt) {
        // The original uses GKMatchmakerViewController (:260057), not BIG.
        // This labelled desktop adapter borrows only the native popup geometry.
        title = "LOCAL MATCHMAKING";
        body = "Waiting for another player.\nLocal simulation has no peer connected.";
    }
    if (!state.matchingPrompt && state.storePromptButtons != nullptr && !StoreFailureText(view, state, body)) { return false; }
    const float bodyHeight = view.movies.TextHeight(0);
    float titleSpace = view.movies.TextHeight(0) + bodyHeight;
    if (!state.storePromptSideVisual) { titleSpace = view.movies.TextHeight(0) + static_cast<unsigned>(bodyHeight) / 2; }
    // GetVisualContentBounds :207100 pads by the body font's even line height.
    float visualPadding = 0;
    if (hasVisual) { visualPadding = static_cast<float>(static_cast<unsigned>(bodyHeight) & ~1u); }
    const float visualWidth = visual.width + visualPadding;
    const float visualHeight = visual.height + visualPadding;
    float textWidth = compactRegion.width;
    if (state.storePromptSideVisual) { textWidth -= visualWidth; }
    const auto lines = FormatStoreText(view.movies, body, textWidth, {0, 1, 0, 0, 0});
    float textHeight = 0;
    for (const StoreTextLine &line : lines) { textHeight += line.height; }
    if (!state.storePopup.IsActive()) {
        float contentHeight = titleSpace + textHeight + visualHeight;
        if (state.storePromptSideVisual) { contentHeight = std::max(visualHeight, titleSpace + textHeight); }
        if (!state.storePopup.Bind(*movie, compactRegion.height, expandedRegion.height, contentHeight)) { return false; }
        state.storePromptRequested = false;
        state.storePopupLastTick = view.clock;
        state.storePromptSpriteTime = 0;
        std::printf("[iap-prompt] BIG target=%u compactRegion=%.0f expandedRegion=%.0f image=%.0fx%.0f body-lines=%zu\n",
            state.storePopup.TargetTime(), compactRegion.height, expandedRegion.height, visual.width, visual.height, lines.size());
    }
    unsigned delta = 0;
    if (view.clock >= state.storePopupLastTick) { delta = static_cast<unsigned>(view.clock - state.storePopupLastTick); }
    state.storePopupLastTick = view.clock;
    state.storePopup.Update(delta);
    if (!state.storePopup.IsActive()) { return true; }
    const unsigned time = state.storePopup.MovieTime();
    if (!view.movies.Draw(ordinal, time)) { return false; }
    MovieRegion area;
    if (!view.movies.Region(ordinal, 1, time, area)) { return true; }
    const float alpha = area.alpha * state.storePopup.ContentAlpha();
    {
        StoreRegionClip clip(view, area);
        view.movies.Text(title, area.x + (area.width - view.movies.TextWidth(title, 0)) / 2, area.y, 0, 1, 0, alpha);
        float y = area.y + titleSpace;
        if (!state.storePromptSideVisual) { y += visualHeight; }
        for (const StoreTextLine &line : lines) {
            float x = area.x + (area.width - line.width) / 2;
            if (state.storePromptSideVisual) { x = area.x + visualWidth; }
            for (const StoreTextRun &run : line.runs) {
                view.movies.Text(run.text, x + run.x, y + (line.height - run.height) / 2, run.font, 1, 0, alpha);
            }
            y += line.height;
        }
        if (hasVisual && time == state.storePopup.TargetTime()) {
            // Update :207249 advances the visual only after the container reaches
            // its size target; it must not inherit time spent opening the box.
            state.storePromptSpriteTime += delta;
            float visualX = area.x + visualWidth / 2;
            float visualY = area.y + area.height / 2;
            if (!state.storePromptSideVisual) {
                visualX = area.x + area.width / 2;
                visualY = area.y + titleSpace + visualHeight / 2;
            }
            if (!view.movies.DrawSprite(entry->sprites[0] >> 16, entry->sprites[0] & 255, state.storePromptSpriteTime,
                visualX, visualY, 1, alpha)) { return false; }
        }
    }
    if (state.storePromptDismiss) {
        MovieRegion dismissal;
        const auto *dismiss = OriginalMenuData("MDS_BUTTON_POPUP_PROMPT", 0);
        if (dismiss == nullptr) { return false; }
        if (!view.movies.Region(ordinal, 2, time, dismissal)) { return true; }
        const std::string text = view.movies.NamedString(dismiss->strings[1]);
        view.movies.Text(text, dismissal.x + (dismissal.width - view.movies.TextWidth(text, 5)) / 2,
            dismissal.y + dismissal.height / 2, 5, 1, 0, alpha);
        if (state.storePopup.IsReady()) {
            MovieRegion touch;
            if (!view.movies.Region(ordinal, 0, time, touch)) { return false; }
            const bool previousInput = view.inputEnabled;
            view.inputEnabled = true;
            if (view.Hit(touch.x, touch.y, touch.width, touch.height)) { state.storePopup.Hide(); }
            view.inputEnabled = previousInput;
        }
    }
    if (state.storePromptButtons != nullptr) {
        unsigned first = 1;
        if (std::strcmp(state.storePromptButtons, "MDS_BUTTON_STORE_PROMPT") == 0) { first = 0; }
        unsigned last = 2;
        if (state.matchingPrompt) { first = 0; last = 0; }
        for (unsigned index = first; index <= last; ++index) {
            MovieRegion region, buttonBounds, popupBounds;
            const auto *button = OriginalMenuData(state.storePromptButtons, index);
            if (button == nullptr) { return false; }
            if (!view.movies.Region(ordinal, index + 2, time, region)) { continue; }
            if (!view.movies.Region(view.movies.Ordinal(button->movies[0]), 0, 0, buttonBounds) ||
                !view.movies.Region(ordinal, 0, time, popupBounds)) { return false; }
            // ButtonCallback :206404 centers within the assigned region and
            // clamps to its edges if the button would cross the popup's bounds.
            MovieRegion placed = region;
            placed.x += region.width / 2 - buttonBounds.width / 2;
            if (placed.x < popupBounds.x) { placed.x = region.x; }
            else if (placed.x + buttonBounds.width > popupBounds.x + popupBounds.width) {
                placed.x = region.x + region.width - buttonBounds.width;
            }
            placed.y += region.height / 2 - buttonBounds.height / 2;
            placed.alpha *= state.storePopup.ContentAlpha();
            const bool previousInput = view.inputEnabled;
            view.inputEnabled = state.storePopup.IsReady();
            bool pressed = false;
            if (!DrawOriginalMovieButton(view, *button, placed, view.movies.NamedString(button->strings[0]), 5,
                state.storePopup.IsReady(), pressed)) { return false; }
            view.inputEnabled = previousInput;
            if (pressed) {
                if (button->action == 139 && state.matchingPrompt) {
                    state.online.CancelMatch();
                    state.storePopup.Hide();
                }
                if (button->action == 45) { state.storePopup.Hide(); }
                if (button->action == 70) {
                    state.storePopup.Hide();
                    std::printf("[store] Tapjoy offers unavailable on host; no reward issued\n");
                }
                if (button->action == 71 && state.currencyOffer >= 0) {
                    state.BeginOfflineIAP(state.currencyOffer, view.clock, state.currencyOfferProduct);
                }
                return true;
            }
        }
    }
    return true;
}

/** Currency entries have no object references and no cost string. The original
 * LevelCallback :180839 therefore places BUY/CONVERT in the bottom right.
 * Focus :181402 requires a cost string, so these cards do not expand. */
bool DrawCurrencyCard(GameMenu &view, CResTOCManager &toc, PackTables &tables,
    const StoreEntry &item, unsigned index, unsigned movie, const StoreCardFace &face,
    bool enabled, MenuState &state, CProfileManager &profile, const std::filesystem::path &savePath) {
    MovieRegion name, icon, kind, price;
    if (!CardRegion(view, movie, kCardNameRegion, face, name) ||
        !CardRegion(view, movie, kCardIconRegion, face, icon) ||
        !CardRegion(view, movie, kCardCategoryRegion, face, kind) ||
        !CardRegion(view, movie, kCardPriceRegion, face, price)) { return false; }
    view.movies.Draw(movie, face.time, face.x, face.y, kMenuWidth, kMenuHeight, 0, face.alpha);
    view.Icon(toc, tables, item, icon.x, icon.y, icon.width, icon.height, face.alpha * icon.alpha, true);
    view.movies.Text(item.name, name.x, name.y, 1, 1, 0, face.alpha * name.alpha);
    view.movies.Text(StoreItemKind(view, item), kind.x, kind.y, 1, 1, 0, face.alpha * kind.alpha);
    unsigned action = kBuyButtonEntry;
    if (item.data.type == 16) { action = 6; }
    const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", action);
    MovieRegion button;
    if (entry == nullptr || !view.movies.Region(view.movies.Ordinal(entry->movies[0]), 1, 0, button)) { return false; }
    if (StoreItemButton(view, action, price.x + price.width, price.y + price.height - button.height,
        button.height, enabled, face.alpha * price.alpha)) {
        if (item.data.value32 == 1) {
            if (item.data.type == 16 && profile.coins < item.data.commonPrice) {
                // CMenuAction :94553 checks before launching the IAP conversion.
                state.ShowStorePrompt("MDS_STORE_PROMPT_CONVERSION_SOFT_TO_HARD_FAIL", false, true);
                return true;
            }
            // CMenuAction 0x38 :94519 displays IAP wait before LaunchIAP.
            // Four seconds is the user's requested Windows offline simulation,
            // not a retail payment timer. Product ID and amounts stay in BIG.
            state.BeginOfflineIAP(static_cast<int>(index), view.clock, item.productId);
            std::printf("[offline-iap] pending product=%s common=%u rare=%u\n",
                ReadGameString(toc, item.data.assets[0]).c_str(), item.data.commonPrice, item.data.rarePrice);
        } else {
            const PurchaseResult result = profile.AcquireCurrency(item.data);
            if (result == PurchaseResult::InsufficientWarbucks) {
                state.message.clear();
                state.ShowStorePrompt("MDS_STORE_PROMPT_MOMONEY_CONV", false, true);
            }
            if (result == PurchaseResult::Purchased && !profile.SaveToDisk(savePath)) { return false; }
        }
    }
    return true;
}

/** CStoreAggregator::EquipItem :156082 and SetGun/SetArmor :171658.
 * Granting inventory and equipping it are separate original menu actions.
 */
bool EquipStoreItem(CProfileManager &profile, const CStoreItem &item, const std::vector<ArmorEntry> &armors) {
    CPlayerConfiguration configuration = profile.configuration;
    unsigned gunCount = 0;
    for (const GameObjectTypeRef &object : item.objects) {
        if (object.type == 6 && gunCount < 2) {
            const unsigned slot = (profile.activeWeaponSlot + gunCount) & 1;
            ++gunCount;
            configuration.SetGun(slot, object.object);
        } else if (object.type == 2) {
            bool alreadyEquipped = false;
            for (const GameObjectRef &part : configuration.armor) {
                if (SameObject(part, object.object)) { alreadyEquipped = true; }
            }
            if (alreadyEquipped) { continue; }
            const ArmorEntry *part = nullptr;
            for (const ArmorEntry &entry : armors) {
                if (entry.packHash == object.object.packHash && entry.ordinal == object.object.localIndex) { part = &entry; break; }
            }
            if (part == nullptr || part->data.GetSlot() >= configuration.armor.size()) {
                std::printf("[store] Cannot equip armor pack=%u ordinal=%u\n", object.object.packHash, object.object.localIndex);
                return false;
            }
            configuration.armor[part->data.GetSlot()] = object.object;
        }
    }
    profile.configuration = configuration;
    return true;
}

bool DrawStore(GameMenu &view, CResTOCManager &toc, PackTables &tables, CProfileManager &profile,
    unsigned level, const std::vector<StoreEntry> &store, const std::vector<WeaponEntry> &weapons,
    const std::vector<ArmorEntry> &armors, MenuState &state, const std::filesystem::path &savePath) {
    const unsigned storeMenu = view.movies.Ordinal("GLU_MOVIE_STORE_MENU");
    const unsigned storeScroll = view.movies.Ordinal("GLU_MOVIE_STORE_SCROLL");
    const unsigned shopBox = view.movies.Ordinal("GLU_MOVIE_SHOP_BOX");
    const unsigned sortBar = view.movies.Ordinal("GLU_MOVIE_SORT_BAR");
    const CMovie *cardMovie = view.movies.GetMovie(shopBox);
    unsigned cardStart = 0, cardEnd = 0, openStart = 0, openEnd = 0;
    if (cardMovie == nullptr || !cardMovie->GetChapterRange(1, cardStart, cardEnd) ||
        !cardMovie->GetChapterRange(2, openStart, openEnd) || !AdvanceStoreCard(view, state, *cardMovie)) { return false; }
    const CMovie *filterMovie = view.movies.GetMovie(sortBar);
    if (filterMovie == nullptr || !AdvanceStoreFilter(view, state, *filterMovie)) { return false; }
    MovieRegion content, categoryBar, playerPanel, gunSwap;
    if (!RequireRegion(view, storeMenu, kStoreContentRegion, 0, content, "content") ||
        !RequireRegion(view, storeMenu, kStoreCategoryRegion, 0, categoryBar, "categories") ||
        !RequireRegion(view, storeMenu, kStorePlayerRegion, 0, playerPanel, "player") ||
        !RequireRegion(view, storeMenu, kStoreGunSwapRegion, 0, gunSwap, "gun swap")) { return false; }
    const bool modalOpen = state.store.shopDetailOpen || state.store.shopFilterOpen || state.currencyPending ||
        state.storePromptRequested || state.storePopup.IsActive() || state.promotion.IsActive();
    DrawStoreCategories(view, categoryBar, state, !modalOpen);

    std::vector<unsigned> items;
    std::vector<unsigned> itemSlots;
    // The first column links to the local friend and currency flows, on every
    // category page. The starter bundle follows as the store's own first row.
    if (state.store.shopFilter == 0) {
        items.push_back(static_cast<unsigned>(store.size())); itemSlots.push_back(6);
        items.push_back(static_cast<unsigned>(store.size() + 1)); itemSlots.push_back(6);
        for (unsigned index = 0; index < store.size(); ++index) {
            if (state.store.shopCategory != 0 || store[index].data.singlePurchase == 0 ||
                profile.IsPackageHidden(store[index].ref)) { continue; }
            items.push_back(index);
            itemSlots.push_back(6);
            break;
        }
    }
    // CStoreItem's trailing int16 is the store's own row order; a negative value
    // keeps the record out of the list entirely.
    // Correction: OverrideItem :233074 makes owned negative-order gear visible.
    std::vector<std::pair<int, unsigned>> ordered;
    for (unsigned index = 0; index < store.size(); ++index) {
        const int order = GetStoreDisplayOrder(store[index].data, profile);
        if (order < 0 || store[index].data.value242 == 1 || store[index].data.singlePurchase != 0) { continue; }
        ordered.push_back({order, index});
    }
    std::sort(ordered.begin(), ordered.end());
    for (const std::pair<int, unsigned> &row : ordered) {
        const unsigned index = row.second;
        unsigned slot = state.store.shopGunSlot;
        bool matches = false;
        if (state.store.shopCategory == 0) {
            matches = MatchesEquipmentSlot(store[index], slot, weapons, armors);
        } else if (state.store.shopCategory == 1) {
            for (slot = 2; slot < 5; ++slot) {
                if (MatchesEquipmentSlot(store[index], slot, weapons, armors)) { matches = true; break; }
            }
        } else if (state.store.shopCategory == 3) {
            // CStoreAggregator ctor :158975 gives bank mask 0x1C000.
            slot = 7;
            matches = store[index].data.type >= 14 && store[index].data.type <= 16;
        } else {
            slot = 5;
            matches = MatchesEquipmentSlot(store[index], slot, weapons, armors);
        }
        if (!matches) { continue; }
        // InitFilteredList :159135 uses STORE.type, never the model category.
        const unsigned category = store[index].data.type;
        if ((state.store.shopExclusionFilter & store[index].data.value8) != 0) { continue; }
        const unsigned categoryFilter = state.store.shopFilter & ~kOwnedFilterBit;
        if (categoryFilter != 0 && (categoryFilter & (1u << category)) == 0) { continue; }
        if ((state.store.shopFilter & kOwnedFilterBit) != 0) {
            const GameObjectTypeRef &ref = store[index].data.objects[0];
            if (!profile.Owns(ref.type, ref.object)) { continue; }
        }
        items.push_back(index);
        itemSlots.push_back(slot);
    }
    const unsigned columns = static_cast<unsigned>((items.size() + 1) / 2);

    // The belt scrolls by whole columns; drag and wheel move the same pixel
    // offset and it settles back onto a column once the button is released.
    MovieRegion firstSlot, secondSlot, viewport;
    if (!RequireRegion(view, storeScroll, kFirstColumnRegion, view.storeRestTime, firstSlot, "first column") ||
        !RequireRegion(view, storeScroll, kFirstColumnRegion + 1, view.storeRestTime, secondSlot, "second column") ||
        !RequireRegion(view, storeScroll, 0, view.storeRestTime, viewport, "belt viewport")) { return false; }
    const float columnPitch = std::max(1.0f, secondSlot.x - firstSlot.x);
    const float maximumScroll = std::max(0.0f, (columns - 1.0f) * columnPitch);
    unsigned scrollStart = 0, scrollEnd = 0, nextScrollStart = 0, nextScrollEnd = 0;
    const CMovie *scrollMovie = view.movies.GetMovie(storeScroll);
    if (scrollMovie == nullptr || !scrollMovie->GetChapterRange(1, scrollStart, scrollEnd) ||
        !scrollMovie->GetChapterRange(2, nextScrollStart, nextScrollEnd)) { return false; }
    view.Scroll(state.store.shopMotion, state.store.shopScroll, viewport, !modalOpen, maximumScroll, columnPitch, nextScrollStart - scrollStart);

    int purchaseIndex = -1;
    unsigned purchaseSlot = 0;
    int focusedColumn = -1, focusedRow = 0;
    const unsigned firstColumn = static_cast<unsigned>(std::max(0.0f, std::floor(state.store.shopScroll / columnPitch)));
    // CMenuStore::ItemCallback :178878 places both rows inside the scroll
    // control. Its input must cover the same authored viewport as drawing.
    const bool listHover = view.MouseIn(content.x, viewport.y, content.width, viewport.height);
    // The belt has its own viewport; the content region alone cuts the second row.
    // STORE_SCROLL region 0 is the control's input rectangle, not a vertical
    // drawing clip. CMenuStore::ItemCallback :178878 and CMovieRegion::Draw
    // :109978 allow corner sprites outside it. Clip only horizontally; the
    // host framebuffer supplies the vertical boundary, just as for CMovie.
    view.Clip(content.x, 0, content.width, kMenuHeight);
    for (const auto &slot : view.movies.Regions(storeScroll, view.storeRestTime)) {
        if (slot.index < kFirstColumnRegion) { continue; }
        const unsigned column = firstColumn + slot.index - kFirstColumnRegion;
        if (column >= columns) { continue; }
        const float slotX = firstSlot.x + column * columnPitch - state.store.shopScroll;
        for (unsigned row = 0; row < 2; ++row) {
            const unsigned position = column * 2 + row;
            if (position >= items.size()) { break; }
            const unsigned index = items[position];
            const unsigned slotKind = itemSlots[position];
            if (state.store.shopDetailOpen && state.selectedItem == static_cast<int>(index)) {
                focusedColumn = static_cast<int>(column);
                focusedRow = static_cast<int>(row);
                continue;
            }
            StoreCardFace face;
            face.x = slotX;
            // ItemCallback stacks the second card at half the slot height plus five.
            face.y = slot.y + row * (slot.height / 2 + 5);
            face.alpha = slot.alpha;
            MovieRegion body;
            if (!CardRegion(view, shopBox, kCardBodyRegion, face, body)) { continue; }
            const bool cardEnabled = !modalOpen && listHover;
            if (index >= store.size()) {
                // The invite friends card art, then the free Warbucks entry.
                view.movies.Draw(shopBox, face.time, face.x, face.y, 1024, 768, 0, face.alpha);
                unsigned promoSprite = kInviteCard;
                if (index != store.size()) { promoSprite = kFreeWarbucksCard; }
                // CMenuTapjoyOption::Draw :221948 uses the sprite origin and
                // bounds, without scaling it to SHOP_BOX's user rectangle.
                view.movies.DrawSprite(5, promoSprite, 0, face.x, face.y);
                if (index != store.size()) {
                    // The money pile is art only; the original prints the words.
                    MovieRegion bounds;
                    if (!view.movies.SpriteBounds(5, promoSprite, bounds)) { view.EndClip(); return false; }
                    const float center = face.x + bounds.x + bounds.width / 2;
                    view.CenterText(kFreeCardTop, center, face.y + bounds.y, 6, 1);
                    view.CenterText(kFreeCardBottom, center, face.y + bounds.y + bounds.height - view.movies.TextHeight(6), 6, 1);
                }
                if (cardEnabled && view.Hit(body.x, body.y, body.width, body.height)) {
                    unsigned action = 125;
                    if (index != store.size()) { action = 130; }
                    if (!state.promotion.Activate(view.movies, action)) { view.EndClip(); return false; }
                    state.promotionTick = view.clock;
                    view.EndClip();
                    return true;
                }
                continue;
            }
            const StoreEntry &item = store[index];
            if (item.data.type >= 14 && item.data.type <= 16) {
                if (!DrawCurrencyCard(view, toc, tables, item, index, shopBox, face,
                    cardEnabled, state, profile, savePath)) { return false; }
                continue;
            }
            const GameObjectTypeRef &ref = item.data.objects[0];
            MovieRegion name, icon, kind, price, right;
            if (!CardRegion(view, shopBox, kCardNameRegion, face, name) ||
                !CardRegion(view, shopBox, kCardIconRegion, face, icon) ||
                !CardRegion(view, shopBox, kCardCategoryRegion, face, kind) ||
                !CardRegion(view, shopBox, kCardPriceRegion, face, price) ||
                !CardRegion(view, shopBox, kCardRightRegion, face, right)) { continue; }
            view.movies.Rectangle(body.x, body.y, body.width, body.height, 0, 0, 0, face.alpha);
            view.movies.Draw(shopBox, face.time, face.x, face.y, 1024, 768, 0, face.alpha);
            // The icon sits between the name and the category row, as on the
            // original card; region 5 alone would run under the title.
            // Correction: ThumbCallback :181036 uses region 5 and original PNG size.
            view.Icon(toc, tables, item, icon.x, icon.y, icon.width, icon.height, face.alpha * icon.alpha, true);
            view.movies.Text(item.name, name.x, name.y, 1, 1, 0, face.alpha * name.alpha);
            // LevelCallback :180799 uses region 7 for the category and price.
            view.movies.Text(StoreItemKind(view, item), price.x, price.y, 1, 1, 0, face.alpha * price.alpha);
            bool owned = profile.Owns(ref.type, ref.object);
            // A single-purchase bundle counts as owned once every part of it is.
            if (item.data.singlePurchase != 0) { owned = OwnsBundle(profile, item.data); }
            // GetItemStatus :155316 excludes consumables from the OWNED state.
            if (ref.type == 17) { owned = false; }
            bool equipped = false;
            if (slotKind < 5) { equipped = IsStoreObjectEquipped(profile, slotKind, ref.object); }
            MovieRegion stamp;
            if ((owned || equipped) && CardRegion(view, shopBox, kCardStampRegion, face, stamp)) {
                unsigned stampSprite = kOwnedStamp;
                if (equipped) { stampSprite = kEquippedStamp; }
                // ThumbCallback draws the ownership overlay centered at region 5.
                view.movies.DrawSprite(5, stampSprite, 0, icon.x + icon.width / 2,
                    icon.y + icon.height / 2, 1, face.alpha * icon.alpha);
            }
            if (!owned || slotKind == 5) { DrawCardPrice(view, item.data, price, face.alpha); }
            // The folded card prints the record's own power template. Bundles
            // leave it empty and put their promo line in the stat template.
            const WeaponEntry *cardWeapon = FindWeaponEntry(weapons, ref.object);
            unsigned cardMastery = 0;
            if (cardWeapon != nullptr) { cardMastery = cardWeapon->data.GetMasteryLevel(profile.GetWeaponExperience(ref.object)); }
            std::string rightTemplate = ReadGameString(toc, item.data.assets[5]);
            if (rightTemplate.empty()) { rightTemplate = ReadGameString(toc, item.data.assets[4]); }
            if (!rightTemplate.empty()) {
                MovieRegion templateArea = right;
                templateArea.alpha *= face.alpha;
                DrawStoreTemplate(view, rightTemplate, templateArea,
                    StoreStatValues(item.data, cardMastery), !ReadGameString(toc, item.data.assets[5]).empty());
            }
            MovieRegion quantity;
            if (CardRegion(view, shopBox, kCardBadgeRegion, face, quantity)) {
                DrawStoreQuantity(view, profile, ref, quantity, face.alpha);
            }
            // The corner region carries the bronze/silver/gold mastery badge.
            if (slotKind < 2 && cardMastery > 0) {
                MovieRegion badge;
                if (CardRegion(view, shopBox, kCardBadgeRegion, face, badge)) {
                    // CornerCallback :180786 + CreateContentSprite :150159 use 26:24..26.
                    view.movies.DrawSprite(26, 24 + cardMastery - 1, 0,
                        badge.x + badge.width / 2, badge.y + badge.height / 2, 1, face.alpha * badge.alpha);
                }
            }
            unsigned action = kBuyButtonEntry;
            if (owned && slotKind < 5) { action = kEquipButtonEntry; }
            const bool soldOut = item.data.singlePurchase != 0 && owned;
            // Only guns carry a mastery meter, and a mastered one has nothing
            // left to buy, so it keeps the plain EQUIP plate.
            if (equipped && slotKind < 2 && cardMastery < kMaxMasteryLevel) { action = kUpgradeButtonEntry; }
            const OriginalMenuEntry *actionEntry = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", action);
            MovieRegion actionLabel;
            if (actionEntry == nullptr || !view.movies.Region(view.movies.Ordinal(actionEntry->movies[0]), 1, 0, actionLabel)) { return false; }
            MovieRegion buttonArea = right;
            // An owned item's cost string is empty; LevelCallback :180839 puts
            // its button on region 7. PropertiesCallback :181022 centers smaller
            // buttons, and right-aligns wider ones, inside region 4.
            if (owned && slotKind < 5) { buttonArea = price; }
            float buttonRight = buttonArea.x + buttonArea.width;
            if ((!owned || slotKind == 5) && actionLabel.width <= buttonArea.width) {
                buttonRight = buttonArea.x + (buttonArea.width + actionLabel.width) / 2;
            }
            if (!soldOut && StoreItemButton(view, action, buttonRight,
                buttonArea.y + buttonArea.height - actionLabel.height, actionLabel.height, cardEnabled, face.alpha)) {
                if (action == kUpgradeButtonEntry) {
                    state.masteryWeapon = ref.object;
                    state.Navigate(26);
                } else {
                    purchaseIndex = static_cast<int>(index);
                    purchaseSlot = slotKind;
                }
            } else if (cardEnabled && view.Hit(body.x, body.y, body.width, body.height)) {
                state.selectedItem = static_cast<int>(index);
                state.slot = slotKind;
                state.store.shopDetailOpen = true;
                state.store.shopPreview = false;
                state.store.shopDetailStart = view.clock;
                state.store.shopDetailLastTick = view.clock;
                state.store.shopDetailTime = cardStart;
                state.store.shopDetailClosing = false;
                state.store.shopFocusAmount = 0;
            }
        }
    }
    view.EndClip();
    // The belt's own gradient fades the far column out behind the player.
    view.movies.Draw(storeScroll, view.storeRestTime);

    const GameObjectTypeRef *preview = nullptr;
    unsigned previewSlot = state.store.shopGunSlot;
    if (state.store.shopPreview && state.selectedItem >= 0 && state.selectedItem < static_cast<int>(store.size()) &&
        state.slot < 5) {
        preview = &store[state.selectedItem].data.objects[0];
        previewSlot = state.slot;
    }
    // The original lets the player turn the model by dragging it.
    unsigned rotationDelta = 0;
    if (view.clock >= state.playerMeshLastTick) { rotationDelta = static_cast<unsigned>(view.clock - state.playerMeshLastTick); }
    state.playerMeshLastTick = view.clock;
    view.UpdateMeshRotation(state.playerMesh, rotationDelta, playerPanel, !modalOpen);
    if (!view.DrawEquippedPlayer(toc, tables, profile, weapons, armors, previewSlot, preview, &playerPanel,
        state.playerMesh.GetRadians())) { return false; }
    if (view.TakePlayerPreviewSlotChange()) {
        profile.activeWeaponSlot = view.GetPlayerPreviewSlot();
        if (!profile.SaveToDisk(savePath)) { return false; }
    }
    // MDS_BUTTON_STORE_GUN_SWAP is the round weapon slot toggle.
    if (!DrawStoreGunSwap(view, state, gunSwap, !modalOpen)) { return false; }

    // The FILTER button and its drop-down both live in GLU_MOVIE_SORT_BAR.
    const unsigned sortTime = state.store.shopFilterTime;
    MovieRegion sortButton, sortLabel;
    if (!RequireRegion(view, sortBar, kSortButtonRegion, sortTime, sortButton, "filter button") ||
        !RequireRegion(view, sortBar, kSortLabelRegion, sortTime, sortLabel, "filter label")) { return false; }
    view.movies.Draw(sortBar, sortTime);
    // SortLabelCallback :178777 uses font 5 at the label region's top and
    // centres its measured width. The touch rectangle is not a text layout.
    const std::string filterLabel = view.movies.NamedString("IDS_SHOP_FILTER");
    view.movies.Text(filterLabel, sortLabel.x + (sortLabel.width - view.movies.TextWidth(filterLabel, 5)) * 0.5f,
        sortLabel.y, 5, 1, 0, sortLabel.alpha);
    if (!state.store.shopDetailOpen && !state.currencyPending && view.Hit(sortButton.x, sortButton.y, sortButton.width, sortButton.height)) {
        state.store.shopFilterOpen = !state.store.shopFilterOpen;
    }
    // The original region callback keeps drawing while the movie reverses.
    // Only open-state buttons accept input (CMenuStore::Update :179558).
    MovieRegion sortPanel;
    if (view.movies.Region(sortBar, kSortPanelRegion, sortTime, sortPanel)) {
        const char *table = nullptr;
        const unsigned rows = StoreFilterRows(state.store.shopCategory, table);
        float optionY = sortPanel.y;
        for (unsigned row = 0; row < rows; ++row) {
            const OriginalMenuEntry *entry = OriginalMenuData(table, row);
            if (entry == nullptr) { return false; }
            // MDS is extracted from the original executable; it selects each
            // button movie, sprite and string. The movie supplies its geometry.
            const unsigned optionPlate = view.movies.Ordinal(entry->movies[0]);
            MovieRegion optionLabel, optionTouch;
            if (!RequireRegion(view, optionPlate, 1, 0, optionLabel, "filter option label") ||
                !RequireRegion(view, optionPlate, 0, 0, optionTouch, "filter option touch")) { return false; }
            const float optionX = sortPanel.x + (sortPanel.width - optionLabel.width) * 0.5f;
            const float y = optionY;
            optionY += optionLabel.height * kSortRowSpacing;
            unsigned bit = 0;
            unsigned mask = state.store.shopFilter;
            if (entry->action == 66) {
                bit = 1u << (entry->parameter - 1);
                mask = state.store.shopExclusionFilter;
            } else if (entry->parameter != 17) { bit = 1u << entry->parameter; }
            bool selected = mask == 0;
            if (bit != 0) { selected = (mask & bit) != 0; }
            unsigned sprite = entry->sprites[1];
            if (selected) { sprite = entry->sprites[0]; }
            view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, optionX, y, optionLabel.width, optionLabel.height);
            PlateLabel(view, view.movies.NamedString(entry->strings[0]), optionX, y, optionLabel.width, optionLabel.height);
            const float touchX = optionX + optionTouch.x - optionLabel.x;
            const float touchY = y + optionTouch.y - optionLabel.y;
            if (!state.store.shopFilterOpen || !view.Hit(touchX, touchY, optionTouch.width, optionTouch.height)) { continue; }
            view.NotePress(optionPlate, optionX, y, optionLabel.width, optionLabel.height);
            if (entry->action == 66) { state.store.shopExclusionFilter ^= bit; }
            else if (bit == 0) { state.store.shopFilter = 0; }
            else { state.store.shopFilter ^= bit; }
            state.store.shopScroll = 0; state.store.shopMotion = MenuScrollMotion{};
        }
    }

    if (state.store.shopDetailOpen && state.selectedItem >= 0 && state.selectedItem < static_cast<int>(store.size())) {
        const StoreEntry &item = store[state.selectedItem];
        const GameObjectTypeRef &ref = item.data.objects[0];
        StoreCardFace face;
        const unsigned elapsed = static_cast<unsigned>(view.clock - state.store.shopDetailStart);
        face.time = state.store.shopDetailTime;
        MovieRegion body, foldedBody;
        if (!CardRegion(view, shopBox, kCardBodyRegion, face, body) ||
            !view.movies.Region(shopBox, kCardBodyRegion, cardStart, foldedBody)) { return false; }
        // The card grows out of its own place on the belt and stays in the list.
        float grownX = content.x, grownY = content.y;
        if (focusedColumn >= 0) {
            MovieRegion slot;
            if (view.movies.Region(storeScroll, kFirstColumnRegion, view.storeRestTime, slot)) {
                grownX = firstSlot.x + focusedColumn * columnPitch - state.store.shopScroll;
                grownY = slot.y + focusedRow * (slot.height / 2 + 5);
            }
        }
        // CMenuStore::Init :180297 gets the focus center from STORE_MENU region 0:
        // centerX = x + width/2 - width/16, centerY = y + height/2.
        const float targetX = content.x + content.width / 2 - static_cast<int>(content.width) / 16;
        const float targetY = content.y + content.height / 2;
        const float centerX = grownX + foldedBody.width / 2;
        const float centerY = grownY + foldedBody.height / 2;
        face.x = centerX + (targetX - centerX) * state.store.shopFocusAmount - body.width / 2;
        face.y = centerY + (targetY - centerY) * state.store.shopFocusAmount - body.height / 2;
        if (!CardRegion(view, shopBox, kCardBodyRegion, face, body)) { return false; }
        // The old implementation used the fully open rectangle for every frame.
        // Bind :181843 only uses chapter 2 to FORMAT text; callbacks paint at the
        // current region, including its alpha and clipping, throughout expansion.
        StoreCardFace open = face;
        open.time = openStart;
        MovieRegion finalDescription, finalStats;
        if (!CardRegion(view, shopBox, kCardDescriptionRegion, open, finalDescription) ||
            !CardRegion(view, shopBox, kCardRightRegion, StoreCardFace{face.x, face.y, 1, cardStart}, finalStats)) { return false; }
        view.movies.Rectangle(body.x, body.y, body.width, body.height, 0, 0, 0, body.alpha);
        view.movies.Draw(shopBox, face.time, face.x, face.y);
        const WeaponEntry *weapon = FindWeaponEntry(weapons, ref.object);
        unsigned mastery = 0;
        if (weapon != nullptr) { mastery = weapon->data.GetMasteryLevel(profile.GetWeaponExperience(ref.object)); }
        const auto values = StoreStatValues(item.data, mastery);
        bool owned = profile.Owns(ref.type, ref.object);
        if (item.data.singlePurchase != 0) { owned = OwnsBundle(profile, item.data); }
        if (ref.type == 17) { owned = false; }
        bool equipped = false;
        if (state.slot < 5) { equipped = IsStoreObjectEquipped(profile, state.slot, ref.object); }
        MovieRegion region;
        if (CardRegion(view, shopBox, kCardIconRegion, face, region)) {
            view.Icon(toc, tables, item, region.x, region.y, region.width, region.height, region.alpha, true);
            if (owned || equipped) {
                unsigned sprite = kOwnedStamp;
                if (equipped) { sprite = kEquippedStamp; }
                view.movies.DrawSprite(5, sprite, 0, region.x + region.width / 2, region.y + region.height / 2, 1, region.alpha);
            }
        }
        if (CardRegion(view, shopBox, kCardBadgeRegion, face, region)) {
            DrawStoreQuantity(view, profile, ref, region);
        }
        if (CardRegion(view, shopBox, kCardNameRegion, face, region)) {
            view.movies.Text(item.name, region.x, region.y, 1, 1, 0, region.alpha);
        }
        if (CardRegion(view, shopBox, kCardPriceRegion, face, region)) {
            view.movies.Text(StoreItemKind(view, item), region.x, region.y, 1, 1, 0, region.alpha);
            if (!owned || state.slot == 5) { DrawCardPrice(view, item.data, region, region.alpha); }
        }
        if (CardRegion(view, shopBox, kCardRightRegion, face, region)) {
            std::string properties = ReadGameString(toc, item.data.assets[5]);
            const bool centered = !properties.empty();
            if (properties.empty()) { properties = ReadGameString(toc, item.data.assets[4]); }
            DrawStoreTemplate(view, properties, region, values, centered, finalStats.width);
        }
        if (CardRegion(view, shopBox, kCardStatsRegion, face, region)) {
            // ARMOR uses precisely the same STORE text as GUNS; do not invent a
            // DEFENSE/ATTACK/SPEED/XP/XPLODIUM list from equipment script values.
            DrawStoreTemplate(view, ReadGameString(toc, item.data.assets[4]), region, values, false, finalStats.width);
        }
        if (CardRegion(view, shopBox, kCardUpgradeRegion, face, region)) {
            if (ref.type == 6 && weapon != nullptr &&
                !DrawMasteryMeter(view, *weapon, profile.GetWeaponExperience(ref.object), region, elapsed)) { return false; }
            if (ref.type == 17 && !DrawPowerupCompatibility(view, item.data, region, elapsed)) { return false; }
        }
        if (CardRegion(view, shopBox, kCardDescriptionRegion, face, region)) {
            DrawStoreTemplate(view, ReadGameString(toc, item.data.assets[3]), region, values, false, finalDescription.width);
        }
        if (CardRegion(view, shopBox, kCardActionRegion, face, region) && region.alpha > 0) {
            const bool interactive = !state.store.shopDetailClosing && state.store.shopDetailTime == cardEnd;
            const OriginalMenuEntry *previewEntry = OriginalMenuData("MDS_BUTTON_STORE_PREVIEW", 0);
            float previewWidth = 0;
            if (previewEntry != nullptr && !owned && state.slot < 5) {
                MovieRegion preview;
                if (!view.movies.Region(view.movies.Ordinal(previewEntry->movies[0]), 1, 0, preview)) { return false; }
                previewWidth = preview.width;
                const unsigned sprite = previewEntry->sprites[0];
                view.movies.DrawSpriteFitted(sprite >> 16, sprite & 255, 0, region.x, region.y, preview.width, preview.height, region.alpha);
                const std::string label = view.movies.NamedString(previewEntry->strings[0]);
                view.movies.Text(label, region.x + (preview.width - view.movies.TextWidth(label, 5)) / 2,
                    region.y + (preview.height - view.movies.TextHeight(5)) / 2, 5, 1, 0, region.alpha);
                if (interactive && view.Hit(region.x, region.y, preview.width, preview.height)) { state.store.shopPreview = !state.store.shopPreview; }
            }
            unsigned action = kBuyButtonEntry;
            if (owned && state.slot < 5) { action = kEquipButtonEntry; }
            if (equipped && weapon != nullptr && mastery < kMaxMasteryLevel) { action = kUpgradeButtonEntry; }
            const bool soldOut = item.data.singlePurchase != 0 && owned;
            if (!soldOut && StoreItemButton(view, action, region.x + region.width, region.y, region.height, interactive, region.alpha)) {
                if (action == kUpgradeButtonEntry) {
                    state.masteryWeapon = ref.object;
                    state.store.shopDetailOpen = false;
                    state.Navigate(26);
                } else {
                    purchaseIndex = state.selectedItem;
                    purchaseSlot = state.slot;
                }
            }
            // PurchaseInfoCallback :180849 reserves both button widths before
            // centering the purchase hint. The level text comes from BIG.
            const OriginalMenuEntry *actionEntry = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", action);
            MovieRegion actionLabel;
            if (actionEntry == nullptr || !view.movies.Region(view.movies.Ordinal(actionEntry->movies[0]), 1, 0, actionLabel)) { return false; }
            const std::string requirement = view.movies.NamedString("IDS_SHOP_LEVEL") + " " + std::to_string(item.data.requiredLevel);
            const float middleX = region.x + previewWidth;
            const float middleWidth = region.width - previewWidth - actionLabel.width;
            view.movies.Text(requirement, middleX + (middleWidth - view.movies.TextWidth(requirement, 1)) / 2,
                region.y + (region.height - view.movies.TextHeight(1)) / 2, 1, 1, 0, region.alpha);
        }
        // Anything outside the expanded card folds it again, like the original.
        // HandleTouchInput :181298 also unfocuses on the card body; button clicks
        // are consumed first. Reverse the current chapter instead of disappearing.
        if (view.Hit(0, 0, 1024, 768) && !state.store.shopDetailClosing) {
            state.store.shopDetailClosing = true;
            state.store.shopPreview = false;
            state.store.shopDetailLastTick = view.clock;
        }
    }
    if (purchaseIndex >= 0) {
        const StoreEntry &item = store[purchaseIndex];
        const PurchaseResult result = profile.AcquireItem(item.data, level);
        // CMenuAction::DoAction 0x38 :94606 uses the original three-button
        // funds prompt. Successful purchases refresh the card without a toast.
        if (result == PurchaseResult::InsufficientCoins) {
            ShowStoreFundsPrompt(state, store, profile, 0, item.data.commonPrice, false);
        } else if (result == PurchaseResult::InsufficientWarbucks) {
            ShowStoreFundsPrompt(state, store, profile, 1, item.data.rarePrice, false);
        }
        if (result == PurchaseResult::Purchased || result == PurchaseResult::Owned) {
            if (purchaseSlot < 2) { profile.configuration.SetGun(purchaseSlot, item.data.objects[0].object); }
            else if (purchaseSlot < 5) { Equipped(profile, purchaseSlot) = item.data.objects[0].object; }
            if (item.data.singlePurchase != 0 && result == PurchaseResult::Purchased) {
                if (!EquipStoreItem(profile, item.data, armors)) { return false; }
                state.store.shopPreview = false;
            }
            if (!profile.SaveToDisk(savePath)) { return false; }
        }
    }
    return true;
}

/** CMenuMovieMultiplayerOverlay :250020..250880, region callbacks 0..5,
 * font 0 and MDS_BUTTON_MP_TOGGLE. No locally fabricated online mode. */
bool DrawOriginalModeOverlay(GameMenu &view, MenuState &state) {
    UpdateLocalConnection(state);
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_MULTIPLAYER_AND_VERSUS_MAP");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned openStart = 0, openEnd = 0, foldStart = 0, foldEnd = 0, unfoldStart = 0, unfoldEnd = 0;
    unsigned idleStart = 0, idleEnd = 0;
    if (movie == nullptr || !movie->GetChapterRange(0, openStart, openEnd) ||
        !movie->GetChapterRange(2, foldStart, foldEnd) || !movie->GetChapterRange(3, unfoldStart, unfoldEnd) ||
        !movie->GetChapterRange(4, idleStart, idleEnd)) { return false; }
    if (!state.mode.modeBound) {
        state.mode.modeBound = true;
        state.mode.modeLastTick = view.clock;
        state.mode.modeTime = openStart;
        state.mode.modePhase = 0;
        if (!view.animateNavigation) { state.mode.modeTime = openEnd; }
        if (state.mode.modeSelected) { state.mode.modePhase = 2; state.mode.modeTime = idleStart; }
    }
    if (state.mode.modeLastTick == 0) { state.mode.modeLastTick = view.clock; }
    const unsigned elapsed = static_cast<unsigned>(view.clock - state.mode.modeLastTick);
    state.mode.modeLastTick = view.clock;
    state.mode.modeSpriteTime += elapsed;
    if (!view.AdvanceModeEffects(elapsed)) { return false; }
    if (state.mode.modePhase == 0) { state.mode.modeTime = std::min(openEnd, state.mode.modeTime + elapsed); }
    if (state.mode.modePhase == 1) {
        state.mode.modeTime = std::min(idleEnd, state.mode.modeTime + elapsed);
        if (state.mode.modeTime == idleEnd) { state.mode.modePhase = 2; }
    } else if (state.mode.modePhase == 2) { state.mode.modeTime = idleStart + (state.mode.modeTime - idleStart + elapsed) % (idleEnd - idleStart + 1); }
    else if (state.mode.modePhase == 3) {
        state.mode.modeTime -= std::min(elapsed, state.mode.modeTime - foldStart);
        if (state.mode.modeTime == foldStart) { state.mode.modePhase = 0; }
    }
    float otherAlpha = 1;
    if (state.mode.modePhase == 1 || state.mode.modePhase == 3) {
        otherAlpha = std::max(0.0f, 1.0f - 2.0f * (state.mode.modeTime - foldStart) / (foldEnd - foldStart));
    }
    ModeOverlayCallbacks callback(view, state, otherAlpha);
    if (!view.movies.Draw(ordinal, state.mode.modeTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callback)) { return false; }
    if (state.mode.modePhase == 1 || state.mode.modePhase == 3 || (state.mode.modePhase == 0 && state.mode.modeTime < openEnd)) { return true; }
    for (const auto &region : view.movies.Regions(ordinal, state.mode.modeTime)) {
        if (region.index > 5 || region.index % 2 != 1) { continue; }
        const unsigned mode = region.index / 2;
        if (state.mode.modePhase == 2 && mode != state.gameMode) { continue; }
        if (!view.Hit(region.x, region.y, region.width, region.height)) { continue; }
        if (state.mode.modePhase == 2) {
            state.mode.modeTime = unfoldStart;
            state.mode.modePhase = 3;
        } else if (mode == 0 || state.online.IsConnected()) {
            state.gameMode = mode;
            state.mode.modeSelected = true;
            state.mode.modeTime = foldStart;
            state.mode.modePhase = 1;
            if (!view.StartModeSelectionEffect()) { return false; }
            if (state.page == 22) { state.Navigate(0, true); }
        } else {
            // SetSelection :250261 uses table 189/2 when multiplayer service
            // availability (provider 82) is false, before changing game type.
            state.ShowStorePrompt("MDS_PROMPT_MP_UNAVAILABLE", false, true, 2);
            std::printf("[mode] unavailable original prompt; game type unchanged\n");
        }
    }
    return true;
}
} // namespace MenuDetail
