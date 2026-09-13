#include "gun_bros_re/ui/PostGameCardCallbacks.h"
#include "gun_bros_re/ui/MenuInternal.h"
namespace MenuDetail {

class PostGameListCallbacks : public IMovieRegionCallback {
public:
    PostGameListCallbacks(GameMenu &menu, MenuState &selection, CResTOCManager &manager, PackTables &resources)
        : view(menu), state(selection), toc(manager), tables(resources) {}
    bool DrawMovieRegion(const MovieRegion &region) override {
        if (state.page == 28) {
            if (region.index < 1 || region.index > 4) { return true; }
            const int index = static_cast<int>(std::floor(state.postGame.postGameGalleryPosition)) + static_cast<int>(region.index) - 1;
            if (index < 0 || index >= static_cast<int>(state.result.casualties.size())) { return true; }
            class CasualtyCallback : public IMovieRegionCallback {
            public:
                CasualtyCallback(PostGameListCallbacks &owner, const EnemyCasualty &value) : list(owner), casualty(value) {}
                bool DrawMovieRegion(const MovieRegion &area) override {
                    return list.view.DrawCasualty(list.tables, list.toc, casualty, 0, &area);
                }
                PostGameListCallbacks &list;
                const EnemyCasualty &casualty;
            } callback(*this, state.result.casualties[index]);
            const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_MODEL_GALLERY_ITEM");
            const auto *movie = view.movies.GetMovie(ordinal);
            if (movie == nullptr) { return false; }
            return view.movies.Draw(ordinal, std::min(state.postGame.postGameItemTime, movie->duration),
                region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &callback);
        }
        if (region.index < 1 || region.index > 2) { return true; }
        const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_WRAPUP_BOX");
        MovieRegion bounds;
        const auto *movie = view.movies.GetMovie(ordinal);
        if (movie == nullptr || !view.movies.Region(ordinal, 0, 0, bounds)) { return false; }
        const unsigned first = (region.index - 1) * 2;
        for (unsigned index = first; index < std::min(3u, first + 2); ++index) {
            unsigned icon = index;
            std::uint64_t amount = state.result.xplodium;
            if (index == 1) { amount = state.result.experience; }
            if (index == 2) {
                icon = 4;
                amount = state.result.perfectWaves;
                if (state.result.horde) { icon = 5; amount = state.result.bestKillStreak; }
            }
            const auto *entry = OriginalMenuData("MDS_ICON_POSTGAME", icon);
            if (entry == nullptr) { return false; }
            float x = region.x;
            if (index == 1) { x += region.width - bounds.width; }
            if (index == 2) { x += static_cast<int>(region.width) / 2 - static_cast<int>(bounds.width) / 2; }
            const std::string value = std::to_string(amount);
            PostGameCardCallbacks callback(view, *entry, value, state.postGame.postGameIconTime);
            if (!view.movies.Draw(ordinal, std::min(state.postGame.postGameItemTime, movie->duration),
                x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &callback)) { return false; }
        }
        return true;
    }
    GameMenu &view;
    MenuState &state;
    CResTOCManager &toc;
    PackTables &tables;
};
// Page callback implementations.

const StoreEntry *FindWeaponStore(const std::vector<StoreEntry> &store, const GameObjectRef &ref) {
    for (const auto &entry : store) {
        if (entry.data.type > 6) { continue; }
        for (const auto &object : entry.data.objects) {
            if (object.type == 6 && SameObject(object.object, ref)) { return &entry; }
        }
    }
    return nullptr;
}

void BeginPostGame(MenuState &state, const SurvivalGameContext &context, const std::vector<WeaponEntry> &weapons) {
    state.result = context.result;
    state.postGame.postGameMusic = true;
    state.refinery.casualtyPage = 0;
    state.refinery.refineryTab = 0;
    state.refinementRequired = context.profile.xplodium != 0;
    state.message.clear();
    state.Navigate(27, true);
    state.postGame.postGameBound = false;
    state.postGame.postGameClosing = false;
    state.postGame.postGameUpgradePending = false;
    // ShowForGuns :394260 prefers the active gun, then the other eligible gun.
    const unsigned activeSlot = context.profile.activeWeaponSlot;
    for (unsigned offset = 0; offset < context.profile.configuration.guns.size(); ++offset) {
        const unsigned slot = (activeSlot + offset) % context.profile.configuration.guns.size();
        const auto &ref = context.profile.configuration.guns[slot];
        const WeaponEntry *weapon = FindMasteryWeapon(weapons, ref);
        if (weapon != nullptr && weapon->data.GetMasteryLevel(context.profile.GetWeaponExperience(ref)) < 3) {
            state.masteryWeapon = ref;
            if (context.profile.nativeArchive) { state.postGame.postGameUpgradePending = true; }
            else { state.Navigate(26); }
            break;
        }
    }
}

// The old fixed fill interval is replaced by CMenuUpgradePopup's original 1x playback.

/** The upgrade popup is reached from the store as well as from the results,
 * so closing it returns to whichever page pushed it. */
void CloseMastery(MenuState &state) {
    state.masteryPopup = CMenuUpgradePopup();
    if (state.history.empty()) { state.page = 27; return; }
    state.Back();
}

/** How far into GLU_MOVIE_WEAPON_UPGRADE_MASTERY the meter stands for this
 * much experience. The movie's chapters are the three cells. */
unsigned MasteryMeterTime(GameMenu &view, const WeaponEntry &weapon, unsigned experience) {
    const unsigned meter = view.movies.Ordinal("GLU_MOVIE_WEAPON_UPGRADE_MASTERY");
    CMovie *movie = view.movies.GetMovie(meter);
    if (movie == nullptr) { return 0; }
    unsigned target = 0;
    if (!CMenuUpgradePopup::StarsTarget(*movie, weapon.data, experience, target)) { return 0; }
    return target;
}

/** Bind CMenuMovieButton's original region 1 graphic/label and region 0 hit box. */
bool DrawUpgradeButton(GameMenu &view, unsigned index, const MovieRegion &area,
    const std::string &label, unsigned font, bool interactive, bool &pressed) {
    const OriginalMenuEntry *entry = OriginalMenuData("MDS_BUTTON_STORE_UPGRADE", index);
    if (entry == nullptr) { return false; }
    return DrawOriginalMovieButton(view, *entry, area, label, font, interactive, pressed);
}

bool DrawOriginalMovieButton(GameMenu &view, const OriginalMenuEntry &entry, const MovieRegion &area,
    const std::string &label, unsigned font, bool interactive, bool &pressed, unsigned chapter, unsigned elapsed, unsigned timeOverride, bool stateArtwork) {
    pressed = false;
    const unsigned ordinal = view.movies.Ordinal(entry.movies[0]);
    const CMovie *movie = view.movies.GetMovie(ordinal);
    unsigned start = 0, end = 0;
    if (movie == nullptr || !movie->GetChapterRange(chapter, start, end)) { return false; }
    unsigned time = end;
    if (chapter == 2 || chapter == 3) { time = start + elapsed % (end - start + 1); }
    if (timeOverride != UINT32_MAX) { time = timeOverride; }
    // CMenuMovieButton::ButtonCallback :144426 selects sprite1 while idle,
    // sprite0 while focused/selected. The Movie chapter only owns the glow.
    unsigned sprite = entry.sprites[0];
    if (stateArtwork && chapter != 1 && chapter != 3) { sprite = entry.sprites[1]; }
    std::string text = label;
    // CMenuMovieButton::Init :144942: optional resource-authored ^fN font prefix.
    if (text.size() >= 4 && text.compare(0, 2, "^f") == 0 && text[2] >= '0' && text[2] <= '9') {
        font = static_cast<unsigned>(text[2] - '0');
        text.erase(0, 3);
    }
    bool foundGraphic = false, foundTouch = false;
    class ButtonCallback : public IMovieRegionCallback {
    public:
        ButtonCallback(GameMenu &menu, const OriginalMenuEntry &data, const std::string &caption, unsigned face,
            unsigned artwork, bool enabled, bool &hit, bool &graphic, bool &touch) : view(menu), entry(data), text(caption), font(face), sprite(artwork),
            interactive(enabled), pressed(hit), foundGraphic(graphic), foundTouch(touch) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index == 1) {
                foundGraphic = true;
                if (sprite != UINT32_MAX && !view.movies.DrawSprite(sprite >> 16, sprite & 255, 0,
                    region.x, region.y, 1, region.alpha)) { return false; }
                if (!text.empty()) {
                    view.movies.Text(text, region.x + (region.width - view.movies.TextWidth(text, font)) / 2,
                        region.y + (region.height - view.movies.TextHeight(font)) / 2, font, 1, 0, region.alpha);
                }
            }
            if (region.index == 0) {
                foundTouch = true;
                if (interactive && region.alpha > 0) { pressed = view.Hit(region.x, region.y, region.width, region.height); }
            }
            return true;
        }
        GameMenu &view;
        const OriginalMenuEntry &entry;
        const std::string &text;
        unsigned font;
        unsigned sprite;
        bool interactive;
        bool &pressed, &foundGraphic, &foundTouch;
    } callback(view, entry, text, font, sprite, interactive, pressed, foundGraphic, foundTouch);
    // Place the content in its original type-6 layer so later glow layers cover it.
    if (!view.movies.Draw(ordinal, time, area.x, area.y, kMenuWidth, kMenuHeight, 0, area.alpha, &callback)) { return false; }
    // BACK_BUTTON has only an invisible logical region0. Input is updated
    // independently of Draw in CMenuMovieButton :144250; alpha0 is not missing data.
    for (const auto &region : view.movies.Regions(ordinal, time, area.x, area.y, true)) {
        if (region.index == 1) { foundGraphic = true; }
        if (region.index == 0 && !foundTouch) {
            foundTouch = true;
            if (interactive) { pressed = view.Hit(region.x, region.y, region.width, region.height); }
        }
    }
    if (entry.sprites[0] == UINT32_MAX && text.empty()) { foundGraphic = true; }
    return foundGraphic && foundTouch;
}

/** Original callbacks use each MovieRegion and bitmap font without fitting. */
void UpgradeCenteredText(GameMenu &view, const MovieRegion &region, const std::string &text, unsigned font) {
    view.movies.Text(text, region.x + (region.width - view.movies.TextWidth(text, font)) / 2,
        region.y + (region.height - view.movies.TextHeight(font)) / 2, font, 1, 0, region.alpha);
}

/** CMenuUpgradePopup::DrawBodyText :392576: only changed stats, then CRIT.
 * CURRENT is the absolute STORE value; NEXT is the relative percentage change. */
void DrawUpgradeStats(GameMenu &view, const MovieRegion &area, const CStoreItem &item, unsigned level, bool next) {
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
bool DrawMastery(GameMenu &view, MenuState &state, CProfileManager &profile, CResTOCManager &toc,
    PackTables &tables, const std::vector<StoreEntry> &store, const std::vector<WeaponEntry> &weapons,
    const std::filesystem::path &savePath, CPlayerProgress *headerProgress ) {
    const WeaponEntry *weapon = FindMasteryWeapon(weapons, state.masteryWeapon);
    const StoreEntry *item = FindWeaponStore(store, state.masteryWeapon);
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
        if (view.Header(profile, *headerProgress, headerPage) == -3) { return false; }
        view.ExchangeClick(click);
    }
    if (!view.movies.Draw(popup, state.masteryPopup.MovieTime())) { return false; }
    bool buyPressed = false, closePressed = false, swapPressed = false;
    const WeaponEntry *other = nullptr;
    // ShowForGuns :394113 prepares both distinct equipped guns below gold.
    // The same popup and swap action are used from the store and the refinery.
    for (const GameObjectRef &gun : profile.configuration.guns) {
        if (SameObject(gun, state.masteryWeapon)) { continue; }
        const WeaponEntry *candidate = FindMasteryWeapon(weapons, gun);
        if (candidate != nullptr && candidate->data.GetMasteryLevel(profile.GetWeaponExperience(gun)) < kMaxMasteryLevel &&
            FindWeaponStore(store, gun) != nullptr) { other = candidate; break; }
    }
    for (const MovieRegion &region : view.movies.Regions(popup, state.masteryPopup.MovieTime())) {
        if (region.alpha <= 0) { continue; }
        switch (region.index) {
        case kUpgradePortraitRegion: {
            MovieRegion bounds;
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
                MovieRegion center = region;
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

/** Resource printf substitution for the original CGame result strings. */
std::string PostGameFormat(GameMenu &view, const char *name, const std::vector<std::string> &values) {
    std::string text = view.movies.NamedString(name);
    std::size_t cursor = 0;
    for (const auto &value : values) {
        cursor = text.find('%', cursor);
        if (cursor == std::string::npos || cursor + 1 >= text.size()) { return {}; }
        const char type = text[cursor + 1];
        if (type != 'i' && type != 'd' && type != 'u' && type != 's') {
            std::printf("[postgame] unsupported format resource=%s value=%s\n", name, text.c_str());
            return {};
        }
        text.replace(cursor, 2, value);
        cursor += value.size();
    }
    return text;
}

/** MENU_POST_GAME_WRAPUP VA0x403350, CMenuPostGame :164559..166204.
 * Native menu/provider logic below; layouts, fonts and artwork stay in BIG. */
bool DrawOriginalPostGame(GameMenu &view, MenuState &state, CResTOCManager &toc, PackTables &tables,
    const CProfileManager &profile) {
    const unsigned ordinal = view.movies.Ordinal("GLU_MOVIE_WRAPUP_SCREEN");
    const auto *movie = view.movies.GetMovie(ordinal);
    unsigned idleStart = 0, idleEnd = 0;
    if (movie == nullptr || !movie->GetChapterRange(1, idleStart, idleEnd)) { return false; }
    if (!state.postGame.postGameBound) {
        state.postGame.postGameBound = true;
        state.postGame.postGameTime = 0;
        state.postGame.postGameItemTime = 0;
        state.postGame.postGameIconTime = 0;
        view.ResetPostGameEffects();
        state.postGame.postGameCloseTime = 0;
        state.postGame.postGameLastTick = view.clock;
        state.postGame.postGameGalleryPosition = 0;
        if (state.result.casualties.size() <= 2) { state.postGame.postGameGalleryPosition = -1; }
        state.postGame.postGameGalleryVelocity = 0;
        // Provider74 walks flattened ENEMY order, not the order of first kills.
        for (std::size_t item = 1; item < state.result.casualties.size(); ++item) {
            std::size_t cursor = item;
            while (cursor > 0) {
                const auto &left = state.result.casualties[cursor - 1].resource;
                const auto &right = state.result.casualties[cursor].resource;
                const int leftPack = toc.GetPackIndexFromHash(left.packHash);
                const int rightPack = toc.GetPackIndexFromHash(right.packHash);
                if (leftPack < rightPack || (leftPack == rightPack && left.localIndex <= right.localIndex)) { break; }
                std::swap(state.result.casualties[cursor - 1], state.result.casualties[cursor]);
                --cursor;
            }
        }
    }
    const unsigned delta = static_cast<unsigned>(view.clock - state.postGame.postGameLastTick);
    state.postGame.postGameDelta = delta;
    state.postGame.postGameLastTick = view.clock;
    state.postGame.postGameTime += delta;
    state.postGame.postGameItemTime += delta;
    // CMenuPostGame::UpdateCurrentView :165242 updates active controls only.
    if (state.page == 27) {
        state.postGame.postGameIconTime += delta;
        unsigned lastIcon = 4;
        if (state.result.horde) { lastIcon = 5; }
        for (unsigned icon : {0u, 1u, lastIcon}) {
            if (!view.AdvancePostGameEffect(icon, delta)) { return false; }
        }
    }
    if (state.postGame.postGameTime > idleEnd) { state.postGame.postGameTime = idleStart + (state.postGame.postGameTime - idleStart) % (idleEnd - idleStart + 1); }
    const bool ready = state.postGame.postGameTime >= idleStart && !state.postGame.postGameClosing;
    const auto *back = OriginalMenuData("MDS_BUTTON_POSTGAME_BACK", 0);
    if (back == nullptr) { return false; }
    const auto *backMovie = view.movies.GetMovie(view.movies.Ordinal(back->movies[0]));
    unsigned exitStart = 0, exitEnd = 0;
    unsigned hideStart = 0, hideEnd = 0;
    if (backMovie == nullptr || !backMovie->GetChapterRange(1, exitStart, exitEnd) ||
        !backMovie->GetChapterRange(0, hideStart, hideEnd)) { return false; }
    const unsigned pressedDuration = exitEnd - exitStart + 1;
    if (state.postGame.postGameClosing) {
        state.postGame.postGameCloseTime += delta;
        if (state.postGame.postGameCloseTime >= pressedDuration + hideEnd - hideStart + 1) {
            // DoAction38 :94446 chooses original menu20 if ore remains, 19 otherwise.
            unsigned target = 0;
            if (profile.xplodium != 0) { target = 3; }
            state.postGame.postGameMusic = false;
            state.Navigate(target, true);
            return true;
        }
    }
    class MainCallbacks : public IMovieRegionCallback {
    public:
        MainCallbacks(GameMenu &menu, MenuState &selection, CResTOCManager &manager, PackTables &resources, bool enabled)
            : view(menu), state(selection), toc(manager), tables(resources), interactive(enabled) {}
        bool DrawMovieRegion(const MovieRegion &region) override {
            if (region.index == 1) {
                const auto *first = OriginalMenuData("MDS_BUTTON_POSTGAME_INFO", 0);
                if (first == nullptr) { return false; }
                MovieRegion bounds;
                if (!view.movies.Region(view.movies.Ordinal(first->movies[0]), 0, 0, bounds)) { return false; }
                // CategoryCallback ARM native gap=2; CMenuMovieButton origin is top-left.
                float x = region.x + static_cast<int>(region.width) / 2 - static_cast<int>((bounds.width + 2) * 2) / 2;
                for (unsigned index = 0; index < 2; ++index) {
                    const auto *entry = OriginalMenuData("MDS_BUTTON_POSTGAME_INFO", index);
                    if (entry == nullptr) { return false; }
                    MovieRegion origin = region;
                    origin.x = x;
                    bool pressed = false;
                    unsigned chapter = 2;
                    if (state.page == 27 + index) { chapter = 3; }
                    unsigned time = UINT32_MAX;
                    if (state.postGame.postGameClosing) {
                        // OnExit -> Hide reverses chapter0, not chapter1's press burst.
                        const auto *button = view.movies.GetMovie(view.movies.Ordinal(entry->movies[0]));
                        const auto *back = OriginalMenuData("MDS_BUTTON_POSTGAME_BACK", 0);
                        if (back == nullptr) { return false; }
                        const auto *backMovie = view.movies.GetMovie(view.movies.Ordinal(back->movies[0]));
                        unsigned pressStart = 0, pressEnd = 0;
                        if (backMovie == nullptr || !backMovie->GetChapterRange(1, pressStart, pressEnd)) { return false; }
                        unsigned begin = 0, end = 0;
                        if (button == nullptr || !button->GetChapterRange(0, begin, end)) { return false; }
                        if (state.postGame.postGameCloseTime > pressEnd - pressStart) {
                            chapter = 0;
                            const unsigned elapsed = state.postGame.postGameCloseTime - (pressEnd - pressStart + 1);
                            time = end - std::min(elapsed, end - begin);
                        }
                    }
                    if (!DrawOriginalMovieButton(view, *entry, origin, view.movies.NamedString(entry->strings[0]), 5,
                        interactive, pressed, chapter, state.postGame.postGameItemTime, time, true)) { return false; }
                    if (pressed) { state.page = 27 + index; }
                    x += bounds.width + 2;
                }
            } else if (region.index == 2) {
                std::string title;
                const auto &result = state.result;
                if (result.horde) {
                    const char *name = "IDS_WRAPUP_SCORE";
                    if (result.score != 0 && result.score == result.highScore) { name = "IDS_WRAPUP_NEW_HIGH_SCORE"; }
                    title = PostGameFormat(view, name, {std::to_string(result.score)});
                } else if (result.wavesPerRevolution > 0) {
                    unsigned revolution = result.wave / result.wavesPerRevolution + 1;
                    unsigned wave = result.wave % result.wavesPerRevolution + 1;
                    if (result.wave == result.waveLimit) { revolution = result.waveLimit / result.wavesPerRevolution + 1; wave = result.wavesPerRevolution; }
                    title = PostGameFormat(view, "IDS_WRAPUP_REVOLUTION_WAVE", {std::to_string(revolution), std::to_string(wave)});
                }
                UpgradeCenteredText(view, region, title, 6);
            } else if (region.index == 3) {
                std::string text;
                if (state.result.horde) {
                    const unsigned seconds = state.result.stopwatchMs / 1000;
                    char time[32];
                    // CUtility::TimeToString flags1,1; ARM string VA0x3c3048.
                    std::snprintf(time, sizeof(time), "%.2u:%.2u:%.2u", seconds / 3600, seconds / 60 % 60, seconds % 60);
                    text = PostGameFormat(view, "IDS_WRAPUP_SURVIVAL_TIME", {time});
                } else { text = PostGameFormat(view, "IDS_WRAPUP_WAVE_CLEARED", {std::to_string(state.result.waves)}); }
                UpgradeCenteredText(view, region, text, 0);
            } else if (region.index == 5) {
                const std::string text = view.movies.NamedString("IDS_WRAPUP_TOTAL_KILLS") + std::to_string(static_cast<std::uint16_t>(state.result.kills));
                view.movies.Text(text, region.x, region.y + (region.height - view.movies.TextHeight(0)) / 2, 0, 1, 0, region.alpha);
            } else if (region.index == 4) {
                PostGameListCallbacks callback(view, state, toc, tables);
                const char *name = "GLU_MOVIE_WRAPUP_MENU_SCROLL";
                if (state.page == 28) { name = "GLU_MOVIE_WRAPUP_GALLERY"; }
                const unsigned list = view.movies.Ordinal(name);
                const auto *listMovie = view.movies.GetMovie(list);
                unsigned start = 0, end = 0;
                if (listMovie == nullptr || !listMovie->GetChapterRange(1, start, end)) { return false; }
                // Show selects the final overview row then settles on row0.
                unsigned time = std::min(state.postGame.postGameItemTime, start);
                if (state.page == 27) { view.Clip(0, region.y, kMenuWidth, region.height); }
                else {
                    unsigned secondStart = 0, secondEnd = 0;
                    if (!listMovie->GetChapterRange(2, secondStart, secondEnd) || secondStart <= start) { return false; }
                    const unsigned duration = secondStart - start;
                    // CalculateBaseVelocity :141749 averages the authored travel
                    // of ALL type>=2 regions between chapter1 and chapter2.
                    float distance = 0;
                    unsigned changed = 0;
                    for (const auto &first : view.movies.Regions(list, start)) {
                        if (first.type < 2) { continue; }
                        MovieRegion last;
                        if (!view.movies.Region(list, first.index, secondStart, last)) { return false; }
                        const int travel = static_cast<int>(first.x + first.width / 2 - last.x - last.width / 2);
                        if (travel != 0) { distance += travel; ++changed; }
                    }
                    if (changed == 0 || distance == 0) { return false; }
                    distance = static_cast<float>(std::abs(static_cast<int>(distance) / static_cast<int>(changed)));
                    const float seconds = state.postGame.postGameDelta / 1000.0f;
                    if (interactive && view.MouseIn(region.x, region.y, region.width, region.height)) {
                        // Wheel is the Windows adapter for one original list option.
                        state.postGame.postGameGalleryPosition -= view.window.TakeWheelDelta();
                        if (view.dragX != 0 && seconds > 0) {
                            const float speed = view.dragX / seconds / (distance * 1000 / duration);
                            state.postGame.postGameGalleryVelocity = std::clamp(-speed, -5.0f, 5.0f);
                        }
                    }
                    if (interactive && seconds > 0) {
                        state.postGame.postGameGalleryPosition += state.postGame.postGameGalleryVelocity * state.postGame.postGameDelta / duration;
                        if (!view.window.IsLeftMouseDown()) {
                            const float slowing = 250000.0f / duration * seconds * seconds / 2;
                            if (state.postGame.postGameGalleryVelocity > 0) { state.postGame.postGameGalleryVelocity = std::max(0.0f, state.postGame.postGameGalleryVelocity - slowing); }
                            else { state.postGame.postGameGalleryVelocity = std::min(0.0f, state.postGame.postGameGalleryVelocity + slowing); }
                        }
                    }
                    // SetBoundsOptions(1,1), Init offset=1; a single enemy is centered.
                    const float maximum = static_cast<float>(std::max(0, static_cast<int>(state.result.casualties.size()) - 2) - 1);
                    const float minimum = std::min(0.0f, maximum);
                    state.postGame.postGameGalleryPosition = std::clamp(state.postGame.postGameGalleryPosition, minimum, maximum);
                    const float fraction = state.postGame.postGameGalleryPosition - std::floor(state.postGame.postGameGalleryPosition);
                    time = start + static_cast<unsigned>(fraction * duration);
                }
                const bool drawn = view.movies.Draw(list, time, region.x, region.y, kMenuWidth, kMenuHeight, 0, region.alpha, &callback);
                if (state.page == 27) { view.EndClip(); }
                return drawn;
            }
            return true;
        }
        GameMenu &view;
        MenuState &state;
        CResTOCManager &toc;
        PackTables &tables;
        bool interactive;
    } callback(view, state, toc, tables, ready);
    if (!view.movies.Draw(ordinal, state.postGame.postGameTime, 512, 384, kMenuWidth, kMenuHeight, 0, 1, &callback)) { return false; }
    MovieRegion position;
    if (!view.movies.Region(ordinal, 0, state.postGame.postGameTime, position)) { return false; }
    position.x += static_cast<int>(position.width) / 2;
    position.y += static_cast<int>(position.height) / 2;
    bool pressed = false;
    unsigned time = UINT32_MAX;
    unsigned chapter = 0;
    if (ready) { chapter = 2; }
    if (state.postGame.postGameClosing) {
        chapter = 1;
        time = exitStart + state.postGame.postGameCloseTime;
        if (state.postGame.postGameCloseTime >= pressedDuration) {
            chapter = 0;
            time = hideEnd - std::min(state.postGame.postGameCloseTime - pressedDuration, hideEnd - hideStart);
        }
    }
    if (!DrawOriginalMovieButton(view, *back, position, {}, 0, ready, pressed, chapter, state.postGame.postGameItemTime, time)) { return false; }
    if (pressed) { state.postGame.postGameClosing = true; state.postGame.postGameCloseTime = 0; }
    if (ready && state.postGame.postGameUpgradePending) {
        state.postGame.postGameUpgradePending = false;
        state.Navigate(26);
    }
    return true;
}

/** Standard intervals occupy 6..11; premium intervals occupy 0..5. */

/** Original menu provider 69; strings are BIG resources except the native
 * GetTimeIntervalString printf patterns at ARM VA 0x3c4980/0x3c49c4. */
std::string RefineryNumber(GameMenu &view, const char *name, std::uint64_t value) {
    std::string text = view.movies.NamedString(name);
    std::size_t offset = text.find("%i");
    if (offset == std::string::npos) { offset = text.find("%d"); }
    if (offset == std::string::npos) {
        std::printf("[refinery] unsupported number format resource=%s text=%s\n", name, text.c_str());
        return {};
    }
    text.replace(offset, 2, std::to_string(value));
    offset = text.find("%%");
    if (offset != std::string::npos) { text.replace(offset, 2, "%"); }
    return text;
}
} // namespace MenuDetail
