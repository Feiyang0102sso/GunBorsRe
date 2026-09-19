/** Native category/ownership predicates exercised against real BIG records. */
#include "gun_bros_re/ui/content/CStoreAggregator.h"
#include "gun_bros_re/ui/system/CMenuSystem.h"
#include "ui/MenuChecks.h"
using namespace MenuDetail;

int CheckStoreFiltering(CResTOCManager &toc, const CRefinementManager::Template &refinement,
    const std::vector<ZStoreEntry> &store, const std::vector<ZWeaponEntry> &weapons,
    const std::vector<ZArmorEntry> &armor) {
    const int pack = toc.GetPackIndexFromName("pack3");
    if (pack < 0) { return 1; }
    const unsigned coreHash = toc.GetPack(toc.GetCorePackIndex())->GetPackHash();
    const unsigned packHash = toc.GetPack(pack)->GetPackHash();
    std::array<unsigned, 4> records{};
    records.fill(static_cast<unsigned>(store.size()));
    // Source byte audit: core STORE0 and pack3 STORE0/1/4, not synthetic items.
    for (unsigned index = 0; index < store.size(); ++index) {
        const auto &ref = store[index].ref;
        if (ref.packHash == coreHash && ref.localIndex == 0) { records[0] = index; }
        if (ref.packHash != packHash) { continue; }
        if (ref.localIndex == 0) { records[1] = index; }
        if (ref.localIndex == 1) { records[2] = index; }
        if (ref.localIndex == 4) { records[3] = index; }
    }
    for (unsigned index : records) { if (index >= store.size()) { return 1; } }
    CProfileManager profile;
    profile.Reset(coreHash, refinement);
    for (unsigned index : {records[1], records[3]}) {
        const auto &object = store[index].data.objects.front();
        profile.Grant(object.type, object.object);
    }
    struct Selection {
        unsigned filter;
        bool all;
        std::array<bool, 4> included;
    };
    const Selection selections[] = {
        {0, true, {true, true, true, true}},
        {1, false, {false, false, true, false}},
        {1 | kOwnedFilterBit, false, {true, true, true, false}},
        {kOwnedFilterBit, false, {true, true, false, true}},
        {1u << 19, false, {true, false, false, false}},
        {0, false, {false, false, false, false}},
    };
    for (const auto &selection : selections) {
        std::vector<unsigned> items, slots;
        CStoreAggregator::InitFilteredList(store, profile, weapons, armor, 0, 0,
            selection.filter, selection.all, 0, items, slots);
        for (unsigned record = 0; record < records.size(); ++record) {
            const bool included = std::find(items.begin(), items.end(), records[record]) != items.end();
            if (included != selection.included[record]) {
                std::printf("[store-filter-check] filter=%x all=%u record=%u included=%u expected=%u\n",
                    selection.filter, selection.all, record, included, selection.included[record]);
                return 1;
            }
        }
        if (!selection.all && selection.filter == 0 && !items.empty()) { return 1; }
    }
    CMenuSystem state;
    state.stack.page = 2;
    state.Navigate(6);
    if (state.stack.page != 2 || !state.stack.HasPending()) { return 1; }
    if (!state.UpdateNavigation() || state.stack.page != 6 || state.UpdateNavigation()) { return 1; }
    state.Back();
    if (state.stack.page != 6 || !state.UpdateNavigation() || state.stack.page != 2) { return 1; }
    state.stack.page = 24;
    state.greeting.greetingBound = true;
    state.Navigate(0, true);
    if (state.UpdateNavigation() || state.stack.page != 24 || !state.greeting.greetingExitRequested) { return 1; }
    state.greeting.greetingBound = false;
    if (!state.UpdateNavigation() || state.stack.page != 0 || !state.stack.history.empty()) { return 1; }
    std::printf("[store-filter-check] original-records=4 selections=6 navigation-pending-busy-once failures=0\n");
    return 0;
}
