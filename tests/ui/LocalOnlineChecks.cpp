/** Local service callbacks are exercised with actual BIG movies and save copies. */
#include "ui/MenuChecks.h"
#include "TestOutput.h"

int CheckBroOps(GameMenu &view, CResTOCManager &toc, PackTables &tables, const CProfileManager &source);

namespace {
struct RestoreConnection {
    bool previous = GameHostSettings().isConnected;
    ~RestoreConnection() { GameHostSettings().isConnected = previous; }
};

// Capture authored logical regions at the actual drawable resolution (including DPI).
std::vector<std::uint8_t> SocialPixels(GameMenu &view, const MovieRegion &region) {
    int width = 0, height = 0;
    view.window.GetDrawableSize(width, height);
    const int x = static_cast<int>(region.x * width / 1024);
    const int y = static_cast<int>((768 - region.y - region.height) * height / 768);
    const int cropWidth = static_cast<int>(region.width * width / 1024);
    const int cropHeight = static_cast<int>(region.height * height / 768);
    std::vector<std::uint8_t> pixels(cropWidth * cropHeight * 4);
    glReadPixels(x, y, cropWidth, cropHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return pixels;
}
}

int RunLocalOnlineCheck(const std::string &bigDirectory) {
    RestoreConnection restore;
    GameHostSettings().isConnected = true;
    CResTOCManager toc;
    if (!toc.Init(bigDirectory, "xga") || !toc.Bind()) { return 1; }
    PackTables tables(toc);
    CRefinementManager::Template refinement;
    std::vector<StoreEntry> store;
    if (!LoadRefinementTemplate(toc, tables, refinement) || !LoadStoreCatalog(toc, tables, store)) { return 1; }
    CProfileManager profile;
    profile.Reset(toc.GetPack(toc.GetCorePackIndex())->GetPackHash(), refinement);
    const auto path = std::filesystem::path(TestOutput::Path("local-online")) / std::to_string(GetTickCount64());
    if (!LoadNativeProfile(toc, tables, profile, path, TestOutput::Fixtures())) { return 1; }
    CPlayerProgress progress;
    progress.Bind(profile.nativeArchive->progression);
    progress.SetExperience(profile.experience);
    GameMenu view;
    if (!view.Open(toc, tables)) { return 1; }
    view.scripted = true;
    view.animateNavigation = false;
    if (CheckBroOps(view, toc, tables, profile) != 0) { std::printf("[bro-ops-check] FAILED\n"); return 1; }
    CChallengeManager catalog;
    if (!catalog.Load(toc, tables) || catalog.templates.size() != 238) { return 1; }
    // Regression: known BIG content must survive parser validation; removing
    // the final field must fail, rather than silently create an empty menu.
    const auto &sample = catalog.templates.front();
    std::vector<std::uint8_t> bytes;
    if (!tables.ReadSectionResource(sample.reference.packHash, static_cast<GameSection>(27), sample.reference.localIndex, bytes)) { return 1; }
    bytes.pop_back();
    CArrayInputStream truncated(bytes);
    CChallengeManager::Template invalid;
    if (invalid.Init(truncated)) { return 1; }
    if (catalog.GenerateChallengeList(1) == catalog.GenerateChallengeList(2)) { return 1; }
    // Bind an existing cycle under a different host date. Its progress must
    // stay attached to that cycle, and merely viewing must not write the save.
    auto savedProfile = profile;
    auto &savedPayload = savedProfile.nativeArchive->records[17].payload;
    savedPayload[0] = 1;
    savedPayload[1] = savedPayload[2] = savedPayload[3] = 0;
    if (savedPayload[6] == 0) { return 1; }
    savedPayload[7] = 37;
    if (!catalog.Bind(toc, tables, savedProfile, 1000000000) || catalog.cycleDay != 1 ||
        catalog.current.front().progress != 37 || catalog.current.front().templateIndex != catalog.GenerateChallengeList(1).front()) { return 1; }
    const auto untouchedChallenges = profile.nativeArchive->records[17].payload;
    for (unsigned page : {4u, 5u}) {
        MenuState state;
        state.page = page;
        view.Begin(page);
        if (!DrawOriginalSocialMenu(view, state, profile, false) || !state.social.onlinePage) { return 1; }
        if (state.social.renderedEntries == 0) {
            std::printf("[social-content-check] page=%u local content missing\n", page);
            return 1;
        }
        if (state.social.challenges.templates.size() != 238 || state.social.challenges.current.empty() ||
            state.social.brotherName.empty() || view.Header(profile, progress, page) == -3 || !GB_SAVE_FRAME(view.window, (path / ("social-initial-" + std::to_string(page) + ".png")).string())) { return 1; }
        if (page == 4) {
            MovieRegion list, row, card;
            const auto listMovie = view.movies.Ordinal("GLU_MOVIE_BROTHER_MENU_SCROLL_3_OPTION");
            const auto cardMovie = view.movies.Ordinal("GLU_MOVIE_BROTHER_BOX");
            unsigned start = 0, end = 0;
            if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_BROBUFF_MENU"), 4, state.social.socialTime, list) ||
                !view.movies.GetMovie(listMovie)->GetChapterRange(1, start, end) ||
                !view.movies.Region(listMovie, 2, start, row) ||
                !view.movies.GetMovie(cardMovie)->GetChapterRange(0, start, end) ||
                !view.movies.Region(cardMovie, 0, end, card)) { return 1; }
            row.x += list.x - 512;
            row.y += list.y - 384;
            card.x += row.x - 512;
            card.y += row.y - 384;
            view.Begin(page);
            view.SetTestClick({card.x + card.width / 2, card.y + card.height / 2});
            if (!DrawOriginalSocialMenu(view, state, profile, false) || state.social.selectedLocalFriend != 1) { return 1; }
            view.Begin(page);
            if (!DrawOriginalSocialMenu(view, state, profile, false) ||
                view.Header(profile, progress, page) == -3 ||
                !GB_SAVE_FRAME(view.window, (path / "local-bot-selected.png").string())) { return 1; }
        }
        // Isolate the warning tape between the title and tabs, away from the
        // independently animated player model, header and selected challenge.
        MovieRegion titleArea, tabsArea;
        const unsigned socialMovie = view.movies.Ordinal("GLU_MOVIE_BROBUFF_MENU");
        if (!view.movies.Region(socialMovie, 2, state.social.socialTime, titleArea) ||
            !view.movies.Region(socialMovie, 1, state.social.socialTime, tabsArea)) { return 1; }
        MovieRegion tape = titleArea;
        tape.y += titleArea.height;
        tape.height = tabsArea.y - tape.y;
        const auto idleBefore = SocialPixels(view, tape);
        view.clock += 125;
        view.Begin(page);
        if (!DrawOriginalSocialMenu(view, state, profile, false)) { return 1; }
        if (idleBefore == SocialPixels(view, tape)) {
            std::printf("[social-animation-check] FAILED: warning tape is frozen page=%u\n", page);
            return 1;
        }
        if (page == 4) {
            state.social.socialTab = 1;
            view.Begin(page);
            if (!DrawOriginalSocialMenu(view, state, profile, false) || state.social.renderedEntries < 3 || view.Header(profile, progress, page) == -3 ||
                !GB_SAVE_FRAME(view.window, (path / "social-buffs.png").string())) { return 1; }
            MovieRegion listOrigin;
            if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_BROBUFF_MENU"), 4, state.social.socialTime, listOrigin)) { return 1; }
            const unsigned list = view.movies.Ordinal("GLU_MOVIE_BROTHER_MENU_SCROLL_3_OPTION");
            unsigned start = 0, end = 0;
            if (!view.movies.GetMovie(list)->GetChapterRange(1, start, end)) { return 1; }
            const auto slots = view.movies.Regions(list, start,
                listOrigin.x, listOrigin.y);
            if (slots.size() < 3) { return 1; }
            view.Begin(page);
            view.clock += 16;
            view.SetTestClick({listOrigin.x + 10, listOrigin.y + 10});
            view.pointerPressed = view.pointerHeld = true;
            view.dragY = -(slots[2].y - slots[1].y) * 7;
            if (!DrawOriginalSocialMenu(view, state, profile, false) || state.social.scrollPosition <= 0 ||
                state.social.renderedEntries < 3 || view.Header(profile, progress, page) == -3 ||
                !GB_SAVE_FRAME(view.window, (path / "social-buffs-last.png").string())) { return 1; }
            view.pointerPressed = view.pointerHeld = false;
            view.dragY = 0;
            view.ExchangeClick(false);
            state.social.socialTab = 0;
        }
        if (page == 5) {
            MovieRegion listOrigin;
            if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_BROBUFF_MENU"), 4, state.social.socialTime, listOrigin)) { return 1; }
            const unsigned list = view.movies.Ordinal("GLU_MOVIE_BROTHER_MENU_SCROLL");
            unsigned start = 0, end = 0;
            if (!view.movies.GetMovie(list)->GetChapterRange(1, start, end)) { return 1; }
            const auto slots = view.movies.Regions(list, start,
                listOrigin.x, listOrigin.y);
            if (slots.size() < 3) { return 1; }
            view.Begin(page);
            view.SetTestClick({slots[2].x + slots[2].width / 2, slots[2].y + slots[2].height / 2});
            if (!DrawOriginalSocialMenu(view, state, profile, false) || state.social.selectedChallenge != 1) { return 1; }
            const auto selectedBefore = SocialPixels(view, slots[2]);
            const auto otherBefore = SocialPixels(view, slots[3]);
            if (!state.social.sidebarReverse || state.social.sidebarChallenge != 0) {
                std::printf("[social-sidebar-check] FAILED: old details must leave before rebinding\n");
                return 1;
            }
            view.clock += 250;
            view.Begin(page);
            if (!DrawOriginalSocialMenu(view, state, profile, false)) { return 1; }
            if (selectedBefore == SocialPixels(view, slots[2]) || otherBefore != SocialPixels(view, slots[3])) {
                std::printf("[social-animation-check] FAILED: selected card must animate independently\n");
                return 1;
            }
            if (state.social.sidebarReverse || state.social.sidebarChallenge != 1) { return 1; }
            view.clock += 200;
            view.Begin(page);
            if (!DrawOriginalSocialMenu(view, state, profile, false) || view.Header(profile, progress, page) == -3 ||
                !GB_SAVE_FRAME(view.window, (path / "social-challenge-selected.png").string())) { return 1; }
            // Four real tasks still support dragging at either bound, then return.
            const float stride = slots[2].y - slots[1].y;
            for (int direction : {-1, 1}) {
                view.clock += 16;
                view.Begin(page);
                view.SetTestClick({slots[2].x + slots[2].width / 2, slots[2].y + slots[2].height / 2});
                view.pointerPressed = view.pointerHeld = true;
                view.dragY = direction * stride / 2;
                if (!DrawOriginalSocialMenu(view, state, profile, false) ||
                    state.social.scrollPosition * direction >= 0 || state.social.selectedChallenge != 1 ||
                    state.social.renderedEntries != 4) {
                    std::printf("[social-scroll-check] FAILED: four tasks must drag without selecting\n");
                    return 1;
                }
                if (!GB_SAVE_FRAME(view.window, (path / ("social-drag-" + std::to_string(direction) + ".png")).string())) { return 1; }
                // A partly clipped first card must not intercept input above the viewport.
                view.Begin(page);
                view.SetTestClick({listOrigin.x + 20, listOrigin.y - 2});
                if (!DrawOriginalSocialMenu(view, state, profile, false) || state.social.selectedChallenge != 1) { return 1; }
                for (unsigned frame = 0; frame < 120 && state.social.scrollPosition != 0; ++frame) {
                    view.clock += 16;
                    view.Begin(page);
                    if (!DrawOriginalSocialMenu(view, state, profile, false)) { return 1; }
                }
                if (state.social.scrollPosition != 0) {
                    std::printf("[social-scroll-check] FAILED: list did not return to its bound\n");
                    return 1;
                }
            }
            // Compare both sets of native checkboxes across incomplete/complete
            // states. Changing status text or the meter cannot satisfy this check.
            MovieRegion sidebar;
            if (!view.movies.Region(socialMovie, 3, state.social.socialTime, sidebar)) { return 1; }
            const unsigned details = view.movies.Ordinal("GLU_MOVIE_BRO_OPS_DETAILS");
            unsigned detailsStart = 0, detailsEnd = 0;
            if (!view.movies.GetMovie(details)->GetChapterRange(1, detailsStart, detailsEnd)) { return 1; }
            const auto rewardRegions = view.movies.Regions(details, detailsEnd, sidebar.x, sidebar.y);
            MovieRegion checkBounds, personBounds;
            // Obtain this card's actual origin, not the default movie origin.
            const auto cardRegions = view.movies.Regions(view.movies.Ordinal("GLU_MOVIE_BRO_OP_BOX"), 0, slots[2].x, slots[2].y);
            if (cardRegions.size() < 4 || rewardRegions.size() < 6) { return 1; }
            const auto &prizeRegion = cardRegions[3];
            const auto *checkEntry = OriginalMenuData("MDS_ICON_CHALLENGES", 1);
            const auto *personEntry = OriginalMenuData("MDS_ICON_CHALLENGES", 0);
            if (!checkEntry || !personEntry ||
                !view.movies.SpriteBounds(checkEntry->sprites[0] >> 16, checkEntry->sprites[0] & 255, checkBounds) ||
                !view.movies.SpriteBounds(personEntry->sprites[0] >> 16, personEntry->sprites[0] & 255, personBounds)) { return 1; }
            std::vector<MovieRegion> markers;
            for (unsigned tier = 0; tier < 3; ++tier) {
                MovieRegion marker = checkBounds;
                marker.x = prizeRegion.x + (tier + 1) * (prizeRegion.width / 3) - checkBounds.width * 1.5f;
                marker.y = prizeRegion.y + prizeRegion.height - checkBounds.height;
                markers.push_back(marker);
                marker.x = rewardRegions[3 + tier].x;
                marker.y = rewardRegions[3 + tier].y + (rewardRegions[3 + tier].height - checkBounds.height) / 2;
                markers.push_back(marker);
            }
            MovieRegion recruits = personBounds;
            recruits.x = rewardRegions[5].x + 4 * checkBounds.width;
            recruits.y = rewardRegions[5].y + personBounds.height;
            recruits.width *= 2;
            markers.push_back(recruits);
            auto &challenge = state.social.challenges.current[1];
            const auto previousProgress = challenge.progress;
            const auto previousFriends = challenge.completedFriends;
            const auto previousAchieved = challenge.achieved;
            challenge.progress = 0;
            challenge.completedFriends = 0;
            view.Begin(page);
            if (!DrawOriginalSocialMenu(view, state, profile, false)) { return 1; }
            std::vector<std::vector<std::uint8_t>> incomplete;
            for (const auto &marker : markers) { incomplete.push_back(SocialPixels(view, marker)); }
            challenge.progress = 100;
            challenge.achieved = challenge.target;
            challenge.completedFriends = state.social.challenges.templates[challenge.templateIndex].participationRequired[2];
            view.Begin(page);
            if (!DrawOriginalSocialMenu(view, state, profile, false)) { return 1; }
            for (unsigned index = 0; index < markers.size(); ++index) {
                if (incomplete[index] == SocialPixels(view, markers[index])) {
                    std::printf("[social-reward-check] FAILED: marker unchanged index=%u\n", index);
                    return 1;
                }
            }
            if (view.Header(profile, progress, page) == -3 ||
                !GB_SAVE_FRAME(view.window, (path / "social-rewards-completed.png").string())) { return 1; }
            challenge.progress = previousProgress;
            challenge.completedFriends = previousFriends;
            challenge.achieved = previousAchieved;
            std::printf("[social-presentation-check] tape=2 selection=1 drag=2 return=2 reward-markers=6 recruits=2\n");
        }
        MovieRegion tabs;
        if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_BROBUFF_MENU"), 1, state.social.socialTime, tabs)) { return 1; }
        const char *table = "MDS_BUTTON_FRIENDS_CATEGORIES";
        if (page == 5) { table = "MDS_BUTTON_CHALLENGE_CATEGORIES"; }
        const auto *button = OriginalMenuData(table, 2);
        MovieRegion bounds;
        if (button == nullptr || !view.movies.Region(view.movies.Ordinal(button->movies[0]), 1, 0, bounds)) { return 1; }
        view.Begin(page);
        view.SetTestClick({tabs.x + tabs.width - bounds.width / 2, tabs.y + bounds.height / 2});
        if (!DrawOriginalSocialMenu(view, state, profile, false) || state.social.socialTab != 2) { return 1; }
        view.Begin(page);
        if (!DrawOriginalSocialMenu(view, state, profile, false) || view.Header(profile, progress, page) == -3 ||
            !GB_SAVE_FRAME(view.window, (path / ("social-" + std::to_string(page) + ".png")).string())) { return 1; }
        GameHostSettings().isConnected = false;
        view.Begin(page);
        if (!DrawOriginalSocialMenu(view, state, profile, false) || state.social.onlinePage) { return 1; }
        GameHostSettings().isConnected = true;
    }
    if (profile.nativeArchive->records[17].payload != untouchedChallenges) { return 1; }

    MenuState match;
    match.gameMode = 1;
    match.planet = 4;
    if (!BeginLocalMatch(match) || !match.online.IsMatching()) { return 1; }
    match = MenuState{};
    match.gameMode = 1;
    match.planet = 0;
    if (!BeginLocalMatch(match) || !match.online.IsMatching()) { return 1; }
    for (unsigned frame = 0; frame < 4; ++frame) {
        view.clock += 1000;
        view.Begin(0);
        if (!DrawStorePrompt(view, match)) { return 1; }
    }
    if (!match.storePopup.IsReady() || !match.online.IsMatching() ||
        !GB_SAVE_FRAME(view.window, (path / "match-waiting.png").string())) { return 1; }
    MovieRegion cancelRegion;
    if (!view.movies.Region(view.movies.Ordinal("GLU_MOVIE_POPUP"), 2, match.storePopup.MovieTime(), cancelRegion)) { return 1; }
    view.Begin(0);
    view.SetTestClick({cancelRegion.x + cancelRegion.width / 2, cancelRegion.y + cancelRegion.height / 2});
    if (!DrawStorePrompt(view, match) || match.online.IsMatching()) { return 1; }
    for (unsigned frame = 0; frame < 3; ++frame) {
        view.clock += 1000;
        view.Begin(0);
        if (!DrawStorePrompt(view, match)) { return 1; }
        UpdateLocalConnection(match);
    }
    if (match.matchingPrompt) { return 1; }
    if (!BeginLocalMatch(match)) { return 1; }
    bool ready = false;
    for (unsigned frame = 0; frame < 7 && !ready; ++frame) {
        view.clock += 1000;
        view.Begin(0);
        if (!DrawStorePrompt(view, match)) { return 1; }
        ready = TakeLocalMatch(match, view.clock);
    }
    if (!ready || match.online.IsMatching() || TakeLocalMatch(match, view.clock + 10000)) { return 1; }
    match.gameMode = 2;
    if (!BeginLocalMatch(match)) { return 1; }
    GameHostSettings().isConnected = false;
    UpdateLocalConnection(match);
    if (match.online.IsMatching() || match.matchingPrompt || match.gameMode != 0) { return 1; }
    GameHostSettings().isConnected = true;

    int product = -1;
    for (unsigned index = 0; index < store.size(); ++index) {
        if (store[index].data.type == 15 && store[index].data.value32 == 1 && !store[index].productId.empty()) {
            product = static_cast<int>(index);
            break;
        }
    }
    if (product < 0) { return 1; }
    const auto startingBalance = profile.warbucks;
    const auto amount = store[product].data.rarePrice;
    MenuState purchase;
    purchase.BeginOfflineIAP(product, 0, store[product].productId);
    if (!purchase.currencyPending || !purchase.currencySimulated) { return 1; }
    if (!CompleteOfflineIAP(1000, purchase, profile, store, path) || profile.warbucks != startingBalance ||
        purchase.online.GetPurchaseState() != LocalOnlineServices::PurchaseState::Verifying) { return 1; }
    // Reordering the visible catalog must not redirect a completed transaction.
    std::vector<StoreEntry> reordered = store;
    std::reverse(reordered.begin(), reordered.end());
    if (!CompleteOfflineIAP(4000, purchase, profile, reordered, path) || purchase.currencyPending ||
        profile.warbucks != startingBalance + amount) { return 1; }
    if (!CompleteOfflineIAP(8000, purchase, profile, reordered, path) ||
        !profile.LoadFromDisk(path) || profile.warbucks != startingBalance + amount) { return 1; }
    purchase.BeginOfflineIAP(product, 9000, store[product].productId);
    GameHostSettings().isConnected = false;
    if (!CompleteOfflineIAP(10000, purchase, profile, store, path) || purchase.currencyPending ||
        profile.warbucks != startingBalance + amount) { return 1; }
    GameHostSettings().isConnected = true;
    purchase.BeginOfflineIAP(product, 11000, "unknown-test-product");
    if (!CompleteOfflineIAP(15000, purchase, profile, store, path) || purchase.currencyPending ||
        profile.warbucks != startingBalance + amount) { return 1; }
    std::printf("[local-online-check] social-tabs=2 mode-selection match-cancel disconnect product-id single-delivery reload failures=0\n");
    return 0;
}

