#include "gun_bros_re/ui/host/ZStorePurchase.h"
#include "gun_bros_re/ui/host/ZMenuSurface.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "gun_bros_re/ui/menus/CMenuStoreOption.h"
namespace MenuDetail {
const ZStoreEntry *FindWeaponStore(const std::vector<ZStoreEntry> &store, const GameObjectRef &ref) {
    for (const auto &entry : store) {
        if (entry.data.type > 6) { continue; }
        for (const auto &object : entry.data.objects) {
            if (object.type == 6 && SameObject(object.object, ref)) { return &entry; }
        }
    }
    return nullptr;
}
/** The upgrade popup is reached from the store as well as from the results,
 * so closing it returns to whichever page pushed it. */
void CloseMastery(CMenuSystem &state) {
    state.masteryPopup = CMenuUpgradePopup();
    if (state.stack.history.empty()) { state.Navigate(27); return; }
    state.Back();
}
/** How far into GLU_MOVIE_WEAPON_UPGRADE_MASTERY the meter stands for this
 * much experience. The movie's chapters are the three cells. */
unsigned MasteryMeterTime(ZMenuSurface &view, const ZWeaponEntry &weapon, unsigned experience) {
    const unsigned meter = view.movies.Ordinal("GLU_MOVIE_WEAPON_UPGRADE_MASTERY");
    CMovie *movie = view.movies.GetMovie(meter);
    if (movie == nullptr) { return 0; }
    unsigned target = 0;
    if (!CMenuUpgradePopup::StarsTarget(*movie, weapon.data, experience, target)) { return 0; }
    return target;
}
/** Bind CMenuMovieButton's original region 1 graphic/label and region 0 hit box. */
bool DrawUpgradeButton(ZMenuSurface &view, unsigned index, const ZMovieRegion &area,
    const std::string &label, unsigned font, bool interactive, bool &pressed) {
    const CMenuDataProvider::Entry *entry = CMenuDataProvider::Find("MDS_BUTTON_STORE_UPGRADE", index);
    if (entry == nullptr) { return false; }
    return CMenuMovieButton::DrawFrame(view, *entry, area, label, font, interactive, pressed);
}
/** Original callbacks use each MovieRegion and bitmap font without fitting. */
void UpgradeCenteredText(ZMenuSurface &view, const ZMovieRegion &region, const std::string &text, unsigned font) {
    view.movies.Text(text, region.x + (region.width - view.movies.TextWidth(text, font)) / 2,
        region.y + (region.height - view.movies.TextHeight(font)) / 2, font, 1, 0, region.alpha);
}
/** CMenuUpgradePopup::DrawBodyText :392576: only changed stats, then CRIT.
 * CURRENT is the absolute STORE value; NEXT is the relative percentage change. */
void DrawUpgradeStats(ZMenuSurface &view, const ZMovieRegion &area, const CStoreItem &item, unsigned level, bool next) {
    constexpr const char *titles[] = {"IDS_UPGRADE_POWER", "IDS_UPGRADE_DAMAGE", "IDS_UPGRADE_RPM", "IDS_UPGRADE_SPEED", "", ""};
    constexpr const char *tokens[] = {"POWER", "DMG", "RPM", "SPD", "DEF", "ATK"};
    constexpr const char *critical[] = {"IDS_UPGRADE_CRITICAL_CHANCE_NONE", "IDS_UPGRADE_CRITICAL_CHANCE_LOW",
        "IDS_UPGRADE_CRITICAL_CHANCE_MED", "IDS_UPGRADE_CRITICAL_CHANCE_HIGH"};
    struct Row { std::string label, value; };
    std::vector<Row> rows;
    const unsigned nextLevel = std::min(kMaxMasteryLevel, level + 1);
    const auto values = StoreStatValues(item, level);
    for (unsigned stat = 0; stat < 6; ++stat) {
        const auto &column = item.statGroups[stat];
        if (column.size() <= nextLevel) { continue; }
        std::int64_t before = column[level], after = column[nextLevel];
        if (stat == 3) { before += 100; after += 100; }
        if (before == 0) { continue; }
        const std::int64_t change = 100 * (after - before) / before;
        if (change == 0) { continue; }
        std::string value = SubstituteStoreStats(std::string("#") + tokens[stat], values);
        if (next) {
            value = std::to_string(change) + "%";
            if (change > 0) { value = "+" + value; }
        }
        rows.push_back({view.movies.NamedString(titles[stat]), value});
    }
    unsigned criticalLevel = level;
    if (next) { criticalLevel = nextLevel; }
    rows.push_back({view.movies.NamedString("IDS_UPGRADE_CRIT"), view.movies.NamedString(critical[criticalLevel])});
    const float height = view.movies.TextHeight(1);
    const int gap = static_cast<int>(area.height - height * rows.size()) / static_cast<int>(rows.size() + 1);
    float y = area.y + gap;
    for (const Row &row : rows) {
        view.movies.Text(row.label, area.x, y, 1, 1, 0, area.alpha);
        view.movies.Text(row.value, area.x + area.width - view.movies.TextWidth(row.value, 1), y, 1, 1, 0, area.alpha);
        y += height + gap;
    }
}
/** The original popup advances its own movie and stars through six states.
 * All geometry, fonts, item values, chapter times and art are read from BIG. */
bool DrawMastery(ZMenuSurface &view, CMenuSystem &state, CProfileManager &profile, CResTOCManager &toc,
    ZPackTables &tables, const std::vector<ZStoreEntry> &store, const std::vector<ZWeaponEntry> &weapons,
    const std::filesystem::path &savePath, CPlayerProgress *headerProgress ) {
    const ZWeaponEntry *weapon = FindMasteryWeapon(weapons, state.masteryWeapon);
    const ZStoreEntry *item = FindWeaponStore(store, state.masteryWeapon);
    if (weapon == nullptr || item == nullptr) { CloseMastery(state); return true; }
    const unsigned popup = view.movies.Ordinal("GLU_MOVIE_UPGRADE_POPUP");
    const unsigned stars = view.movies.Ordinal("GLU_MOVIE_WEAPON_UPGRADE_MASTERY");
    const CMovie *popupMovie = view.movies.GetMovie(popup);
    const CMovie *starsMovie = view.movies.GetMovie(stars);
    if (popupMovie == nullptr || starsMovie == nullptr) { return false; }
    if (!state.masteryPopup.IsBound()) {
        if (!state.masteryPopup.Bind(*popupMovie, *starsMovie, weapon->data, profile.GetWeaponExperience(state.masteryWeapon))) { return false; }
        state.masteryOpened = view.clock;
    } else {
        unsigned delta = 0;
        if (view.clock >= state.masteryOpened) { delta = static_cast<unsigned>(view.clock - state.masteryOpened); }
        state.masteryOpened = view.clock;
        if (!state.storePromptRequested && !state.storePopup.IsActive()) { state.masteryPopup.Update(delta); }
    }
    if (state.masteryPopup.GetState() == CMenuUpgradePopup::State::Closed) { CloseMastery(state); return true; }
    const bool interactive = state.masteryPopup.GetState() == CMenuUpgradePopup::State::Ready &&
        !state.storePromptRequested && !state.storePopup.IsActive();
    const unsigned experience = state.masteryPopup.DisplayExperience();
    const unsigned level = weapon->data.GetMasteryLevel(experience);
    const unsigned nextLevel = std::min(kMaxMasteryLevel, level + 1);
    const float backdrop = state.masteryPopup.BackdropAlpha() / 255.0f;
    view.movies.Rectangle(0, 0, kMenuWidth, kMenuHeight, 0, 0, 0, backdrop);
    // CMenuSystem::Draw :96847 draws BetweenMenuAndHud before the HUD,
    // then draws the popup last. Update :96904 consumes all underlying input.
    if (headerProgress != nullptr) {
        const bool click = view.ExchangeClick(false);
        unsigned headerPage = 2;
        if (state.refinementRequired) { headerPage = 25; }
        if (view.navigation.Draw(view, profile, *headerProgress, headerPage) == -3) { return false; }
        view.ExchangeClick(click);
    }
    if (!view.movies.Draw(popup, state.masteryPopup.MovieTime())) { return false; }
    bool buyPressed = false, closePressed = false, swapPressed = false;
    const ZWeaponEntry *other = nullptr;
    // ShowForGuns :394113 prepares both distinct equipped guns below gold.
    // The same popup and swap action are used from the store and the refinery.
    for (const GameObjectRef &gun : profile.configuration.guns) {
        if (SameObject(gun, state.masteryWeapon)) { continue; }
        const ZWeaponEntry *candidate = FindMasteryWeapon(weapons, gun);
        if (candidate != nullptr && candidate->data.GetMasteryLevel(profile.GetWeaponExperience(gun)) < kMaxMasteryLevel &&
            FindWeaponStore(store, gun) != nullptr) { other = candidate; break; }
    }
    for (const ZMovieRegion &region : view.movies.Regions(popup, state.masteryPopup.MovieTime())) {
        if (region.alpha <= 0) { continue; }
        switch (region.index) {
        case kUpgradePortraitRegion: {
            ZMovieRegion bounds;
            const unsigned animation = kBrotherPortrait + profile.playerBrother;
            if (!view.movies.SpriteBounds(0, animation, bounds) ||
                !view.movies.DrawSprite(0, animation, 0, region.x + region.width - bounds.width,
                    region.y + region.height - bounds.height, 1, region.alpha)) { return false; }
            break;
        }
        case kUpgradeCloseRegion:
            if (!DrawUpgradeButton(view, 0, region, "", 0, interactive, closePressed)) { return false; }
            break;
        case kUpgradeMeterRegion:
            if (!view.movies.Draw(stars, state.masteryPopup.StarsTime(), region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha)) { return false; }
            break;
        case kUpgradeCurrentHeaderRegion:
            UpgradeCenteredText(view, region, view.movies.NamedString("IDS_UPGRADE_CURRENT_LEVEL_TITLE"), 0);
            break;
        case kUpgradeNextHeaderRegion: {
            constexpr const char *titles[] = {"IDS_UPGRADE_NEXT_LEVEL_TITLE_BRONZE", "IDS_UPGRADE_NEXT_LEVEL_TITLE_SILVER", "IDS_UPGRADE_NEXT_LEVEL_TITLE_GOLD"};
            UpgradeCenteredText(view, region, view.movies.NamedString(titles[std::min(level, 2u)]), 0);
            break;
        }
        case kUpgradeCurrentColumnRegion:
            DrawUpgradeStats(view, region, item->data, level, false);
            break;
        case kUpgradeNextColumnRegion:
            DrawUpgradeStats(view, region, item->data, level, true);
            break;
        case kUpgradeIconRegion:
            view.Icon(toc, tables, *item, region.x, region.y, region.width, region.height, region.alpha, true);
            break;
        case kUpgradeTitleRegion:
            UpgradeCenteredText(view, region, view.movies.NamedString("IDS_UPGRADE_TITLE"), 11);
            break;
        case kUpgradeBuyRegion:
            if (level < kMaxMasteryLevel) {
                const std::string label = SubstituteStoreStats(view.movies.NamedString("IDS_UPGRADE_BUY_BUCKS"), StoreStatValues(item->data, nextLevel));
                if (!DrawUpgradeButton(view, 2, region, label, 0, interactive, buyPressed)) { return false; }
            }
            break;
        case 10:
            if (other != nullptr) {
                ZMovieRegion center = region;
                center.x += static_cast<int>(center.width) / 2;
                center.y += static_cast<int>(center.height) / 2;
                // CreateContentString(155) :152944 supplies selected gun + 1;
                // CMenuUpgradePopup::Bind :393792 uses the original font6.
                unsigned gunNumber = 1;
                if (SameObject(state.masteryWeapon, profile.configuration.guns[1])) { gunNumber = 2; }
                if (!DrawUpgradeButton(view, 1, center, std::to_string(gunNumber), 6, interactive, swapPressed)) { return false; }
            }
            break;
        case kUpgradeNameRegion:
            UpgradeCenteredText(view, region, item->name, 1);
            break;
        }
    }
    if (closePressed) { state.masteryPopup.Hide(); }
    if (swapPressed && other != nullptr) {
        state.masteryWeapon.packHash = other->packHash;
        state.masteryWeapon.localIndex = other->ordinal;
        if (!state.masteryPopup.SelectGun(*starsMovie, other->data, profile.GetWeaponExperience(state.masteryWeapon))) { return false; }
    }
    if (buyPressed && level < kMaxMasteryLevel) {
        const auto &prices = item->data.statGroups[7];
        if (prices.size() <= nextLevel || prices[nextLevel] < 0) { return false; }
        const unsigned price = static_cast<unsigned>(prices[nextLevel]);
        if (profile.warbucks >= price) {
            const unsigned threshold = weapon->data.GetMasteryThreshold(level);
            profile.warbucks -= price;
            profile.AddWeaponExperience(state.masteryWeapon, threshold - experience, weapon->data.GetMasteryLimit());
            if (!profile.SaveToDisk(savePath) || !state.masteryPopup.PerformUpgrade(*starsMovie, weapon->data, threshold)) { return false; }
            std::printf("[upgrade] weapon=%s xp=%u price=%u target=%u\n", item->name.c_str(), threshold, price, state.masteryPopup.TargetTime());
        } else {
            // BuyAction :393339 shows modal category11, button mode1/table154.
            ShowStoreFundsPrompt(state, store, profile, 1, price);
        }
    }
    if (state.masteryPopup.FlashAlpha() > 0) {
        // Original CMenuUpgradePopup::Draw constants :21160-21162, not resource values.
        constexpr unsigned colors[3][3] = {{240, 184, 155}, {217, 217, 217}, {254, 240, 125}};
        const unsigned index = level - 1;
        if (index >= 3) { return false; }
        view.movies.Rectangle(0, 0, kMenuWidth, kMenuHeight, colors[index][0] / 255.0f, colors[index][1] / 255.0f,
            colors[index][2] / 255.0f, state.masteryPopup.FlashAlpha() / 255.0f);
    }
    return true;
}
}
