#include "gun_bros_re/ui/MenuInternal.h"
#include "TestOutput.h"
#include "ui/MenuChecks.h"
using namespace MenuDetail;

/** Exercise real card resources and the same renderer/input path as --game.
 * All money, XP, mutated templates and profile writes below are test fixtures. */
int CheckStoreCards(CResTOCManager &toc, PackTables &tables, CProfileManager &profile,
    const CPlayerProgress::Template &progress, const CRefinementManager::Template &refinement,
    const std::vector<StoreEntry> &store, const std::vector<WeaponEntry> &weapons,
    const std::vector<ArmorEntry> &armor) {
    MenuTestClick cardClick, purchaseClick, previewClick;
    unsigned start = 0, end = 0;
    {
        GameMenu probe;
        if (!probe.Open(toc, tables)) { return 1; }
        const CMovie *mastery = probe.movies.GetMovie(probe.movies.Ordinal("GLU_MOVIE_MASTERY"));
        if (mastery == nullptr) { return 1; }
        unsigned masteryCases = 0;
        for (const WeaponEntry &weapon : weapons) {
            GameObjectRef weaponRef;
            weaponRef.packHash = weapon.packHash;
            weaponRef.localIndex = weapon.ordinal;
            if (FindWeaponStore(store, weaponRef) == nullptr) { continue; }
            unsigned lower = 0;
            for (unsigned tier = 0; tier < kMaxMasteryLevel; ++tier) {
                const unsigned upper = weapon.data.GetMasteryThreshold(tier);
                if (upper <= lower) { return 1; }
                unsigned start = 0, end = 0, atStart = 0, belowNext = 0, atNext = 0;
                if (!mastery->GetChapterRange(tier + 1, start, end) ||
                    !StoreMasteryTarget(*mastery, weapon.data, lower, atStart) ||
                    !StoreMasteryTarget(*mastery, weapon.data, upper - 1, belowNext) ||
                    !StoreMasteryTarget(*mastery, weapon.data, upper, atNext)) { return 1; }
                // Last XP before a tier must leave the authored gap; reaching
                // the threshold jumps to the following chapter (or full duration).
                if (atStart != start || belowNext < atStart || belowNext >= end || atNext <= belowNext) { return 1; }
                if (tier == kMaxMasteryLevel - 1 && atNext != mastery->duration) { return 1; }
                CMovie changed = *mastery;
                for (unsigned &chapter : changed.chapters) { chapter += 13; }
                changed.duration += 13;
                unsigned shifted = 0;
                if (!StoreMasteryTarget(changed, weapon.data, upper - 1, shifted) || shifted != belowNext + 13) { return 1; }
                lower = upper;
                ++masteryCases;
            }
        }
        std::printf("[store-card-check] mastery tier-boundaries resource-shift cases=%u failures=0\n", masteryCases);
        MovieRegion playerRegion;
        if (!probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_MENU"), kStorePlayerRegion, 0, playerRegion)) { return 1; }
        // Follow an actual authored region, including capture beyond its edges.
        CMenuMesh rotation;
        const float touchX = std::ceil(playerRegion.x + playerRegion.width / 2);
        const float touchY = std::ceil(playerRegion.y + playerRegion.height / 2);
        rotation.UpdateRotation(0, true, touchX, touchY, playerRegion.x, playerRegion.y, playerRegion.width, playerRegion.height, true);
        rotation.UpdateRotation(16, true, touchX + 100, touchY, playerRegion.x, playerRegion.y, playerRegion.width, playerRegion.height, true);
        const float expectedDegrees = 360.0f - 100.0f / playerRegion.width * 180.0f;
        if (std::abs(rotation.GetDegrees() - expectedDegrees) > 0.001f) { return 1; }
        rotation.UpdateRotation(1000, false, touchX, touchY, playerRegion.x, playerRegion.y, playerRegion.width, playerRegion.height, true);
        if (rotation.GetDegrees() != 0) { return 1; }
        rotation.UpdateRotation(16, true, playerRegion.x - 10, touchY, playerRegion.x, playerRegion.y, playerRegion.width, playerRegion.height, true);
        rotation.UpdateRotation(16, true, touchX, touchY, playerRegion.x, playerRegion.y, playerRegion.width, playerRegion.height, true);
        if (rotation.GetDegrees() != 0) { return 1; }
        std::printf("[store-card-check] original-rotation drag-origin region-width release-return outside-start failures=0\n");
        probe.verifyPlayerProjection = true;
        probe.Begin(14);
        if (!probe.DrawEquippedPlayer(toc, tables, profile, weapons, armor, 0, nullptr, &playerRegion)) { return 1; }
        // Perturb the authored region only in this fixture, across both brothers
        // and one retail gun per category. No viewport or framing constant may win.
        MovieRegion changedRegion = playerRegion;
        changedRegion.x -= 53;
        changedRegion.y += 19;
        changedRegion.width *= 0.8f;
        changedRegion.height *= 0.65f;
        for (unsigned brother = 0; brother < 2; ++brother) {
            CProfileManager modelProfile = profile;
            modelProfile.playerBrother = brother;
            std::array<bool, kWeaponCategoryCount> checked{};
            for (const WeaponEntry &weapon : weapons) {
                if (!weapon.hasStoreEntry || weapon.visualOnly || weapon.category < 0 || weapon.category >= kWeaponCategoryCount || checked[weapon.category]) { continue; }
                GameObjectRef ref;
                ref.packHash = weapon.packHash;
                ref.localIndex = weapon.ordinal;
                if (FindWeaponStore(store, ref) == nullptr) { continue; }
                modelProfile.configuration.guns[0] = ref;
                probe.Begin(14);
                std::printf("[store-player-check] brother=%u weapon=%s category=%d mutated-region=1\n", brother, weapon.name.c_str(), weapon.category);
                if (!probe.DrawEquippedPlayer(toc, tables, modelProfile, weapons, armor, 0, nullptr, &changedRegion)) { return 1; }
                checked[weapon.category] = true;
            }
            for (bool found : checked) { if (!found) { return 1; } }
        }
        // Imported equipment catches UI states absent from the simple fixtures.
        CProfileManager savedProfile = profile;
        const auto savedPath = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) / ("model-native-" + std::to_string(GetTickCount64()));
        if (!LoadNativeProfile(toc, tables, savedProfile, savedPath, TestOutput::Fixtures())) { return 1; }
        for (unsigned slot = 0; slot < 2; ++slot) {
            for (unsigned phase = 0; phase < 2; ++phase) {
                probe.Begin(14);
                if (phase == 1) {
                    for (unsigned step = 0; step < 300; ++step) { probe.AdvancePlayerPreview(16); }
                }
                if (!probe.DrawEquippedPlayer(toc, tables, savedProfile, weapons, armor, slot, nullptr, &playerRegion)) { return 1; }
                const auto image = TestOutput::Path("ui-original-2026-09-09/native-model-") + std::to_string(slot) + "-" + std::to_string(phase) + ".png";
                if (!GB_SAVE_FRAME(probe.window, image)) { return 1; }
                const auto &brother = probe.GetPlayerPreview()->weapon->brother;
                std::printf("[store-player-check] native slot=%u phase=%u state=%d torso-move=%d time=%d\n", slot, phase,
                    brother.GetStateId(), brother.GetTorso().GetMoveIndex(), brother.GetTorso().GetAnimation().GetTimeMs());
            }
        }
        // Original PLAYER flow fixture: 17 -> 18 -> 19/native 3 -> 17.
        // A switch must preserve the actor and the outgoing torso until the
        // next sequence consumes the incoming gun's move overrides.
        CBrother *originalActor = &probe.GetPlayerPreview()->weapon->brother;
        for (unsigned exchange = 0; exchange < 3; ++exchange) {
            const unsigned oldSlot = probe.GetPlayerPreviewSlot();
            const unsigned targetSlot = 1 - oldSlot;
            PlayerModel &model = *probe.GetPlayerPreview();
            PlayerWeaponState *oldWeapon = model.uiActiveWeapon;
            if (oldWeapon == nullptr) { oldWeapon = model.weapon.get(); }
            probe.TakePlayerPreviewSlotChange();
            if (!probe.DrawEquippedPlayer(toc, tables, savedProfile, weapons, armor, targetSlot, nullptr, &playerRegion)) { return 1; }
            if (&model.weapon->brother != originalActor || probe.GetPlayerPreviewSlot() != oldSlot) { return 1; }
            unsigned elapsed = 0;
            while (probe.GetPlayerPreviewSlot() == oldSlot && elapsed < 10000) {
                probe.AdvancePlayerPreview(16);
                elapsed += 16;
            }
            if (probe.GetPlayerPreviewSlot() != targetSlot || originalActor->GetStateId() != 19 ||
                model.uiActiveWeapon == oldWeapon || !probe.TakePlayerPreviewSlotChange() || probe.TakePlayerPreviewSlotChange()) { return 1; }
            const CMesh *switchTorso = originalActor->GetTorso().GetAnimation().GetMesh();
            bool outgoingTorso = !originalActor->TorsoUsesWeapon();
            for (const auto &part : oldWeapon->configs) { if (&part->mesh == switchTorso) { outgoingTorso = true; } }
            if (!outgoingTorso) { return 1; }
            while (originalActor->GetStateId() != 17 && elapsed < 10000) {
                probe.AdvancePlayerPreview(16);
                elapsed += 16;
            }
            bool incomingTorso = !originalActor->TorsoUsesWeapon();
            for (const auto &part : model.uiActiveWeapon->configs) {
                if (&part->mesh == originalActor->GetTorso().GetAnimation().GetMesh()) { incomingTorso = true; }
            }
            if (originalActor->GetStateId() != 17 || !incomingTorso) { return 1; }
            savedProfile.activeWeaponSlot = targetSlot;
            if (!savedProfile.SaveToDisk(savedPath)) { return 1; }
            CProfileManager restored;
            if (!LoadNativeProfile(toc, tables, restored, savedPath, savedPath / "absent-source") || restored.activeWeaponSlot != targetSlot) { return 1; }
            probe.Begin(14);
            if (!probe.DrawEquippedPlayer(toc, tables, savedProfile, weapons, armor, targetSlot, nullptr, &playerRegion)) { return 1; }
            if (!GB_SAVE_FRAME(probe.window, TestOutput::Path("ui-original-2026-09-09/native-model-swapped-") + std::to_string(exchange) + ".png")) { return 1; }
            std::printf("[store-player-check] native-swap exchange=%u slot=%u actor-preserved=1 outgoing-torso=1 incoming-torso=1 reload=1 elapsed=%u\n",
                exchange, targetSlot, elapsed);
        }
        const auto *swapEntry = OriginalMenuData("MDS_BUTTON_STORE_GUN_SWAP", 0);
        if (swapEntry == nullptr) { return 1; }
        const unsigned swapMovieId = probe.movies.Ordinal(swapEntry->movies[0]);
        const CMovie *swapMovie = probe.movies.GetMovie(swapMovieId);
        unsigned showStart = 0, showEnd = 0, pressStart = 0, pressEnd = 0;
        if (swapMovie == nullptr || !swapMovie->GetChapterRange(0, showStart, showEnd) ||
            !swapMovie->GetChapterRange(1, pressStart, pressEnd)) { return 1; }
        MovieRegion swapParent, swapOrigin;
        if (!probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_MENU"), kStoreGunSwapRegion, 0, swapParent)) { return 1; }
        // Mutated parent verifies that native placement and the child touch box
        // move together. No fitted sprite or hand-written text baseline remains.
        swapParent.x -= 31;
        swapParent.y += 17;
        if (!StoreGunSwapOrigin(probe, swapParent, swapOrigin)) { return 1; }
        MovieRegion swapTouch;
        bool foundTouch = false;
        for (const auto &region : probe.movies.Regions(swapMovieId, showEnd, swapOrigin.x, swapOrigin.y, true)) {
            if (region.index == 0) { swapTouch = region; foundTouch = true; }
        }
        if (!foundTouch) { return 1; }
        MenuState swapState;
        swapState.store.shopGunSlot = probe.GetPlayerPreviewSlot();
        const unsigned beforeSlot = swapState.store.shopGunSlot;
        const MenuTestClick swapClick{swapTouch.x + swapTouch.width / 2, swapTouch.y + swapTouch.height / 2};
        probe.Begin(14); probe.clock = 1000;
        probe.SetTestClick(swapClick);
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.store.shopSwapPhase != 0 || swapState.store.shopGunSlot != beforeSlot) { return 1; }
        probe.Begin(14); probe.clock += showEnd - showStart;
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.store.shopSwapPhase != 2) { return 1; }
        probe.SetTestClick(swapClick);
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.store.shopSwapPhase != 4 || swapState.store.shopGunSlot != beforeSlot) { return 1; }
        probe.Begin(14); probe.clock += pressEnd - pressStart - 1;
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.store.shopGunSlot != beforeSlot) { return 1; }
        probe.Begin(14); ++probe.clock;
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.store.shopGunSlot != 1 - beforeSlot) { return 1; }
        swapState.store.shopFilter = 1;
        // Filtering the GUNS list must preserve the equipped-player slot button.
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.store.shopSwapPhase != 2) {
            std::printf("[store-player-check] FAIL filtered GUNS hides swap button phase=%u\n", swapState.store.shopSwapPhase);
            return 1;
        }
        swapState.store.shopCategory = 1;
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.store.shopSwapPhase != 1) { return 1; }
        probe.Begin(14); probe.clock += showEnd - showStart;
        if (!DrawStoreGunSwap(probe, swapState, swapParent, true) || swapState.store.shopSwapPhase != 8) { return 1; }
        std::printf("[store-player-check] original-swap-button movie=%u region-shift=1 intro-gate=1 press-boundary=1 category-hide=1 failures=0\n", swapMovieId);
        const CMovie *movie = probe.movies.GetMovie(probe.movies.Ordinal("GLU_MOVIE_SHOP_BOX"));
        if (movie == nullptr || !movie->GetChapterRange(1, start, end)) { return 1; }
        MovieRegion column, body;
        if (!probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_SCROLL"), 1, probe.storeRestTime, column) ||
            !probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_SHOP_BOX"), 0, start, body)) { return 1; }
        cardClick = {column.x + body.width / 4, column.y + body.height / 4};
        MovieRegion content, openBody, actions, buyLabel, previewLabel;
        const unsigned card = probe.movies.Ordinal("GLU_MOVIE_SHOP_BOX");
        if (!probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_MENU"), 0, 0, content) ||
            !probe.movies.Region(card, 0, end, openBody)) { return 1; }
        const StoreCardFace face{content.x + content.width / 2 - static_cast<int>(content.width) / 16 - openBody.width / 2,
            content.y + content.height / 2 - openBody.height / 2, 1, end};
        const OriginalMenuEntry *buy = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", 0);
        const OriginalMenuEntry *preview = OriginalMenuData("MDS_BUTTON_STORE_PREVIEW", 0);
        if (buy == nullptr || preview == nullptr || !CardRegion(probe, card, 10, face, actions) ||
            !probe.movies.Region(probe.movies.Ordinal(buy->movies[0]), 1, 0, buyLabel) ||
            !probe.movies.Region(probe.movies.Ordinal(preview->movies[0]), 1, 0, previewLabel)) { return 1; }
        purchaseClick = {actions.x + actions.width - buyLabel.width / 2, actions.y + buyLabel.height / 2};
        previewClick = {actions.x + previewLabel.width / 2, actions.y + previewLabel.height / 2};
        MenuState animation;
        animation.store.shopDetailOpen = true;
        animation.store.shopDetailTime = start;
        probe.clock = 60;
        if (!AdvanceStoreCard(probe, animation, *movie) || animation.store.shopDetailTime != start + 240) { return 1; }
        animation.store.shopDetailClosing = true;
        probe.clock += 10;
        if (!AdvanceStoreCard(probe, animation, *movie) || animation.store.shopDetailTime != start + 200) { return 1; }
        probe.clock += 1000;
        if (!AdvanceStoreCard(probe, animation, *movie) || animation.store.shopDetailOpen) { return 1; }
        CMovie changed = *movie;
        changed.chapters[1] += 32;
        changed.chapters[2] += 64;
        changed.duration += 64;
        animation = MenuState{};
        animation.store.shopDetailOpen = true;
        animation.store.shopDetailTime = changed.chapters[1];
        if (!AdvanceStoreCard(probe, animation, changed) || animation.store.shopDetailTime != end + 64) { return 1; }
        const auto mixed = FormatStoreText(probe.movies, "^f0DMG ^f112 ^f2SPD ^f3+4 ^f450\nNEXT", 1000);
        constexpr unsigned expectedFonts[] = {1, 2, 4, 3, 0};
        if (mixed.size() != 2 || mixed[0].runs.size() != 5) { return 1; }
        for (unsigned index = 0; index < 5; ++index) {
            if (mixed[0].runs[index].font != expectedFonts[index]) { return 1; }
        }
        unsigned templates = 0;
        for (const StoreEntry &entry : store) {
            for (unsigned field = 3; field <= 5; ++field) {
                const std::string original = ReadGameString(toc, entry.data.assets[field]);
                if (original.empty()) { continue; }
                ++templates;
                const std::string expanded = SubstituteStoreStats(original, StoreStatValues(entry.data, 0));
                if (expanded.find('#') != std::string::npos || expanded.find("^i") != std::string::npos) {
                    std::printf("[store-card-check] unresolved text item=%s field=%u text=%s\n", entry.name.c_str(), field, expanded.c_str());
                    return 1;
                }
            }
        }
        // Mutate an in-memory STORE value, then verify the display consumes it.
        for (const StoreEntry &entry : store) {
            if (entry.data.statGroups[1].empty()) { continue; }
            CStoreItem changedItem = entry.data;
            changedItem.statGroups[1][0] = 12345;
            if (SubstituteStoreStats("^f0DMG ^f1#DMG", StoreStatValues(changedItem, 0)) != "^f0DMG ^f112345") { return 1; }
            break;
        }
        std::printf("[store-card-check] templates=%u five-fonts newline changed-STORE changed-chapters 4x reversal failures=0\n", templates);
        std::printf("[store-card-check] currency common=%s rare=%s\n", probe.movies.NamedString("IDS_SHOP_COMMON").c_str(), probe.movies.NamedString("IDS_SHOP_RARE").c_str());
    }
    CProfileManager before = profile;
    constexpr const char *categories[] = {"guns", "armor", "powerups"};
    constexpr unsigned categoryOrder[] = {2, 0, 1};
    for (unsigned category : categoryOrder) {
        const std::string base = std::string(TestOutput::Path("store-card-")) + categories[category];
        MenuState folded, opening, opened, closing, closed;
        MenuState *states[] = {&folded, &opening, &opened, &closing, &closed};
        for (MenuState *state : states) {
            state->page = 2;
            state->store.shopCategory = category;
            const char *table = nullptr;
            const unsigned rows = StoreFilterRows(category, table);
            // Native STORE.type criteria; the previous fixture assumed every
            // category reused gun bit 0 and silently disabled powerup filtering.
            for (unsigned row = 2; row < rows; ++row) {
                const auto *entry = OriginalMenuData(table, row);
                if (entry->action == 65 && entry->parameter < 17) { state->store.shopFilter |= 1u << entry->parameter; }
                if (category != 2) { break; }
            }
        }
        const std::vector<MenuTestClick> idle = {{-100, -100}};
        const std::vector<MenuTestClick> begin = {cardClick, {-100, -100, 60}};
        const std::vector<MenuTestClick> open = {cardClick, {-100, -100, 2500}};
        const std::vector<MenuTestClick> reverse = {cardClick, {-100, -100, 2500}, {10, 700}, {-100, -100, 60}};
        const std::vector<MenuTestClick> finish = {cardClick, {-100, -100, 2500}, {10, 700}, {-100, -100, 250}};
        const std::vector<MenuTestClick> *clicks[] = {&idle, &begin, &open, &reverse, &finish};
        constexpr const char *phases[] = {"folded", "opening", "open", "closing", "closed"};
        for (unsigned phase = 0; phase < 5; ++phase) {
            const std::string image = base + "-" + phases[phase] + ".png";
            if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
                *states[phase], TestOutput::Path("store-card-check.dat"), image.c_str(), clicks[phase]) != -2) { return 1; }
        }
        if (!opening.store.shopDetailOpen || opening.store.shopDetailTime != start + 240 || !opened.store.shopDetailOpen ||
            opened.store.shopDetailTime != end || !closing.store.shopDetailClosing || closing.store.shopDetailTime != end - 240 ||
            closed.store.shopDetailOpen || opened.selectedItem < 0) { return 1; }
        const StoreEntry &entry = store[opened.selectedItem];
        if (category != 0) {
            CProfileManager buyer = profile;
            buyer.coins = 2ull * entry.data.commonPrice;
            buyer.warbucks = 2ull * entry.data.rarePrice;
            MenuState previewState = opened;
            const std::vector<MenuTestClick> previewActions = {{-100, -100, 2500}, previewClick};
            if (category == 1) {
                if (ShowGameMenu(toc, tables, buyer, progress, refinement, store, weapons, armor, previewState,
                    TestOutput::Path("store-card-preview.dat"), base + "-preview.png", &previewActions) != -2 || !previewState.store.shopPreview ||
                    buyer.inventory.size() != profile.inventory.size()) { return 1; }
            }
            // Exercise the real purchase failure, including its original button table.
            CProfileManager poorBuyer = profile;
            poorBuyer.coins = 0;
            poorBuyer.warbucks = 0;
            MenuState insufficient = opened;
            const std::vector<MenuTestClick> insufficientActions = {{-100, -100, 2500}, purchaseClick};
            if (ShowGameMenu(toc, tables, poorBuyer, progress, refinement, store, weapons, armor, insufficient,
                TestOutput::Path("store-card-insufficient.dat"), base + "-insufficient.png", &insufficientActions) != -2 ||
                insufficient.failedPrice == 0 || insufficient.storePromptButtons == nullptr ||
                std::string(insufficient.storePromptButtons) != "MDS_BUTTON_STORE_PROMPT") { return 1; }
            std::printf("[store-card-check] insufficient category=%s original-three-buttons=1 failures=0\n", categories[category]);
            MenuState purchase = opened;
            std::vector<MenuTestClick> purchaseActions = {{-100, -100, 2500}, purchaseClick};
            if (category == 2) { purchaseActions.push_back(purchaseClick); }
            const std::filesystem::path path = base + "-purchase.dat";
            if (ShowGameMenu(toc, tables, buyer, progress, refinement, store, weapons, armor, purchase,
                path, base + "-purchased.png", &purchaseActions) != -2) { return 1; }
            const GameObjectTypeRef &object = entry.data.objects[0];
            if (category == 1 && (!buyer.Owns(object.type, object.object) ||
                !SameObject(Equipped(buyer, purchase.slot), object.object))) { return 1; }
            // The first powerup is the byte-checked five-charge Speed Boost pack.
            if (category == 2 && (buyer.GetPowerupCount(object.object) != 10 || buyer.warbucks != 0)) {
                std::printf("[store-card-check] powerup purchase count=%u expected=10 warbucks=%llu expected=0\n",
                    buyer.GetPowerupCount(object.object), static_cast<unsigned long long>(buyer.warbucks));
                return 1;
            }
            CProfileManager reloaded = profile;
            if (!reloaded.LoadFromDisk(path) || reloaded.coins != buyer.coins || reloaded.warbucks != buyer.warbucks ||
                reloaded.GetPowerupCount(object.object) != buyer.GetPowerupCount(object.object)) { return 1; }
            std::printf("[store-card-check] category=%s expanded-purchase preview reload failures=0\n", categories[category]);
        }
        std::printf("[store-card-check] category=%s item=%s folded=%s expanded=%s screenshots=5 failures=0\n",
            categories[category], entry.name.c_str(), ReadGameString(toc, entry.data.assets[5]).c_str(),
            ReadGameString(toc, entry.data.assets[4]).c_str());
    }
    if (profile.coins != before.coins || profile.warbucks != before.warbucks || profile.inventory.size() != before.inventory.size()) { return 1; }
    for (unsigned slot = 0; slot < 5; ++slot) {
        if (!SameObject(Equipped(profile, slot), Equipped(before, slot))) { return 1; }
    }
    std::printf("[store-card-check] no-purchase unchanged-loadout failures=0\n");
    // Full native menu path: actual child-button click -> authored press ->
    // PLAYER native 3 -> DrawStore save. No direct profile mutation in the driver.
    const auto nativeSwapPath = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) /
        ("store-native-click-" + std::to_string(GetTickCount64()));
    CProfileManager nativeSwapProfile;
    if (!LoadNativeProfile(toc, tables, nativeSwapProfile, nativeSwapPath, TestOutput::Fixtures())) { return 1; }
    const unsigned originalSlot = nativeSwapProfile.activeWeaponSlot;
    MenuTestClick nativeSwapClick;
    unsigned showDuration = 0, pressDuration = 0;
    {
        GameMenu probe;
        if (!probe.Open(toc, tables)) { return 1; }
        MovieRegion parent, origin;
        const auto *entry = OriginalMenuData("MDS_BUTTON_STORE_GUN_SWAP", 0);
        if (entry == nullptr || !probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_MENU"), kStoreGunSwapRegion, 0, parent) ||
            !StoreGunSwapOrigin(probe, parent, origin)) { return 1; }
        const unsigned movieId = probe.movies.Ordinal(entry->movies[0]);
        const CMovie *movie = probe.movies.GetMovie(movieId);
        unsigned start = 0, end = 0;
        if (movie == nullptr || !movie->GetChapterRange(0, start, end)) { return 1; }
        showDuration = end - start;
        if (!movie->GetChapterRange(1, start, end)) { return 1; }
        pressDuration = end - start;
        bool found = false;
        for (const auto &region : probe.movies.Regions(movieId, showDuration, origin.x, origin.y, true)) {
            if (region.index != 0) { continue; }
            nativeSwapClick = {region.x + region.width / 2, region.y + region.height / 2};
            found = true;
        }
        if (!found) { return 1; }
    }
    MenuState nativeSwapState;
    nativeSwapState.page = 2;
    nativeSwapState.store.shopGunSlot = originalSlot;
    nativeSwapState.store.shopFilter = kOwnedFilterBit;
    std::vector<MenuTestClick> nativeSwapActions{{-100, -100, 1}, {-100, -100, showDuration + 1},
        nativeSwapClick, {-100, -100, pressDuration + 1}};
    for (unsigned frame = 0; frame < 120; ++frame) { nativeSwapActions.push_back({-100, -100, 16}); }
    if (ShowGameMenu(toc, tables, nativeSwapProfile, progress, refinement, store, weapons, armor,
        nativeSwapState, nativeSwapPath, TestOutput::Path("ui-original-2026-09-09/native-store-swap-click.png"), &nativeSwapActions) != -2) { return 1; }
    CProfileManager swapReloaded;
    if (nativeSwapProfile.activeWeaponSlot != 1 - originalSlot || !LoadNativeProfile(toc, tables, swapReloaded,
        nativeSwapPath, nativeSwapPath / "absent-source") || swapReloaded.activeWeaponSlot != 1 - originalSlot) { return 1; }
    std::printf("[store-card-check] native real-button filtered-GUNS authored-press player-Flow active-slot=%u saved-reload=1 failures=0\n", swapReloaded.activeWeaponSlot);
    // Visual research fixture only: IMG_0800's silver/blue rifle may be the
    // catalog's Infinity Laser. Do not replace the real account's loadout or
    // treat this unconfirmed image match as a runtime weapon rule.
    // Infinity Laser was visually rejected. Keep that attempt above recorded;
    // enumerate the original rifle/laser categories for a same-weapon match.
    // No rifle/laser matched IMG_0800. Include every original category: visual
    // appearance is not reliable evidence of the STORE classification.
    if (TestOutput::referenceGallery) {
    for (const WeaponEntry &weapon : weapons) {
        const std::string referenceKey = std::to_string(weapon.packHash) + "-" + std::to_string(weapon.ordinal);
        const auto referencePath = nativeSwapPath / "reference-gun-fixture" / referenceKey;
        CProfileManager reference;
        if (!LoadNativeProfile(toc, tables, reference, referencePath, TestOutput::Fixtures())) { return 1; }
        reference.configuration.guns[reference.activeWeaponSlot].packHash = weapon.packHash;
        reference.configuration.guns[reference.activeWeaponSlot].localIndex = static_cast<std::uint8_t>(weapon.ordinal);
        MenuState referenceState;
        referenceState.page = 2;
        referenceState.store.shopGunSlot = reference.activeWeaponSlot;
        const std::string referenceImage = TestOutput::Path("ui-original-2026-09-09/reference-gun-") + referenceKey + ".png";
        if (ShowGameMenu(toc, tables, reference, progress, refinement, store, weapons, armor, referenceState, referencePath,
            referenceImage) != -2) { return 1; }
        std::printf("[store-player-research] reference-image-candidate=%s resource=%u:%u fixture-only image-match-unconfirmed=1\n",
            weapon.name.c_str(), weapon.packHash, weapon.ordinal);
    }
    }
    return 0;
}

int RunUpgradePopupCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadStoreCatalog(toc, tables, store) ||
        !LoadWeaponCatalog(toc, tables, weapons)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    MenuState state;
    state.page = 26;
    state.masteryWeapon = profile.configuration.guns[0];
    const WeaponEntry *weapon = FindMasteryWeapon(weapons, state.masteryWeapon);
    const StoreEntry *item = FindWeaponStore(store, state.masteryWeapon);
    if (weapon == nullptr || item == nullptr || item->data.statGroups[7].size() < 2) { return 1; }
    const unsigned threshold = weapon->data.GetMasteryThreshold(0);
    const unsigned initialXP = threshold / 2;
    // Explicit test-only money/XP; this profile never touches saves/ or userdata/.
    profile.warbucks = 10000;
    profile.AddWeaponExperience(state.masteryWeapon, initialXP, weapon->data.GetMasteryLimit());
    const std::filesystem::path path = TestOutput::Path("ui-original-2026-09-09/upgrade-profile.dat");
    if (!profile.SaveToDisk(path)) { return 1; }
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    const unsigned popupOrdinal = view.movies.Ordinal("GLU_MOVIE_UPGRADE_POPUP");
    const CMovie *popup = view.movies.GetMovie(popupOrdinal);
    const CMovie *stars = view.movies.GetMovie(view.movies.Ordinal("GLU_MOVIE_WEAPON_UPGRADE_MASTERY"));
    if (popup == nullptr || stars == nullptr) { return 1; }
    unsigned openStart = 0, openEnd = 0, closeStart = 0, closeEnd = 0, idleStart = 0, idleEnd = 0;
    if (!popup->GetChapterRange(0, openStart, openEnd) || !popup->GetChapterRange(1, idleStart, idleEnd) ||
        !popup->GetChapterRange(2, closeStart, closeEnd)) { return 1; }
    unsigned initialTarget = 0, upgradedTarget = 0;
    if (!CMenuUpgradePopup::StarsTarget(*stars, weapon->data, initialXP, initialTarget) ||
        !CMenuUpgradePopup::StarsTarget(*stars, weapon->data, threshold, upgradedTarget)) { return 1; }

    CMovie changedPopup = *popup, changedStars = *stars;
    for (unsigned &chapter : changedPopup.chapters) { chapter *= 2; }
    changedPopup.duration *= 2;
    for (unsigned &chapter : changedStars.chapters) { chapter *= 3; }
    changedStars.duration *= 3;
    CMenuUpgradePopup mutation;
    if (!mutation.Bind(changedPopup, changedStars, weapon->data, initialXP)) { return 1; }
    unsigned changedStart = 0, changedEnd = 0;
    if (!changedPopup.GetChapterRange(0, changedStart, changedEnd) || mutation.TargetTime() == initialTarget) { return 1; }
    mutation.Update(changedEnd - changedStart);
    if (mutation.GetState() != CMenuUpgradePopup::State::Opening || mutation.StarsTime() != 0) { return 1; }
    mutation.Update(1);
    if (mutation.GetState() != CMenuUpgradePopup::State::Ready || mutation.StarsTime() != 0) { return 1; }
    mutation.Update(123);
    if (mutation.StarsTime() != 123) { return 1; }
    std::printf("[upgrade-check] mutated-chapters opening-no-stars 1x-fill failures=0\n");

    MovieRegion buyArea, closeArea;
    if (!view.movies.Region(popupOrdinal, kUpgradeBuyRegion, idleStart, buyArea) ||
        !view.movies.Region(popupOrdinal, kUpgradeCloseRegion, idleStart, closeArea)) { return 1; }
    const OriginalMenuEntry *buy = OriginalMenuData("MDS_BUTTON_STORE_UPGRADE", 2);
    const OriginalMenuEntry *close = OriginalMenuData("MDS_BUTTON_STORE_UPGRADE", 0);
    if (buy == nullptr || close == nullptr) { return 1; }
    MovieRegion buyTouch, closeTouch;
    bool foundBuy = false, foundClose = false;
    for (const MovieRegion &region : view.movies.Regions(view.movies.Ordinal(buy->movies[0]), 0, buyArea.x, buyArea.y)) {
        if (region.index == 0) { buyTouch = region; foundBuy = true; }
    }
    for (const MovieRegion &region : view.movies.Regions(view.movies.Ordinal(close->movies[0]), 0, closeArea.x, closeArea.y)) {
        if (region.index == 0) { closeTouch = region; foundClose = true; }
    }
    if (!foundBuy || !foundClose) { return 1; }
    struct Step { unsigned delta; const char *name; unsigned click; };
    const unsigned opening = openEnd - openStart + 1, closing = closeEnd - closeStart + 1;
    const unsigned fill = upgradedTarget - initialTarget;
    const Step steps[] = {{0, "opening", 0}, {opening / 2, "opening-half", 1},
        {opening - opening / 2, "opened", 0}, {initialTarget / 2, "fill-half", 0},
        {initialTarget, "filled", 0}, {0, "upgrade-start", 1}, {fill / 2, "upgrade-half", 1},
        {fill - fill / 2, "flash", 0}, {125, "flash-half", 0}, {125, "ready", 0},
        {0, "close", 2}, {closing / 2, "closing", 0}, {closing - closing / 2, "closed", 0}};
    unsigned index = 0;
    for (const Step &step : steps) {
        view.clock += step.delta;
        view.Begin(26);
        if (step.click == 1) { view.SetTestClick({buyTouch.x + buyTouch.width / 2, buyTouch.y + buyTouch.height / 2}); }
        if (step.click == 2) { view.SetTestClick({closeTouch.x + closeTouch.width / 2, closeTouch.y + closeTouch.height / 2}); }
        if (!DrawMastery(view, state, profile, toc, tables, store, weapons, path)) { return 1; }
        if (index < 3 && state.masteryPopup.StarsTime() != 0) { return 1; }
        if (index < 5 && profile.GetWeaponExperience(state.masteryWeapon) != initialXP) { return 1; }
        if (index == 4 && state.masteryPopup.StarsTime() != initialTarget) { return 1; }
        if (index == 5 && state.masteryPopup.GetState() != CMenuUpgradePopup::State::Upgrading) { return 1; }
        if (index == 6 && state.masteryPopup.DisplayExperience() != initialXP) { return 1; }
        if (index == 7 && state.masteryPopup.GetState() != CMenuUpgradePopup::State::Flash) { return 1; }
        if (index == 9 && state.masteryPopup.GetState() != CMenuUpgradePopup::State::Ready) { return 1; }
        if (index == 12 && state.page == 26) { return 1; }
        const std::string capture = std::string(TestOutput::Path("ui-original-2026-09-09/upgrade-")) + step.name + ".png";
        if (glGetError() != 0 || !GB_SAVE_FRAME(view.window, capture)) { return 1; }
        view.window.Present();
        std::printf("[upgrade-check] phase=%s movie=%u stars=%u state=%u\n", step.name,
            state.masteryPopup.MovieTime(), state.masteryPopup.StarsTime(), static_cast<unsigned>(state.masteryPopup.GetState()));
        ++index;
    }
    CProfileManager restored;
    restored.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!restored.LoadFromDisk(path) || restored.GetWeaponExperience(state.masteryWeapon) != threshold ||
        restored.warbucks != 10000 - item->data.statGroups[7][1]) { return 1; }
    std::printf("[upgrade-check] phases=%u one-purchase opening-and-upgrading-block-input reload failures=0\n", index);
    // Reopen the real panel, then buy silver and gold. Gold closes only after
    // the star movie reaches its target and the native 250 ms flash completes.
    state.Navigate(26);
    view.Begin(26);
    if (!DrawMastery(view, state, profile, toc, tables, store, weapons, path)) { return 1; }
    view.clock += opening;
    view.Begin(26);
    if (!DrawMastery(view, state, profile, toc, tables, store, weapons, path)) { return 1; }
    unsigned totalPrice = static_cast<unsigned>(item->data.statGroups[7][1]);
    for (unsigned level = 2; level <= kMaxMasteryLevel; ++level) {
        view.Begin(26);
        view.SetTestClick({buyTouch.x + buyTouch.width / 2, buyTouch.y + buyTouch.height / 2});
        if (!DrawMastery(view, state, profile, toc, tables, store, weapons, path) ||
            state.masteryPopup.GetState() != CMenuUpgradePopup::State::Upgrading) { return 1; }
        view.clock += stars->duration;
        view.Begin(26);
        if (!DrawMastery(view, state, profile, toc, tables, store, weapons, path) ||
            state.masteryPopup.GetState() != CMenuUpgradePopup::State::Flash) { return 1; }
        view.clock += 250;
        view.Begin(26);
        if (!DrawMastery(view, state, profile, toc, tables, store, weapons, path)) { return 1; }
        totalPrice += static_cast<unsigned>(item->data.statGroups[7][level]);
        if (level < kMaxMasteryLevel && state.masteryPopup.GetState() != CMenuUpgradePopup::State::Ready) { return 1; }
        if (level == kMaxMasteryLevel && state.masteryPopup.GetState() != CMenuUpgradePopup::State::Closing) { return 1; }
    }
    view.clock += closing;
    view.Begin(26);
    if (!DrawMastery(view, state, profile, toc, tables, store, weapons, path) || state.page == 26 ||
        !restored.LoadFromDisk(path) || restored.GetWeaponExperience(state.masteryWeapon) != weapon->data.GetMasteryThreshold(2) ||
        restored.warbucks != 10000 - totalPrice) { return 1; }
    std::printf("[upgrade-check] reopen silver gold auto-close total-price=%u reload failures=0\n", totalPrice);
    // Exercise the nested original modal against an isolated native account.
    CProfileManager poorProfile;
    poorProfile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto fundsPath = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) /
        ("upgrade-funds-native-" + std::to_string(GetTickCount64()));
    if (!LoadNativeProfile(toc, tables, poorProfile, fundsPath, TestOutput::Fixtures())) { return 1; }
    poorProfile.warbucks = 0;
    poorProfile.weaponMastery.clear();
    MenuState fundsState;
    fundsState.page = 26;
    fundsState.masteryWeapon = state.masteryWeapon;
    const unsigned price = static_cast<unsigned>(item->data.statGroups[7][1]);
    const int offer = FindCurrencyOffer(store, 1, price);
    if (offer < 0 || store[offer].data.rarePrice < price) { return 1; }
    const CMovie *prompt = view.movies.GetMovie(view.movies.Ordinal("GLU_MOVIE_POPUP"));
    if (prompt == nullptr) { return 1; }
    const unsigned durations[] = {0, opening + 1, 0, prompt->duration, 0, prompt->duration,
        prompt->duration, 0, prompt->duration, 0, 2000, 2000, prompt->duration, prompt->duration, 0};
    for (unsigned phase = 0; phase < 15; ++phase) {
        view.clock += durations[phase];
        view.Begin(26);
        if (phase == 2 || phase == 7 || phase == 14) {
            view.SetTestClick({buyTouch.x + buyTouch.width / 2, buyTouch.y + buyTouch.height / 2});
        }
        if (phase == 4 || phase == 9) {
            unsigned buttonRegion = 3;
            if (phase == 9) { buttonRegion = 4; }
            MovieRegion touch;
            if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_POPUP"), buttonRegion,
                fundsState.storePopup.MovieTime(), touch)) { return 1; }
            view.SetTestClick({touch.x + touch.width / 2, touch.y + touch.height / 2});
        }
        if (!CompleteOfflineIAP(view.clock, fundsState, poorProfile, store, fundsPath) ||
            !DrawMastery(view, fundsState, poorProfile, toc, tables, store, weapons, fundsPath) ||
            !DrawStorePrompt(view, fundsState)) { return 1; }
        if (phase <= 10 && (poorProfile.warbucks != 0 || poorProfile.GetWeaponExperience(fundsState.masteryWeapon) != 0)) { return 1; }
        if (phase == 3 && (!fundsState.storePopup.IsReady() || fundsState.failedPrice != price ||
            fundsState.failedMissing != price || fundsState.currencyOffer != offer)) { return 1; }
        if ((phase == 6 || phase == 13) && fundsState.storePopup.IsActive()) { return 1; }
        if (phase == 9 && !fundsState.currencyPending) { return 1; }
        if (phase >= 11 && phase < 14 && poorProfile.warbucks != store[offer].data.rarePrice) { return 1; }
        if (phase == 3 || phase == 10 || phase == 14) {
            const auto capture = TestOutput::Path("ui-original-2026-09-09/upgrade-funds-") + std::to_string(phase) + ".png";
            if (glGetError() != 0 || !GB_SAVE_FRAME(view.window, capture)) { return 1; }
        }
        std::printf("[upgrade-funds-check] phase=%u pending=%u modal=%u rare=%llu xp=%u\n", phase,
            fundsState.currencyPending, fundsState.storePopup.IsActive(), poorProfile.warbucks,
            poorProfile.GetWeaponExperience(fundsState.masteryWeapon));
    }
    if (poorProfile.warbucks != store[offer].data.rarePrice - price ||
        poorProfile.GetWeaponExperience(fundsState.masteryWeapon) != threshold ||
        !poorProfile.LoadFromDisk(fundsPath) || poorProfile.warbucks != store[offer].data.rarePrice - price ||
        poorProfile.GetWeaponExperience(fundsState.masteryWeapon) != threshold) { return 1; }
    std::printf("[upgrade-funds-check] original-prompt dismiss offer wait retry-upgrade native-reload failures=0\n");
    poorProfile.configuration.guns[0] = state.masteryWeapon;
    const GameObjectRef secondGun = poorProfile.configuration.guns[1];
    const WeaponEntry *secondWeapon = FindMasteryWeapon(weapons, secondGun);
    std::printf("[upgrade-swap-check] second=%u:%u found=%u same=%u store=%u\n", secondGun.packHash, secondGun.localIndex,
        secondWeapon != nullptr, SameObject(secondGun, state.masteryWeapon), FindWeaponStore(store, secondGun) != nullptr);
    if (secondWeapon == nullptr || SameObject(secondGun, state.masteryWeapon)) { return 1; }
    // The imported second gun is already gold. Native reload preserves records
    // absent from the projected vector; reset the isolated fixture explicitly.
    poorProfile.weaponMastery.clear();
    poorProfile.AddWeaponExperience(state.masteryWeapon, threshold, weapon->data.GetMasteryLimit());
    poorProfile.AddWeaponExperience(secondGun, secondWeapon->data.GetMasteryThreshold(0) / 2, secondWeapon->data.GetMasteryLimit());
    for (unsigned slot : {1u, 0u}) {
        poorProfile.activeWeaponSlot = slot;
        SurvivalGameContext context{poorProfile, fundsPath, 0};
        MenuState resultState;
        BeginPostGame(resultState, context, weapons);
        if (!SameObject(resultState.masteryWeapon, poorProfile.configuration.guns[slot])) {
            std::printf("[upgrade-active-slot-check] expected-slot=%u selected=%u:%u\n", slot,
                resultState.masteryWeapon.packHash, resultState.masteryWeapon.localIndex);
            return 1;
        }
        resultState.Navigate(26);
        view.Begin(26);
        if (!DrawMastery(view, resultState, poorProfile, toc, tables, store, weapons, fundsPath)) { return 1; }
        view.clock += opening;
        view.Begin(26);
        if (!DrawMastery(view, resultState, poorProfile, toc, tables, store, weapons, fundsPath) ||
            !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/upgrade-postgame-active-") + std::to_string(slot + 1) + ".png")) { return 1; }
        std::printf("[upgrade-active-slot-check] active=%u selected=%u:%u failures=0\n", slot,
            resultState.masteryWeapon.packHash, resultState.masteryWeapon.localIndex);
    }
    const auto fundsBeforeSwap = poorProfile.warbucks;
    MenuState swapState;
    swapState.page = 26;
    swapState.masteryWeapon = state.masteryWeapon;
    view.Begin(26);
    if (!DrawMastery(view, swapState, poorProfile, toc, tables, store, weapons, fundsPath)) {
        std::printf("[upgrade-swap-check] opening draw failed\n"); return 1;
    }
    view.clock += opening;
    view.Begin(26);
    if (!DrawMastery(view, swapState, poorProfile, toc, tables, store, weapons, fundsPath)) {
        std::printf("[upgrade-swap-check] ready draw failed\n"); return 1;
    }
    MovieRegion swapArea, swapTouch;
    const auto *swap = OriginalMenuData("MDS_BUTTON_STORE_UPGRADE", 1);
    if (swap == nullptr || !view.movies.Region(popupOrdinal, 10, swapState.masteryPopup.MovieTime(), swapArea)) { return 1; }
    // Match DrawOriginalMovieButton: child origin is the parent region center.
    bool foundSwapTouch = false;
    for (const auto &region : view.movies.Regions(view.movies.Ordinal(swap->movies[0]), 0,
        swapArea.x + swapArea.width / 2, swapArea.y + swapArea.height / 2)) {
        if (region.index == 0) { swapTouch = region; foundSwapTouch = true; break; }
    }
    if (!foundSwapTouch) { return 1; }
    const MenuTestClick swapClick{swapTouch.x + swapTouch.width / 2, swapTouch.y + swapTouch.height / 2};
    if (!GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/upgrade-swap-1.png"))) { return 1; }
    const unsigned beforeSwapTime = swapState.masteryPopup.MovieTime();
    std::printf("[upgrade-swap-check] click=%.1f/%.1f movie=%u state=%u alpha=%.3f input=%u xp=%u limit=%u\n", swapClick.x, swapClick.y, beforeSwapTime,
        static_cast<unsigned>(swapState.masteryPopup.GetState()), swapArea.alpha, view.inputEnabled,
        poorProfile.GetWeaponExperience(secondGun), secondWeapon->data.GetMasteryLimit());
    view.Begin(26);
    view.SetTestClick(swapClick);
    if (!DrawMastery(view, swapState, poorProfile, toc, tables, store, weapons, fundsPath) ||
        !SameObject(swapState.masteryWeapon, secondGun) || swapState.masteryPopup.MovieTime() != beforeSwapTime ||
        swapState.masteryPopup.StarsTime() != 0 || poorProfile.warbucks != fundsBeforeSwap ||
        !SameObject(poorProfile.configuration.guns[0], state.masteryWeapon)) {
        std::printf("[upgrade-swap-check] selected=%u:%u time=%u stars=%u\n", swapState.masteryWeapon.packHash,
            swapState.masteryWeapon.localIndex, swapState.masteryPopup.MovieTime(), swapState.masteryPopup.StarsTime()); return 1;
    }
    view.clock += 100;
    view.Begin(26);
    if (!DrawMastery(view, swapState, poorProfile, toc, tables, store, weapons, fundsPath) ||
        swapState.masteryPopup.StarsTime() != std::min(100u, swapState.masteryPopup.TargetTime())) { return 1; }
    if (!GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/upgrade-swap.png"))) { return 1; }
    view.Begin(26);
    view.SetTestClick(swapClick);
    if (!DrawMastery(view, swapState, poorProfile, toc, tables, store, weapons, fundsPath) ||
        !SameObject(swapState.masteryWeapon, poorProfile.configuration.guns[0]) ||
        poorProfile.warbucks != fundsBeforeSwap) { return 1; }
    view.Begin(26);
    if (!DrawMastery(view, swapState, poorProfile, toc, tables, store, weapons, fundsPath) ||
        !GB_SAVE_FRAME(view.window, TestOutput::Path("ui-original-2026-09-09/upgrade-swap-back-1.png"))) { return 1; }
    std::printf("[upgrade-swap-check] store-entry distinct-guns stars-reset no-reopen unchanged-loadout failures=0\n");
    return 0;
}

/** Real bank card/input/prompt path, with native saves and isolated fixtures. */
int CheckBank(CResTOCManager &toc, PackTables &tables, const CPlayerProgress::Template &progress,
    const CRefinementManager::Template &refinement, const std::vector<StoreEntry> &store,
    const std::vector<WeaponEntry> &weapons, const std::vector<ArmorEntry> &armor) {
    MenuTestClick buyClick;
    MenuTestClick dismissClick;
    float columnPitch = 0, rowPitch = 0;
    std::vector<std::pair<int, unsigned>> currencies;
    for (unsigned index = 0; index < store.size(); ++index) {
        const auto &item = store[index].data;
        if (item.type >= 14 && item.type <= 16 && item.displayOrder >= 0 && item.value242 != 1) {
            currencies.push_back({item.displayOrder, index});
        }
    }
    std::sort(currencies.begin(), currencies.end());
    {
        GameMenu probe;
        if (!probe.Open(toc, tables)) { return 1; }
        MovieRegion slot, price, button;
        StoreCardFace face;
        if (!probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_SCROLL"), kFirstColumnRegion, probe.storeRestTime, slot)) { return 1; }
        MovieRegion secondColumn, promptTouch;
        if (!probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_STORE_SCROLL"), kFirstColumnRegion + 1, probe.storeRestTime, secondColumn) ||
            !probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_POPUP"), 0, 500, promptTouch)) { return 1; }
        columnPitch = secondColumn.x - slot.x;
        rowPitch = slot.height / 2 + 5;
        dismissClick = {promptTouch.x + promptTouch.width / 2, promptTouch.y + promptTouch.height / 2, 100};
        face.x = slot.x;
        face.y = slot.y;
        const auto *entry = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", kBuyButtonEntry);
        if (entry == nullptr || !CardRegion(probe, probe.movies.Ordinal("GLU_MOVIE_SHOP_BOX"), kCardPriceRegion, face, price) ||
            !probe.movies.Region(probe.movies.Ordinal(entry->movies[0]), 1, 0, button)) { return 1; }
        buyClick = {price.x + price.width - button.width / 2, price.y + price.height - button.height / 2};
        constexpr unsigned parameters[] = {17, 14, 15, 16};
        for (unsigned index = 0; index < 4; ++index) {
            entry = OriginalMenuData("MDS_BUTTON_STORE_SORT_CURRENCY", index);
            if (entry == nullptr || entry->action != 65 || entry->parameter != parameters[index]) { return 1; }
            std::printf("[bank-check] original-filter row=%u label=%s parameter=%u\n", index,
                probe.movies.NamedString(entry->strings[0]).c_str(), entry->parameter);
        }
    }
    const auto root = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) / ("bank-check-" + std::to_string(GetTickCount64()));
    for (unsigned phase = 0; phase < 9 + currencies.size(); ++phase) {
        CProfileManager profile;
        profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
        const auto path = root / std::to_string(phase);
        if (!LoadNativeProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
        MenuState state;
        state.page = 2;
        state.store.shopCategory = 3;
        state.store.shopGunSlot = profile.activeWeaponSlot;
        state.store.shopFilter = 1u << 14;
        if (phase == 2) { state.store.shopFilter = 1u << 15; }
        if (phase >= 3) { state.store.shopFilter = 1u << 16; }
        if (phase == 4 || phase == 7 || phase == 8) { profile.coins = 0; profile.warbucks = 0; }
        const auto coins = profile.coins;
        const auto bucks = profile.warbucks;
        std::vector<StoreEntry> fixture = store;
        unsigned first = static_cast<unsigned>(fixture.size());
        if (phase >= 9) {
            first = currencies[phase - 9].second;
            state.store.shopFilter = 1u << fixture[first].data.type;
        }
        for (unsigned index = 0; index < fixture.size(); ++index) {
            const auto &item = fixture[index].data;
            if (item.type < 14 || item.type > 16 || item.displayOrder < 0 || item.value242 == 1 ||
                (state.store.shopFilter & (1u << item.type)) == 0) { continue; }
            if (phase >= 9) { continue; }
            if (phase >= 6 && item.value32 == 0) { continue; }
            if (first == fixture.size() || item.displayOrder < fixture[first].data.displayOrder) { first = index; }
        }
        if (first == fixture.size()) { return 1; }
        unsigned position = 0;
        for (const auto &row : currencies) {
            if (row.second == first) { break; }
            if ((state.store.shopFilter & (1u << (fixture[row.second].data.type))) != 0) { ++position; }
        }
        state.store.shopScroll = (position / 2) * columnPitch;
        MenuTestClick cardClick = buyClick;
        cardClick.y += (position % 2) * rowPitch;
        if (phase == 5) { fixture[first].data.commonPrice += 321; }
        std::vector<MenuTestClick> clicks = {cardClick, {30, 90, 2000}};
        if (phase >= 3) { clicks[1] = {-100, -100, 2000}; }
        if (phase != 0) { clicks.push_back({-100, -100, 4000}); clicks.push_back({-100, -100, 1000}); }
        if (phase == 8) {
            clicks.push_back(dismissClick);
            clicks.push_back({-100, -100, 2000});
            clicks.push_back({-100, -100, 2000});
        }
        const auto capture = std::filesystem::path(TestOutput::Path("ui-original-2026-09-09")) / ("bank-phase-" + std::to_string(phase) + ".png");
        if (ShowGameMenu(toc, tables, profile, progress, refinement, fixture, weapons, armor, state, path, capture.string(), &clicks) != -2 ||
            state.page != 2 || state.store.shopCategory != 3) { return 1; }
        if (phase == 0) {
            if (!state.currencyPending || profile.coins != coins || profile.warbucks != bucks || !state.storePopup.IsReady()) { return 1; }
        } else {
            auto expectedCoins = coins;
            auto expectedBucks = bucks;
            const auto &item = fixture[first].data;
            const bool insufficient = phase == 4 || phase == 7 || phase == 8;
            if (!insufficient) {
                if (item.type == 14) { expectedCoins += item.commonPrice; }
                else if (item.type == 15) { expectedBucks += item.rarePrice; }
                else if (item.value32 != 0) { expectedCoins -= item.commonPrice; expectedBucks += item.rarePrice; }
                else { expectedCoins += item.commonPrice; expectedBucks -= item.rarePrice; }
            }
            if (state.currencyPending || profile.coins != expectedCoins || profile.warbucks != expectedBucks) { return 1; }
            if (!insufficient && (!profile.LoadFromDisk(path) || profile.coins != expectedCoins || profile.warbucks != expectedBucks)) { return 1; }
            if ((phase == 4 || phase == 7) && !state.storePopup.IsReady()) { return 1; }
            if (phase == 8 && state.storePopup.IsActive()) { return 1; }
        }
        std::printf("[bank-check] phase=%u type=%u pending=%u common=%llu rare=%llu original-data reload failures=0\n",
            phase, fixture[first].data.type, state.currencyPending, profile.coins, profile.warbucks);
    }
    return 0;
}

/** Focused regression for the user's splash, package and clipped badge report. */
int CheckUiFeedback(CResTOCManager &toc, PackTables &tables, const CPlayerProgress::Template &progress,
    const CRefinementManager::Template &refinement, const std::vector<StoreEntry> &store,
    const std::vector<WeaponEntry> &weapons, const std::vector<ArmorEntry> &armor) {
    const auto root = std::filesystem::path(TestOutput::Path("ui-feedback-2026-09-09"));
    const auto save = root / ("profile-" + std::to_string(GetTickCount64()));
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (!LoadNativeProfile(toc, tables, profile, save, TestOutput::Fixtures())) { return 1; }
    {
        GameMenu view;
        if (!view.Open(toc, tables)) { return 1; }
        MovieRegion viewport, column, content, badge, sprite;
        const unsigned scroll = view.movies.Ordinal("GLU_MOVIE_STORE_SCROLL");
        if (!view.movies.Region(scroll, 0, view.storeRestTime, viewport) ||
            !view.movies.Region(scroll, 1, view.storeRestTime, column) ||
            !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_STORE_MENU"), 0, 0, content)) { return 1; }
        StoreCardFace face;
        face.x = column.x;
        face.y = column.y;
        if (!CardRegion(view, view.movies.Ordinal("GLU_MOVIE_SHOP_BOX"), kCardBadgeRegion, face, badge) ||
            !view.movies.SpriteBounds(0, 88, sprite)) { return 1; }
        const float badgeTop = badge.y + badge.height / 2 + sprite.y;
        std::printf("[ui-feedback-check] content-y=%.1f viewport-y=%.1f column-y=%.1f badge-top=%.1f sprite-height=%.1f\n",
            content.y, viewport.y, column.y, badgeTop, sprite.height);
        if (badgeTop < content.y) {
            std::printf("[ui-feedback-check] first-row badge clipped by %.1f pixels failures=1\n", viewport.y - badgeTop);
            return 1;
        }
    }
    for (unsigned phase = 0; phase < 4; ++phase) {
        MenuState state;
        state.page = 14;
        profile.firstLaunch = phase == 3;
        std::vector<MenuTestClick> clicks;
        if (phase == 0) { clicks.push_back({-1, -1, 1200}); }
        if (phase == 1) { clicks.push_back({-1, -1, 600}); }
        if (phase >= 2) { clicks.push_back({12, 12, 16}); }
        const auto screenshot = root / ("splash-" + std::to_string(phase) + ".png");
        if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state, save,
            screenshot.string(), &clicks) != -2) { return 1; }
        unsigned expectedPage = 14;
        if (phase == 2) { expectedPage = 24; }
        if (phase == 3) { expectedPage = 25; }
        if (state.page != expectedPage) { return 1; }
        std::printf("[ui-feedback-check] splash-phase=%u page=%u wait blink click original-entry failures=0\n", phase, state.page);
    }
    profile.firstLaunch = false;
    const StoreEntry *package = nullptr;
    unsigned packageIndex = 0;
    for (unsigned index = 0; index < store.size(); ++index) {
        if (store[index].data.singlePurchase != 0) { package = &store[index]; packageIndex = index; break; }
    }
    if (package == nullptr) { return 1; }
    MenuTestClick packageCard, buyPackage;
    {
        GameMenu view;
        if (!view.Open(toc, tables)) { return 1; }
        MovieRegion column, body, right, label;
        StoreCardFace face;
        const auto *entry = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", kBuyButtonEntry);
        if (entry == nullptr || !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_STORE_SCROLL"), 2, view.storeRestTime, column)) { return 1; }
        face.x = column.x; face.y = column.y;
        const unsigned box = view.movies.Ordinal("GLU_MOVIE_SHOP_BOX");
        if (!CardRegion(view, box, kCardBodyRegion, face, body) || !CardRegion(view, box, kCardRightRegion, face, right) ||
            !view.movies.Region(view.movies.Ordinal(entry->movies[0]), 1, 0, label)) { return 1; }
        packageCard = {body.x + 10, body.y + 10, 1000};
        buyPackage = {right.x + right.width - label.width / 2, right.y + right.height - label.height / 2, 1000};
        if (label.width <= right.width) { buyPackage.x = right.x + right.width / 2; }
    }
    for (unsigned phase = 0; phase < 7; ++phase) {
        MenuState state;
        state.page = 2;
        state.store.shopGunSlot = profile.activeWeaponSlot;
        if (phase == 0 || phase == 1) { state.store.shopCategory = 2; }
        if (phase == 2) { state.store.shopCategory = 1; }
        std::vector<MenuTestClick> clicks{{-1, -1, 1000}};
        if (phase == 1 || phase == 2 || phase == 3 || phase == 6) { clicks.push_back(packageCard); }
        if (phase == 4) { clicks.push_back(buyPackage); }
        const auto screenshot = root / ("store-" + std::to_string(phase) + ".png");
        if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor, state, save,
            screenshot.string(), &clicks) != -2) { return 1; }
        if (phase == 3 && state.selectedItem != static_cast<int>(packageIndex)) { return 1; }
        if ((phase == 1 || phase == 2 || phase == 6) && state.selectedItem == static_cast<int>(packageIndex)) { return 1; }
        if (phase == 4 && (!profile.IsPackagePurchased(package->ref) || state.store.shopDetailOpen)) { return 1; }
        if (phase == 5) {
            // A disk reload in the same session must retain OWNED. Reset models
            // a new process before applying the original purchased-item override.
            profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
            if (!LoadNativeProfile(toc, tables, profile, save) || !profile.IsPackagePurchased(package->ref)) { return 1; }
        }
        std::printf("[ui-feedback-check] store-phase=%u category=%u selected=%d package-purchased=%u failures=0\n",
            phase, state.store.shopCategory, state.selectedItem, profile.IsPackagePurchased(package->ref));
    }
    return 0;
}

/** Fresh inventory and one live menu, including purchase from an expanded card. */
int RunPackagePurchaseCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadStoreCatalog(toc, tables, store) ||
        !LoadWeaponCatalog(toc, tables, weapons) || !LoadArmorCatalog(toc, tables, armor)) { return 1; }
    const StoreEntry *package = nullptr;
    for (const StoreEntry &entry : store) {
        if (entry.data.singlePurchase != 0) { package = &entry; break; }
    }
    if (package == nullptr) { return 1; }
    const auto root = std::filesystem::path(TestOutput::Path("package-purchase-check")) / std::to_string(GetTickCount64());
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = false;
    const unsigned box = view.movies.Ordinal("GLU_MOVIE_SHOP_BOX");
    MovieRegion column, content, body, right, button, actions;
    unsigned start = 0, end = 0;
    const auto *buy = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", kBuyButtonEntry);
    if (buy == nullptr || !view.movies.GetMovie(box)->GetChapterRange(1, start, end) ||
        !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_STORE_SCROLL"), 2, view.storeRestTime, column) ||
        !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_STORE_MENU"), 0, 0, content) ||
        !view.movies.Region(view.movies.Ordinal(buy->movies[0]), 1, 0, button)) { return 1; }
    StoreCardFace folded{column.x, column.y};
    if (!CardRegion(view, box, kCardBodyRegion, folded, body) || !CardRegion(view, box, kCardRightRegion, folded, right)) { return 1; }
    const MenuTestClick openClick{body.x + 10, body.y + 10};
    MenuTestClick foldedBuy{right.x + right.width - button.width / 2, right.y + right.height - button.height / 2};
    if (button.width <= right.width) { foldedBuy.x = right.x + right.width / 2; }
    if (!view.movies.Region(box, kCardBodyRegion, end, body)) { return 1; }
    const StoreCardFace expanded{content.x + content.width / 2 - static_cast<int>(content.width) / 16 - body.width / 2,
        content.y + content.height / 2 - body.height / 2, 1, end};
    if (!CardRegion(view, box, kCardActionRegion, expanded, actions)) { return 1; }
    const MenuTestClick expandedBuy{actions.x + actions.width - button.width / 2, actions.y + button.height / 2};
    unsigned failures = 0;
    for (unsigned mode = 0; mode < 2; ++mode) {
        CProfileManager profile;
        profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
        const auto save = root / std::to_string(mode);
        if (!LoadNativeProfile(toc, tables, profile, save, root / "absent-source")) { return 1; }
        profile.coins = package->data.commonPrice;
        profile.warbucks = package->data.rarePrice;
        profile.activeWeaponSlot = mode;
        const CPlayerConfiguration initialConfiguration = profile.configuration;
        MenuState state;
        state.page = 2;
        state.store.shopGunSlot = mode;
        std::vector<MenuTestClick> clicks{{-1, -1, 1000}};
        if (mode == 1) { clicks.push_back(openClick); clicks.push_back({-1, -1, 1000}); clicks.push_back(expandedBuy); }
        else { clicks.push_back(foldedBuy); }
        clicks.push_back({-1, -1, 1000});
        for (const MenuTestClick &click : clicks) {
            view.clock += click.advanceMs;
            view.Begin(2); view.SetTestClick(click);
            if (!DrawStore(view, toc, tables, profile, package->data.requiredLevel, store, weapons, armor, state, save)) { return 1; }
        }
        if (!GB_SAVE_FRAME(view.window, (save / "after-purchase.png").string())) { return 1; }
        const bool purchased = profile.IsPackagePurchased(package->ref);
        unsigned delivered = 0, equipped = 0, gear = 0;
        for (const auto &object : package->data.objects) {
            if (object.type == 17) {
                unsigned expected = 0;
                for (const auto &other : package->data.objects) {
                    if (other.type == 17 && SameObject(other.object, object.object)) { ++expected; }
                }
                if (profile.GetPowerupCount(object.object) != expected) { ++failures; }
                continue;
            }
            ++gear;
            if (profile.Owns(object.type, object.object)) { ++delivered; }
            if (object.type == 6) {
                for (const auto &gun : profile.configuration.guns) { if (SameObject(gun, object.object)) { ++equipped; break; } }
            } else if (object.type == 2) {
                for (const auto &part : profile.configuration.armor) { if (SameObject(part, object.object)) { ++equipped; break; } }
            }
        }
        if (!purchased || state.store.shopDetailOpen != (mode == 1) || delivered != gear || equipped != gear ||
            profile.IsPackageHidden(package->ref)) { ++failures; }
        unsigned gunIndex = 0, restoredRows = 0;
        for (const auto &object : package->data.objects) {
            if (object.type == 6 && gunIndex < 2) {
                if (!SameObject(profile.configuration.guns[(mode + gunIndex) & 1], object.object)) { ++failures; }
                ++gunIndex;
            }
            if (object.type != 2) { continue; }
            for (const ArmorEntry &part : armor) {
                if (part.packHash == object.object.packHash && part.ordinal == object.object.localIndex &&
                    !SameObject(profile.configuration.armor[part.data.GetSlot()], object.object)) { ++failures; }
            }
            for (const StoreEntry &entry : store) {
                if (entry.data.objects.size() == 1 && entry.data.objects[0].type == 2 &&
                    SameObject(entry.data.objects[0].object, object.object) && entry.data.displayOrder < 0) {
                    if (GetStoreDisplayOrder(entry.data, profile) < 0) { ++failures; }
                    ++restoredRows;
                }
            }
        }
        const auto coins = profile.coins;
        const auto warbucks = profile.warbucks;
        const auto inventory = profile.inventory.size();
        const auto acquiredConfiguration = profile.configuration;
        // Clicking the former BUY area must not acquire or equip again.
        view.clock += 1000; view.Begin(2);
        if (mode == 0) { view.SetTestClick(foldedBuy); }
        else { view.SetTestClick(expandedBuy); }
        if (!DrawStore(view, toc, tables, profile, package->data.requiredLevel, store, weapons, armor, state, save)) { return 1; }
        if (profile.AcquireItem(package->data, package->data.requiredLevel) != PurchaseResult::Owned ||
            profile.coins != coins || profile.warbucks != warbucks || profile.inventory.size() != inventory) { ++failures; }
        // Re-entering categories must preserve the session's OWNED card.
        for (unsigned category : {1u, 2u, 0u}) {
            state = MenuState{};
            state.page = 2; state.store.shopCategory = category; state.store.shopGunSlot = mode;
            view.clock += 1000;
            view.Begin(2); view.SetTestClick(openClick);
            if (!DrawStore(view, toc, tables, profile, package->data.requiredLevel, store, weapons, armor, state, save)) { return 1; }
            bool selectedPackage = state.selectedItem >= 0 && static_cast<unsigned>(state.selectedItem) < store.size() &&
                SameObject(store[state.selectedItem].ref, package->ref);
            if (selectedPackage != (category == 0)) { ++failures; }
        }
        if (!profile.LoadFromDisk(save) || profile.IsPackageHidden(package->ref)) { ++failures; }
        CProfileManager restarted;
        restarted.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
        if (!LoadNativeProfile(toc, tables, restarted, save, root / "absent-source") ||
            !restarted.IsPackageHidden(package->ref)) { return 1; }
        for (unsigned slot = 0; slot < acquiredConfiguration.guns.size(); ++slot) {
            if (!SameObject(restarted.configuration.guns[slot], acquiredConfiguration.guns[slot])) { ++failures; }
        }
        for (unsigned slot = 0; slot < acquiredConfiguration.armor.size(); ++slot) {
            if (!SameObject(restarted.configuration.armor[slot], acquiredConfiguration.armor[slot])) { ++failures; }
        }
        for (const auto &object : package->data.objects) {
            if (object.type == 17) {
                unsigned expected = 0;
                for (const auto &other : package->data.objects) {
                    if (other.type == 17 && SameObject(other.object, object.object)) { ++expected; }
                }
                if (restarted.GetPowerupCount(object.object) != expected || profile.GetPowerupCount(object.object) != expected) { ++failures; }
            } else if (!restarted.Owns(object.type, object.object)) { ++failures; }
        }
        state = MenuState{}; state.page = 2; state.store.shopGunSlot = mode;
        view.clock += 1000; view.Begin(2); view.SetTestClick(openClick);
        if (!DrawStore(view, toc, tables, restarted, package->data.requiredLevel, store, weapons, armor, state, save)) { return 1; }
        if (state.selectedItem >= 0 && static_cast<unsigned>(state.selectedItem) < store.size() &&
            SameObject(store[state.selectedItem].ref, package->ref)) { ++failures; }
        if (!GB_SAVE_FRAME(view.window, (save / "after-restart.png").string())) { return 1; }
        // Owned bundle-only armor remains selectable after changing equipment.
        restarted.configuration = initialConfiguration;
        std::vector<std::pair<int, unsigned>> ownedArmor;
        for (unsigned index = 0; index < store.size(); ++index) {
            const auto &item = store[index].data;
            if (item.objects.size() != 1 || item.objects[0].type != 2 || item.value242 == 1 ||
                !restarted.Owns(2, item.objects[0].object)) { continue; }
            const int order = GetStoreDisplayOrder(item, restarted);
            if (order >= 0) { ownedArmor.push_back({order, index}); }
        }
        std::sort(ownedArmor.begin(), ownedArmor.end());
        MovieRegion firstColumn, secondColumn, equipButton;
        const auto *equipEntry = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", kEquipButtonEntry);
        const unsigned scroll = view.movies.Ordinal("GLU_MOVIE_STORE_SCROLL");
        if (equipEntry == nullptr || !view.movies.Region(scroll, kFirstColumnRegion, view.storeRestTime, firstColumn) ||
            !view.movies.Region(scroll, kFirstColumnRegion + 1, view.storeRestTime, secondColumn) ||
            !view.movies.Region(view.movies.Ordinal(equipEntry->movies[0]), 1, 0, equipButton)) { return 1; }
        unsigned armorClicks = 0;
        for (unsigned position = 0; position < ownedArmor.size(); ++position) {
            const auto &item = store[ownedArmor[position].second].data;
            if (item.displayOrder >= 0) { continue; }
            state = MenuState{}; state.page = 2; state.store.shopCategory = 1;
            state.store.shopGunSlot = mode; state.store.shopFilter = kOwnedFilterBit;
            state.store.shopScroll = (position / 2) * (secondColumn.x - firstColumn.x);
            const StoreCardFace face{firstColumn.x, firstColumn.y + (position % 2) * (firstColumn.height / 2 + 5)};
            MovieRegion price;
            if (!CardRegion(view, box, kCardPriceRegion, face, price)) { return 1; }
            view.clock += 1000; view.Begin(2);
            view.SetTestClick({price.x + price.width - equipButton.width / 2, price.y + price.height - equipButton.height / 2});
            if (!DrawStore(view, toc, tables, restarted, package->data.requiredLevel, store, weapons, armor, state, save)) { return 1; }
            ++armorClicks;
        }
        for (unsigned slot = 0; slot < acquiredConfiguration.armor.size(); ++slot) {
            if (!SameObject(restarted.configuration.armor[slot], acquiredConfiguration.armor[slot])) { ++failures; }
        }
        if (armorClicks != restoredRows) { ++failures; }
        state = MenuState{}; state.page = 2; state.store.shopCategory = 1; state.store.shopFilter = kOwnedFilterBit; state.store.shopGunSlot = mode;
        view.clock += 1000; view.Begin(2); view.SetTestClick({-1, -1});
        if (!DrawStore(view, toc, tables, restarted, package->data.requiredLevel, store, weapons, armor, state, save) ||
            !GB_SAVE_FRAME(view.window, (save / "armor-re-equipped.png").string())) { return 1; }
        std::printf("[package-purchase-check] expanded=%u purchased=%u detail-open=%u gear-delivered=%u/%u gear-equipped=%u/%u failures=%u\n",
            mode, purchased, state.store.shopDetailOpen, delivered, gear, equipped, gear, failures);
        std::printf("[package-purchase-check] restored-armor-rows=%u session-owned restart-hidden loadout-reloaded repeat-rejected failures=%u\n", restoredRows, failures);
        std::printf("[package-purchase-check] armor-equip-clicks=%u failures=%u\n", armorClicks, failures);
    }
    return failures != 0;
}

int RunStoreTemplateCheck(const std::string &bigDirectory, bool cardsOnly, bool bankOnly, bool feedbackOnly) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CPlayerProgress::Template progress;
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    if (!LoadPlayerProgress(toc, tables, progress) || !LoadRefinementTemplate(toc, tables, refinement) ||
        !LoadStoreCatalog(toc, tables, store) || !LoadWeaponCatalog(toc, tables, weapons) ||
        !LoadArmorCatalog(toc, tables, armor)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    if (bankOnly) { return CheckBank(toc, tables, progress, refinement, store, weapons, armor); }
    if (feedbackOnly) { return CheckUiFeedback(toc, tables, progress, refinement, store, weapons, armor); }
    if (cardsOnly) { return CheckStoreCards(toc, tables, profile, progress, refinement, store, weapons, armor); }
    unsigned closedStart = 0, closedEnd = 0, slideStart = 0, slideEnd = 0;
    MovieRegion closedButton, openButton, openPanel, optionLabel;
    {
        GameMenu probe;
        if (!probe.Open(toc, tables)) { return 1; }
        const unsigned ordinal = probe.movies.Ordinal("GLU_MOVIE_SORT_BAR");
        const CMovie *movie = probe.movies.GetMovie(ordinal);
        if (movie == nullptr || !movie->GetChapterRange(0, closedStart, closedEnd) ||
            !movie->GetChapterRange(1, slideStart, slideEnd)) { return 1; }
        // Byte-checked regression for this shipped asset; runtime uses its data.
        if (closedEnd != 100 || slideStart != 101 || slideEnd != 276) { return 1; }
        if (!probe.movies.Region(ordinal, kSortButtonRegion, closedEnd, closedButton) ||
            !probe.movies.Region(ordinal, kSortButtonRegion, slideEnd, openButton) ||
            !probe.movies.Region(ordinal, kSortPanelRegion, slideEnd, openPanel) ||
            !probe.movies.Region(probe.movies.Ordinal("GLU_MOVIE_BUTTON_LG"), 1, 0, optionLabel)) { return 1; }
        MenuState playback;
        if (!AdvanceStoreFilter(probe, playback, *movie)) { return 1; }
        playback.store.shopFilterOpen = true;
        probe.clock = (slideEnd - closedEnd) / 2;
        if (!AdvanceStoreFilter(probe, playback, *movie)) { return 1; }
        const unsigned middle = playback.store.shopFilterTime;
        MovieRegion movingButton;
        if (!probe.movies.Region(ordinal, kSortButtonRegion, middle, movingButton) ||
            movingButton.y <= openButton.y || movingButton.y >= closedButton.y) { return 1; }
        playback.store.shopFilterOpen = false;
        ++probe.clock;
        if (!AdvanceStoreFilter(probe, playback, *movie) || playback.store.shopFilterTime != middle - 1) { return 1; }
        playback.store.shopFilterOpen = true;
        probe.clock += movie->duration;
        if (!AdvanceStoreFilter(probe, playback, *movie) || playback.store.shopFilterTime != slideEnd) { return 1; }
        playback.store.shopFilterOpen = false;
        probe.clock += movie->duration;
        if (!AdvanceStoreFilter(probe, playback, *movie) || playback.store.shopFilterTime != slideStart) { return 1; }
        // Perturb only an in-memory copy: playback must follow changed chapters.
        // This distinguishes resource-driven bounds from the old 101/277 constants.
        CMovie changed = *movie;
        changed.chapters[1] += 20;
        changed.chapters[2] += 20;
        MenuState changedPlayback;
        if (!AdvanceStoreFilter(probe, changedPlayback, changed) || changedPlayback.store.shopFilterTime != closedEnd + 20) { return 1; }
        changedPlayback.store.shopFilterOpen = true;
        probe.clock += changed.duration;
        if (!AdvanceStoreFilter(probe, changedPlayback, changed) || changedPlayback.store.shopFilterTime != slideEnd + 20) { return 1; }
        unsigned unusedStart = 0, unusedEnd = 0;
        if (movie->GetChapterRange(static_cast<unsigned>(movie->chapters.size()), unusedStart, unusedEnd)) { return 1; }
        std::printf("[store-template-check] chapter-bounds moving-region reversal modified-resource failures=0\n");
    }
    const MenuTestClick openClick{closedButton.x + closedButton.width / 2, closedButton.y + closedButton.height / 2};
    const MenuTestClick closeClick{openButton.x + openButton.width / 2, openButton.y + openButton.height / 2};
    const unsigned travel = slideEnd - closedEnd;
    const float optionX = openPanel.x + openPanel.width / 2;
    const float optionY = openPanel.y + optionLabel.height / 2;
    const float rowPitch = optionLabel.height * kSortRowSpacing;
    MenuState closed, halfway, selected, closing, closedAgain;
    closed.page = halfway.page = selected.page = closing.page = closedAgain.page = 2;
    const std::filesystem::path profilePath = TestOutput::Path("store-template-check.dat");
    const std::vector<MenuTestClick> idle = {{-100, -100}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        closed, profilePath, TestOutput::Path("store-template-closed.png"), &idle) != -2) { return 1; }
    const std::vector<MenuTestClick> halfwayClicks = {openClick, {-100, -100, travel / 2}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        halfway, profilePath, TestOutput::Path("store-template-opening.png"), &halfwayClicks) != -2 ||
        halfway.store.shopFilterTime != closedEnd + travel / 2) { return 1; }
    // Click a row's final position before it arrives: it must not select there.
    const std::vector<MenuTestClick> selectClicks = {openClick, {optionX, optionY + 2 * rowPitch},
        {optionX, optionY + 2 * rowPitch, travel}, {optionX, optionY + 3 * rowPitch}, {-100, -100}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        selected, profilePath, TestOutput::Path("store-template-open.png"), &selectClicks) != -2 ||
        selected.store.shopFilter != 3 || selected.store.shopFilterTime != slideEnd) { return 1; }
    const std::vector<MenuTestClick> closingClicks = {openClick, {-100, -100, travel}, closeClick, {-100, -100, travel / 2}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        closing, profilePath, TestOutput::Path("store-template-closing.png"), &closingClicks) != -2 ||
        closing.store.shopFilterOpen || closing.store.shopFilterTime != slideEnd - travel / 2) { return 1; }
    const std::vector<MenuTestClick> closeClicks = {openClick, {-100, -100, travel}, closeClick, {-100, -100, travel}};
    if (ShowGameMenu(toc, tables, profile, progress, refinement, store, weapons, armor,
        closedAgain, profilePath, TestOutput::Path("store-template-closed-again.png"), &closeClicks) != -2 ||
        closedAgain.store.shopFilterOpen || closedAgain.store.shopFilterTime != slideStart) { return 1; }
    if (profile.coins != 0 || profile.warbucks != 0 || profile.inventory.size() != 4) { return 1; }
    std::printf("[store-template-check] screenshots=5 multiselect moving-hitbox close no-purchase failures=0\n");
    return CheckStoreCards(toc, tables, profile, progress, refinement, store, weapons, armor);
}

/** Real BIG cards, native save copies and actual expanded-card input. */
int RunDualWeaponCheck(const std::string &bigDirectory) {
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    std::vector<StoreEntry> store;
    std::vector<WeaponEntry> weapons;
    std::vector<ArmorEntry> armor;
    if (!LoadStoreCatalog(toc, tables, store) || !LoadWeaponCatalog(toc, tables, weapons) ||
        !LoadArmorCatalog(toc, tables, armor)) { return 1; }
    const auto path = std::filesystem::path(TestOutput::Path("dual-weapon-check")) / std::to_string(GetTickCount64());
    CProfileManager profile;
    if (!LoadNativeProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
    std::vector<unsigned> entries;
    for (unsigned index = 0; index < store.size() && entries.size() < 3; ++index) {
        const auto &item = store[index].data;
        if (item.objects.size() != 1 || item.objects[0].type != 6 || item.singlePurchase != 0) { continue; }
        const auto &ref = item.objects[0].object;
        const auto *weapon = FindWeaponEntry(weapons, ref);
        if (weapon == nullptr || weapon->visualOnly) { continue; }
        bool duplicate = false;
        for (unsigned existing : entries) { if (SameObject(store[existing].data.objects[0].object, ref)) { duplicate = true; } }
        if (duplicate) { continue; }
        entries.push_back(index);
        profile.Grant(6, ref);
        // Gold weapons still expose EQUIP: exercise the duplicate-write guard.
        profile.AddWeaponExperience(ref, weapon->data.GetMasteryThreshold(2), weapon->data.GetMasteryThreshold(2));
    }
    if (entries.size() != 3) { return 1; }
    const GameObjectRef first = store[entries[0]].data.objects[0].object;
    const GameObjectRef second = store[entries[1]].data.objects[0].object;
    const GameObjectRef third = store[entries[2]].data.objects[0].object;
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    const unsigned card = view.movies.Ordinal("GLU_MOVIE_SHOP_BOX");
    const CMovie *movie = view.movies.GetMovie(card);
    unsigned start = 0, end = 0;
    MovieRegion content, body, actions, label;
    const auto *equip = OriginalMenuData("MDS_BUTTON_STORE_ITEMS", kEquipButtonEntry);
    if (movie == nullptr || equip == nullptr || !movie->GetChapterRange(1, start, end) ||
        !view.movies.Region(view.movies.Ordinal("GLU_MOVIE_STORE_MENU"), 0, 0, content) ||
        !view.movies.Region(card, 0, end, body) ||
        !view.movies.Region(view.movies.Ordinal(equip->movies[0]), 1, 0, label)) { return 1; }
    const StoreCardFace face{content.x + content.width / 2 - static_cast<int>(content.width) / 16 - body.width / 2,
        content.y + content.height / 2 - body.height / 2, 1, end};
    if (!CardRegion(view, card, kCardActionRegion, face, actions)) { return 1; }
    unsigned failures = 0;
    for (unsigned active : {0u, 1u}) {
        profile.configuration.guns = {first, second};
        profile.activeWeaponSlot = active;
        unsigned stamps = 0;
        for (const auto &gun : profile.configuration.guns) { if (IsStoreObjectEquipped(profile, active, gun)) { ++stamps; } }
        if (stamps != 2) { ++failures; }
        for (unsigned step = 0; step < 2; ++step) {
            MenuState state;
            state.page = 2;
            state.store.shopGunSlot = active;
            state.slot = active;
            state.store.shopDetailOpen = true;
            state.store.shopFocusAmount = 1;
            state.store.shopDetailTime = end;
            state.selectedItem = entries[1 - active];
            if (step == 1) { state.selectedItem = entries[2]; }
            view.Begin(2);
            view.SetTestClick({actions.x + actions.width - label.width / 2, actions.y + label.height / 2});
            if (!DrawStore(view, toc, tables, profile, 200, store, weapons, armor, state, path)) { return 1; }
            bool correct = SameObject(profile.configuration.guns[0], first) && SameObject(profile.configuration.guns[1], second);
            if (step == 1) {
                correct = SameObject(profile.configuration.guns[active], third);
                if (active == 0) { correct = correct && SameObject(profile.configuration.guns[1], second); }
                else { correct = correct && SameObject(profile.configuration.guns[0], first); }
            }
            const bool distinct = !SameObject(profile.configuration.guns[0], profile.configuration.guns[1]);
            if (!correct || !distinct) { ++failures; }
            std::printf("[dual-weapon-check] active=%u stamps=%u step=%u correct=%d distinct=%d\n", active, stamps, step, correct, distinct);
            if (step == 0) {
                state.store.shopDetailOpen = false;
                view.Begin(2);
                if (!DrawStore(view, toc, tables, profile, 200, store, weapons, armor, state, path) ||
                    !GB_SAVE_FRAME(view.window, TestOutput::Path("dual-weapon-slot-") + std::to_string(active) + ".png")) { return 1; }
            }
        }
        CProfileManager restored;
        if (!LoadNativeProfile(toc, tables, restored, path, path / "absent-source")) { return 1; }
        for (unsigned slot = 0; slot < 2; ++slot) {
            if (!SameObject(profile.configuration.guns[slot], restored.configuration.guns[slot])) { ++failures; }
        }
        if (restored.activeWeaponSlot != active) { ++failures; }
    }
    std::printf("[dual-weapon-check] failures=%u\n", failures);
    return failures != 0;
}
