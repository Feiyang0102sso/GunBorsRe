#include "gun_bros_re/ui/host/ZStoreRegionClip.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
#include "gun_bros_re/ui/menus/CMenuUpgradePopup.h"
#include "gun_bros_re/ui/host/ZLocalOnlineMenus.h"
#include "gun_bros_re/ui/controls/CTextBox.h"
namespace MenuDetail {

bool CMenuStoreOption::DrawCompact(ZMenuSurface &view, CResTOCManager &toc, ZPackTables &tables,
    CProfileManager &profile, const ZStoreEntry &item, const std::vector<ZWeaponEntry> &weapons,
    unsigned slotKind, unsigned shopBox, const Face &face, bool cardEnabled, bool actionEnabled,
    CMenuMovieButton &button, Action &result) {
    result = Action::None;
    const GameObjectTypeRef &ref = item.data.objects[0];
    ZMovieRegion body, name, icon, kind, price, right;
    if (!CardRegion(view, shopBox, kCardBodyRegion, face, body) ||
        !CardRegion(view, shopBox, kCardNameRegion, face, name) ||
        !CardRegion(view, shopBox, kCardIconRegion, face, icon) ||
        !CardRegion(view, shopBox, kCardCategoryRegion, face, kind) ||
        !CardRegion(view, shopBox, kCardPriceRegion, face, price) ||
        !CardRegion(view, shopBox, kCardRightRegion, face, right)) { return false; }
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
    if (item.data.singlePurchase != 0) { owned = CStoreAggregator::OwnsBundle(profile, item.data); }
    // GetItemStatus :155316 excludes consumables from the OWNED state.
    if (ref.type == 17) { owned = false; }
    bool equipped = false;
    if (slotKind < 5) { equipped = CStoreAggregator::IsStoreObjectEquipped(profile, slotKind, ref.object); }
    ZMovieRegion stamp;
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
    const ZWeaponEntry *cardWeapon = CStoreAggregator::FindWeaponEntry(weapons, ref.object);
    unsigned cardMastery = 0;
    if (cardWeapon != nullptr) { cardMastery = cardWeapon->data.GetMasteryLevel(profile.GetWeaponExperience(ref.object)); }
    std::string rightTemplate = ReadGameString(toc, item.data.assets[5]);
    if (rightTemplate.empty()) { rightTemplate = ReadGameString(toc, item.data.assets[4]); }
    if (!rightTemplate.empty()) {
        ZMovieRegion templateArea = right;
        templateArea.alpha *= face.alpha;
        DrawStoreTemplate(view, rightTemplate, templateArea,
            StoreStatValues(item.data, cardMastery), !ReadGameString(toc, item.data.assets[5]).empty());
    }
    ZMovieRegion quantity;
    if (CardRegion(view, shopBox, kCardBadgeRegion, face, quantity)) {
        DrawStoreQuantity(view, profile, ref, quantity, face.alpha);
    }
    // The corner region carries the bronze/silver/gold mastery badge.
    if (slotKind < 2 && cardMastery > 0) {
        ZMovieRegion badge;
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
    // Correction: GetElementAction table 144 :153662 leaves action 193
    // for equipped, non-upgradeable items; Bind :182082 hides that button.
    if (equipped && slotKind < 2 && cardMastery < kMaxMasteryLevel) { action = kUpgradeButtonEntry; }
    const bool actionVisible = !soldOut && (!equipped || action == kUpgradeButtonEntry);
    if (!actionVisible) { button.Cancel(); }
    const CMenuDataProvider::Entry *actionEntry = CMenuDataProvider::Find("MDS_BUTTON_STORE_ITEMS", action);
    ZMovieRegion actionLabel;
    if (actionEntry == nullptr || !view.movies.Region(view.movies.Ordinal(actionEntry->movies[0]), 1, 0, actionLabel)) { return false; }
    ZMovieRegion buttonArea = right;
    // An owned item's cost string is empty; LevelCallback :180839 puts
    // its button on region 7. PropertiesCallback :181022 centers smaller
    // buttons, and right-aligns wider ones, inside region 4.
    if (owned && slotKind < 5) { buttonArea = price; }
    float buttonRight = buttonArea.x + buttonArea.width;
    if ((!owned || slotKind == 5) && actionLabel.width <= buttonArea.width) {
        buttonRight = buttonArea.x + (buttonArea.width + actionLabel.width) / 2;
    }
    if (actionVisible && StoreItemButton(view, button, action, buttonRight,
        buttonArea.y + buttonArea.height - actionLabel.height, actionLabel.height, actionEnabled, face.alpha)) {
        result = Action::Purchase;
        if (action == kUpgradeButtonEntry) { result = Action::Upgrade; }
    } else if (cardEnabled && view.Hit(body.x, body.y, body.width, body.height)) {
        result = Action::Focus;
    }
    return true;
}


// Every menu button prints its label at the same size; the original never
// squeezes one to fit a narrower plate, it picks a wider plate instead.

/** Right aligned price: the original prints the currency sprite and the number,
 * never the currency's name. */
// CreateItemCostString :157573 resolves IDS_SHOP_COMMON/RARE (Omega/delta
// glyph plus %i in this BIG). The currency icons are glyphs in font 0; drawing
// an unrelated sprite at 1.3 times the row height duplicated their layout.

/** Category caption under the icon, from the weapon or armour catalogue. */
// CreateItemCategoryString :157413 indexes IDS_SHOP_SORT3 + STORE.category.

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

/** SHOP_BOX regions resolved for a card whose own origin sits at (x, y). */
bool CardRegion(ZMenuSurface &view, unsigned card, unsigned index, const CMenuStoreOption::Face &face, ZMovieRegion &region) {
    for (const ZMovieRegion &candidate : view.movies.Regions(card, face.time, face.x, face.y)) {
        if (candidate.index != index) { continue; }
        region = candidate;
        return true;
    }
    return false;
}

/** Draw one MDS_BUTTON_STORE_ITEMS plate. Its width is the width of the button
 * movie that row names, right aligned on `right`: BUY and EQUIP take the small
 * plate, UPGRADE the large one, which is why UPGRADE reaches further left. */
bool StoreItemButton(ZMenuSurface &view, CMenuMovieButton &button, unsigned entryIndex,
    float right, float y, float height, bool enabled, float alpha) {
    const auto *entry = CMenuDataProvider::Find("MDS_BUTTON_STORE_ITEMS", entryIndex);
    if (entry == nullptr) { return false; }
    ZMovieRegion label;
    if (!view.movies.Region(view.movies.Ordinal(entry->movies[0]), 1, 0, label)) { return false; }
    ZMovieRegion origin;
    origin.x = right - label.width - label.x + kMenuWidth / 2;
    origin.y = y - label.y + kMenuHeight / 2;
    origin.alpha = alpha;
    bool activated = false;
    if (!button.Draw(view, *entry, origin, 5, enabled, activated)) { return false; }
    return activated;
}

std::string StoreCostText(ZMenuSurface &view, const CStoreItem &item) {
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

void DrawCardPrice(ZMenuSurface &view, const CStoreItem &item, const ZMovieRegion &row, float alpha) {
    const std::string text = StoreCostText(view, item);
    view.movies.Text(text, row.x + row.width - view.movies.TextWidth(text, 0), row.y, 0, 1, 0, alpha);
}

std::string StoreItemKind(ZMenuSurface &view, const ZStoreEntry &item) {
    if (item.data.type > 15) { return {}; }
    return view.movies.NamedString("IDS_SHOP_SORT3", item.data.type);
}

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

void DrawStoreTemplate(ZMenuSurface &view, const std::string &text, const ZMovieRegion &area,
    const std::vector<std::pair<std::string, std::string>> &values, bool centered , float layoutWidth ) {
    if (text.empty() || area.alpha <= 0 || area.width <= 0 || area.height <= 0) { return; }
    if (layoutWidth <= 0) { layoutWidth = area.width; }
    const auto lines = CTextBox::Format(view.movies, SubstituteStoreStats(text, values), layoutWidth);
    ZStoreRegionClip clip(view, area);
    float y = area.y;
    for (const CTextBox::Line &line : lines) {
        float x = area.x;
        if (centered) { x += (layoutWidth - line.width) / 2; }
        for (const CTextBox::Run &run : line.runs) {
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

bool DrawMasteryMeter(ZMenuSurface &view, const ZWeaponEntry &weapon, unsigned experience,
    const ZMovieRegion &area, unsigned elapsed) {
    if (area.alpha <= 0) { return true; }
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_MASTERY");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned target = 0;
    if (movie == nullptr || !StoreMasteryTarget(*movie, weapon.data, experience, target)) { return false; }
    // Focus resets this movie to chapter 0; Update :181490 advances it at 2x.
    const unsigned time = static_cast<unsigned>(std::min<std::uint64_t>(target, 2ull * elapsed));
    ZStoreRegionClip clip(view, area);
    if (!view.movies.Draw(ordinal, time, area.x, area.y, 1024, 768, 0, area.alpha)) { return false; }
    constexpr const char *captions[] = {
        "IDS_WEAPONMASTERY_STORE_TITLE", "IDS_WEAPONMASTERY_STORE_MASTERY_CRITICAL_CHANCE",
        "IDS_WEAPONMASTERY_STORE_MASTERY_CRITICAL_CHANCE_LOW",
        "IDS_WEAPONMASTERY_STORE_MASTERY_CRITICAL_CHANCE_MED",
        "IDS_WEAPONMASTERY_STORE_MASTERY_CRITICAL_CHANCE_HIGH"};
    for (const ZMovieRegion &region : view.movies.Regions(ordinal, time, area.x, area.y)) {
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
bool DrawPowerupCompatibility(ZMenuSurface &view, const CStoreItem &item, const ZMovieRegion &area, unsigned elapsed) {
    if (area.alpha <= 0) { return true; }
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_DEATHMATCH_ONLY_POWERUPS");
    const CMovie *movie = view.movies.GetMovie(ordinal);
    if (movie == nullptr) { return false; }
    const unsigned time = std::min(elapsed, movie->duration);
    ZStoreRegionClip clip(view, area);
    if (!view.movies.Draw(ordinal, time, area.x, area.y, 1024, 768, 0, area.alpha)) { return false; }
    constexpr const char *labels[] = {"IDS_MULTIPLAYER_INACTIVE", "IDS_MULTIPLAYER_ACTIVE", "IDS_MULTIPLAYER_ACTIVE_VERSUS"};
    for (const ZMovieRegion &region : view.movies.Regions(ordinal, time, area.x, area.y)) {
        if (region.index < 1 || region.index > 6) { continue; }
        const unsigned mode = (region.index - 1) / 2;
        if ((region.index & 1) != 0) {
            const std::string text = view.movies.NamedString(labels[mode]);
            view.movies.Text(text, region.x + (region.width - view.movies.TextWidth(text, 1)) / 2,
                region.y, 1, 1, 0, area.alpha * region.alpha);
        } else {
            // CreateContentSprite :150197 uses STORE value8 exclusion bits.
            unsigned sprite = 174;
            if (!item.IsExcludedFromGameType(mode)) { sprite = 173; }
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

/** CornerCallback :180757 and CreateContentSprite :149994 display quantity
 * in a 0:87/88 badge, with the original numeric font; no handwritten OWN label. */
void DrawStoreQuantity(ZMenuSurface &view, const CProfileManager &profile, const GameObjectTypeRef &ref,
    const ZMovieRegion &area, float alpha ) {
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
bool CMenuStoreOption::Update(std::uint64_t clock, const CMovie &movie) {
    unsigned start = 0, end = 0;
    if (!movie.GetChapterRange(1, start, end)) { return false; }
    if (!shopDetailOpen) { return true; }
    std::uint64_t elapsed = 0;
    if (clock >= shopDetailLastTick) { elapsed = clock - shopDetailLastTick; }
    shopDetailLastTick = clock;
    const unsigned step = static_cast<unsigned>(std::min<std::uint64_t>(end - start, elapsed * kCardPlaybackRate));
    shopDetailTime = std::clamp(shopDetailTime, start, end);
    // CMenuStore::SetupFocusInterp :179101 uses 125 ms, an original code constant.
    const float movement = static_cast<float>(elapsed) / 125;
    if (shopDetailClosing) {
        shopDetailTime -= std::min(step, shopDetailTime - start);
        shopFocusAmount = std::max(0.0f, shopFocusAmount - movement);
        if (shopDetailTime == start && shopFocusAmount == 0) {
            shopDetailOpen = false;
            shopDetailClosing = false;
        }
    } else {
        shopDetailTime += std::min(step, end - shopDetailTime);
        shopFocusAmount = std::min(1.0f, shopFocusAmount + movement);
    }
    return true;
}

/** Currency entries have no object references and no cost string. The original
 * LevelCallback :180839 therefore places BUY/CONVERT in the bottom right.
 * Focus :181402 requires a cost string, so these cards do not expand. */
bool DrawCurrencyCard(ZMenuSurface &view, CResTOCManager &toc, ZPackTables &tables,
    const ZStoreEntry &item, unsigned index, unsigned movie, const CMenuStoreOption::Face &face,
    bool enabled, CMenuSystem &state, CProfileManager &profile, const std::filesystem::path &savePath) {
    ZMovieRegion name, icon, kind, price;
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
    const CMenuDataProvider::Entry *entry = CMenuDataProvider::Find("MDS_BUTTON_STORE_ITEMS", action);
    ZMovieRegion button;
    if (entry == nullptr || !view.movies.Region(view.movies.Ordinal(entry->movies[0]), 1, 0, button)) { return false; }
    if (StoreItemButton(view, state.store.options.buttons[index], action, price.x + price.width, price.y + price.height - button.height,
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
            const ZPurchaseResult result = profile.AcquireCurrency(item.data);
            if (result == ZPurchaseResult::InsufficientWarbucks) {
                state.feedback.Clear();
                state.ShowStorePrompt("MDS_STORE_PROMPT_MOMONEY_CONV", false, true);
            }
            if (result == ZPurchaseResult::Purchased && !profile.SaveToDisk(savePath)) { return false; }
        }
    }
    return true;
}

bool CMenuStoreOption::Draw(ZMenuSurface &view, CResTOCManager &toc, ZPackTables &tables,
    CProfileManager &profile, const ZStoreEntry &item, const std::vector<ZWeaponEntry> &weapons,
    unsigned slot, const Face &face, CMenuMovieButton &actionButton, Action &actionResult) {
    actionResult = Action::None;
    const auto &ref = item.data.objects[0];
    const unsigned shopBox = view.movies.Ordinal("GLU_MOVIE_SHOP_BOX");
    const auto *movie = view.movies.GetMovie(shopBox);
    unsigned cardStart = 0, cardEnd = 0, openStart = 0, openEnd = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, cardStart, cardEnd) ||
        !movie->GetChapterRange(2, openStart, openEnd)) { return false; }
    ZMovieRegion body;
    if (!CardRegion(view, shopBox, kCardBodyRegion, face, body)) { return false; }
    const unsigned elapsed = static_cast<unsigned>(view.clock - shopDetailStart);
        CMenuStoreOption::Face open = face;
        open.time = openStart;
        ZMovieRegion finalDescription, finalStats;
        if (!CardRegion(view, shopBox, kCardDescriptionRegion, open, finalDescription) ||
            !CardRegion(view, shopBox, kCardRightRegion, CMenuStoreOption::Face{face.x, face.y, 1, cardStart}, finalStats)) { return false; }
        view.movies.Rectangle(body.x, body.y, body.width, body.height, 0, 0, 0, body.alpha);
        view.movies.Draw(shopBox, face.time, face.x, face.y);
        // Object ordinals are local to their type; an ARMOR can share a GUN's
        // pack/index without being that weapon (CanBeUpgraded :156746).
        const ZWeaponEntry *weapon = nullptr;
        if (ref.type == 6) { weapon = CStoreAggregator::FindWeaponEntry(weapons, ref.object); }
        unsigned mastery = 0;
        if (weapon != nullptr) { mastery = weapon->data.GetMasteryLevel(profile.GetWeaponExperience(ref.object)); }
        const auto values = StoreStatValues(item.data, mastery);
        bool owned = profile.Owns(ref.type, ref.object);
        if (item.data.singlePurchase != 0) { owned = CStoreAggregator::OwnsBundle(profile, item.data); }
        if (ref.type == 17) { owned = false; }
        bool equipped = false;
        if (slot < 5) { equipped = CStoreAggregator::IsStoreObjectEquipped(profile, slot, ref.object); }
        ZMovieRegion region;
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
            if (!owned || slot == 5) { DrawCardPrice(view, item.data, region, region.alpha); }
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
            const bool interactive = !shopDetailClosing && shopDetailTime == cardEnd;
            const CMenuDataProvider::Entry *previewEntry = CMenuDataProvider::Find("MDS_BUTTON_STORE_PREVIEW", 0);
            float previewWidth = 0;
            if (previewEntry != nullptr && !owned && slot < 5) {
                ZMovieRegion preview;
                if (!view.movies.Region(view.movies.Ordinal(previewEntry->movies[0]), 1, 0, preview)) { return false; }
                previewWidth = preview.width;
                ZMovieRegion origin;
                origin.x = region.x - preview.x + kMenuWidth / 2;
                origin.y = region.y - preview.y + kMenuHeight / 2;
                origin.alpha = region.alpha;
                bool activated = false;
                if (!previewButton.Draw(view, *previewEntry, origin, 5, interactive, activated)) { return false; }
                // CStoreAggregator::PreviewItem :156151 reapplies a configuration;
                // repeated PREVIEW must not toggle back to the saved equipment.
                if (activated) { shopPreview = true; }
            }
            unsigned action = kBuyButtonEntry;
            if (owned && slot < 5) { action = kEquipButtonEntry; }
            if (equipped && weapon != nullptr && mastery < kMaxMasteryLevel) { action = kUpgradeButtonEntry; }
            const bool soldOut = item.data.singlePurchase != 0 && owned;
            // Match the folded card and native action 193: no plate, no input,
            // and no pending selection may survive an equipment-state change.
            const bool actionVisible = !soldOut && (!equipped || action == kUpgradeButtonEntry);
            if (!actionVisible) { actionButton.Cancel(); }
            if (actionVisible && StoreItemButton(view, actionButton, action, region.x + region.width, region.y, region.height, interactive, region.alpha)) {
                if (action == kUpgradeButtonEntry) {
                    shopDetailOpen = false;
                    actionResult = Action::Upgrade;
                } else {
                    actionResult = Action::Purchase;
                }
            }
            // PurchaseInfoCallback :180849 reserves both button widths before
            // centering the purchase hint. The level text comes from BIG.
            float actionWidth = 0;
            if (actionVisible) {
                const CMenuDataProvider::Entry *actionEntry = CMenuDataProvider::Find("MDS_BUTTON_STORE_ITEMS", action);
                ZMovieRegion actionLabel;
                if (actionEntry == nullptr || !view.movies.Region(view.movies.Ordinal(actionEntry->movies[0]), 1, 0, actionLabel)) { return false; }
                actionWidth = actionLabel.width;
            }
            const std::string requirement = view.movies.NamedString("IDS_SHOP_LEVEL") + " " + std::to_string(item.data.requiredLevel);
            const float middleX = region.x + previewWidth;
            const float middleWidth = region.width - previewWidth - actionWidth;
            view.movies.Text(requirement, middleX + (middleWidth - view.movies.TextWidth(requirement, 1)) / 2,
                region.y + (region.height - view.movies.TextHeight(1)) / 2, 1, 1, 0, region.alpha);
        }
        // Anything outside the expanded card folds it again, like the original.
        // HandleTouchInput :181298 also unfocuses on the card body; button clicks
        // are consumed first. Reverse the current chapter instead of disappearing.
        if (view.Hit(0, 0, 1024, 768) && !shopDetailClosing) {
            actionButton.Cancel();
            previewButton.Cancel();
            shopDetailClosing = true;
            shopPreview = false;
            shopDetailLastTick = view.clock;
        }
    return true;
}
}
