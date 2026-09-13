/** Local service callbacks are exercised with actual BIG movies and save copies. */
#include "ui/MenuChecks.h"
#include "TestOutput.h"

int CheckBroOps(GameMenu &view, CResTOCManager &toc, PackTables &tables, const CProfileManager &source);

namespace {
struct RestoreConnection {
    bool previous = GameHostSettings().isConnected;
    ~RestoreConnection() { GameHostSettings().isConnected = previous; }
};
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
            view.Begin(page);
            if (!DrawOriginalSocialMenu(view, state, profile, false) || view.Header(profile, progress, page) == -3 ||
                !GB_SAVE_FRAME(view.window, (path / "social-challenge-selected.png").string())) { return 1; }
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

